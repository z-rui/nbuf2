#include "nbuf.h"
#include "nbuf_schema.nb.h"
#include "lex.h"
#include "util.h"
#include "libnbuf.h"
#include "libnbufc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local limits */
#define MAX_DEPTH 500
#define MAX_IMPORTS 32767
#define MAX_UNRESOLVED 32767
#define MAX_BUFFER 4

#define UNRESOLVED_IMPORT_ID 65535

/* Schema parser: text -> nbuf_schema_set */

/* Poor man's std::vector */
#define LEN(typ, f) ((f).len / sizeof (typ))
#define ADD(typ, f) ((typ *) nbuf_alloc(&f, sizeof (typ)))
#define POP(typ, f) ((f).len -= sizeof (typ))
#define GET(typ, f, i) ((typ *) (f).base + i)
#define FOR_EACH(typ, var, n, f) \
	for (n = LEN(typ, f), var = (typ *) (f).base; n--; var++)
#define CLR(typ, f, dtor) do { \
	typ *_p; \
	size_t _n; \
	FOR_EACH(typ, _p, _n, f) dtor(_p); \
	nbuf_clear(&f); \
} while (0)

struct FileState {
	const char *filename;
	bool is_open;
	struct nbuf_schema_set *ss;
};

static void dtor_FileState(struct FileState *fs)
{
	assert(!fs->is_open);
	if (fs->ss) {
		nbuf_clear(&fs->ss->buf);
		free(fs->ss);
	}
}

static bool
samefile(struct FileState *fs, FILE *f, const char *path)
{
	/* Without OS-specific features, we can only test for path equality.
	 */
	return strcmp(fs->filename, path) == 0;
}

struct ctx {
	size_t depth;
	Token token;
	struct nbuf_buf *file_states;  // struct FileState
	const char *const *search_path;

	// parsing state of current file.
	struct nbuf_buf scratch_buf;
	struct nbuf_buf typenames;
	struct nbuf_buf bufs[MAX_BUFFER];
	struct nbuf_buf *buf;
};

static struct FileState *
new_FileState(struct ctx *ctx, const char *path)
{
	struct FileState *fs;

	fs = ADD(struct FileState, *ctx->file_states);
	fs->filename = path;
	fs->is_open = true;
	fs->ss = NULL;
	return fs;
}

#define NEXT_BUF (assert(ctx->buf < ctx->bufs + MAX_BUFFER), ctx->buf++)

#define NEXT ctx->token = nbuf_lex(l)
#define IS_C(X) (ctx->token == X)
#define IS(X) IS_C(Token_##X)
#define IS_ID(X) (IS(ID) && TOKENLEN(l) == strlen(X) && strncmp(TOKEN(l), X, TOKENLEN(l)) == 0)
#define EXPECT_C(X) if (ctx->token != (Token) X) { nbuf_lexsyntax(l, (Token) X, ctx->token); goto err; }
#define EXPECT(X) if (ctx->token != Token_##X) { nbuf_lexsyntax(l, Token_##X, ctx->token); goto err; }

static struct nbuf_schema_set *
parse_file(struct ctx *ctx, const char *filename);

// strcat ::= { STR }
static char *
parse_strcat(struct ctx *ctx, lexState *l)
{
	struct nbuf_buf *buf = &ctx->scratch_buf;

	EXPECT(STR);
	buf->len = 0;
	for (;;) {
		const char *s = TOKEN(l);
		size_t len = TOKENLEN(l);

		assert(*s == '"');
		assert(len >= 2);
		/* remove "" */
		if (!nbuf_unescape(buf, s + 1, len - 2))
			goto err;
		NEXT;
		if (!IS(STR))
			break;
		buf->len--;  /* remove '\0' */
	}
	return buf->base;
err:
	return NULL;
}

static char *
parse_fqn(struct ctx *ctx, lexState *l)
{
	size_t len = 0;
	struct nbuf_buf *buf = &ctx->scratch_buf;

	buf->len = 0;
	for (;;) {
		char *p;
		size_t this_len;

		EXPECT(ID);
		this_len = TOKENLEN(l);
		/* +1 for either '.' or '\0' */
		if (!(p = nbuf_alloc(buf, this_len + 1)))
			goto err;
		memcpy(p, TOKEN(l), this_len);
		NEXT;
		len += this_len + 1;
		if (IS_C('.')) {
			NEXT;
			p[this_len] = '.';
		} else {
			p[this_len] = '\0';
			break;
		}
	}
	return buf->base;
err:
	return NULL;
}

// package_stmt ::= [ "package" ID { '.' ID } ";" ]
static bool
parse_package(struct ctx *ctx, lexState *l, nbuf_Schema schema)
{
	bool rc = false;
	const char *p;

	if (!IS_ID("package"))
		return true;
	NEXT;
	if (!(p = parse_fqn(ctx, l)))
		return false;
	if (!nbuf_Schema_set_pkg_name(schema, p, -1)) {
		fprintf(stderr, "internal error: cannot set pkg_name\n");
		return false;
	}
	EXPECT_C(';'); NEXT;
	rc = true;
err:
	return rc;
}

// import_stmts := { "import" { STR } ";" }
static bool
parse_import(struct ctx *ctx, lexState *l, struct nbuf_buf *imports)
{
	while (IS_ID("import")) {
		struct nbuf_schema_set **p;
		char *input_filename;

		if (!ctx->search_path) {
			nbuf_lexerror(l, "import is disabled in compiler");
			goto err;
		}
		NEXT;
		input_filename = parse_strcat(ctx, l);
		if (!input_filename)
			goto err;
		/* parse_file will clobber ctx->token,
		 * thus check the semicolon before and NEXT after. */
		EXPECT_C(';');
		if (!(p = ADD(struct nbuf_schema_set *, *imports)))
			goto err;
		if (!(*p = parse_file(ctx, input_filename)))
			goto err;
		NEXT;
	}
	return true;
err:
	return false;
}

static bool resolve_type(struct nbuf_schema_set *ss, const char *name, nbuf_Kind *kind, unsigned *import_id, unsigned *type_id)
{
	size_t len = nbufc_baselen(name);
	size_t i;
	nbuf_Schema schema;
	const char *pkg_name;
	size_t pkg_name_len;
	const char *base_name;

	if (name[len] == '\0') {
		len = 0;
		base_name = name;
	} else {
		assert(name[len] == '.');
		base_name = name + len + 1;
	}
	if (len == 0 && nbuf_lookup_builtin_type(name, kind, type_id)) {
		*import_id = 0;
		return true;
	}
	if (!nbuf_get_Schema(&schema, &ss->buf, 0))
		return false;
	pkg_name = nbuf_Schema_pkg_name(schema, &pkg_name_len);
	if (len == 0 || (len == pkg_name_len && strncmp(pkg_name, name, len) == 0)) {
		if (nbuf_lookup_defined_type(schema, base_name, kind, type_id)) {
			*import_id = 0;
			return true;
		}
	}
	if (len == 0) {
		name = pkg_name;
		len = pkg_name_len;
	}
	for (i = 0; i < ss->nimports; i++) {
		struct nbuf_schema_set *import = ss->imports[i];
		if (!nbuf_get_Schema(&schema, &import->buf, 0))
			return false;
		pkg_name = nbuf_Schema_pkg_name(schema, &pkg_name_len);
		if (len == pkg_name_len && strncmp(pkg_name, name, len) == 0) {
			if (nbuf_lookup_defined_type(schema, base_name, kind, type_id)) {
				*import_id = i + 1;
				return true;
			}
		}
	}
	*import_id = UNRESOLVED_IMPORT_ID;
	return true;  // will resolve later
}


static bool
parse_field_defs(struct ctx *ctx, lexState *l, nbuf_MsgDef mdef)
{
	size_t count = 0;
	nbuf_FieldDef fdef;
	bool rc = false;
	struct nbuf_buf *oldbuf = ctx->buf;

	while (IS(ID)) {
		struct nbuf_obj o;
		char *s, **p;
		unsigned import_id = 0, type_id = 0;
		bool ok;
		nbuf_Kind kind = nbuf_Kind_VOID;

		ok = (count++ == 0) ? 
			(bool) nbuf_alloc_multi_FieldDef(&fdef, NEXT_BUF, 1) :
			(bool) nbuf_alloc(NBUF_OBJ(fdef)->buf, nbuf_obj_size(NBUF_OBJ(fdef)));
		if (!ok)
			goto err;
		// Record the line number, for better error reporting.
		nbuf_FieldDef_set_offset(fdef, l->lineno);
		if (!(s = parse_fqn(ctx, l)))
			goto err;
		// Builtin types are resolved early.
		// Unresolved names and will be resolved in the second pass.
		// TODO: check there is no redefinition.
		if (!nbuf_lookup_builtin_type(s, &kind, &type_id)) {
			type_id = LEN(char *, ctx->typenames);
			if (type_id >= MAX_UNRESOLVED) {
				fprintf(stderr, "error: too many unresolved types\n");
				goto err;
			}
			p = ADD(char *, ctx->typenames);
			if (!p)
				goto err;
			/* steal from scratch buffer */
			*p = s;
			memset(&ctx->scratch_buf, 0, sizeof ctx->scratch_buf);
			import_id = UNRESOLVED_IMPORT_ID;
		}

		nbuf_FieldDef_set_import_id(fdef, import_id);
		nbuf_FieldDef_set_type_id(fdef, type_id);
		if (IS_C('[')) {
			NEXT;
			EXPECT_C(']'); NEXT;
			kind = (nbuf_Kind) (kind | nbuf_Kind_ARR);
		}
		nbuf_FieldDef_set_kind(fdef, kind);
		EXPECT(ID);
		o.buf = ctx->buf;
		if (!nbuf_alloc_str(&o, TOKEN(l), TOKENLEN(l)))
			goto err;
		if (!nbuf_FieldDef_set_raw_name(fdef, &o))
			goto err;
		NEXT;
		EXPECT_C(';'); NEXT;
		nbuf_next(NBUF_OBJ(fdef));
	}
	if (count == 0) {
		nbuf_lexerror(l, "empty message");
		goto err;
	}
	nbuf_advance(NBUF_OBJ(fdef), -count);
	if (!nbuf_resize_arr(NBUF_OBJ(fdef), count) ||
		!nbuf_fix_arr(NBUF_OBJ(fdef), count, ctx->buf)) {
		fprintf(stderr, "internal error: cannot resize MsgDef.fields\n");
		goto err;
	}
	if (!nbuf_MsgDef_set_raw_fields(mdef, NBUF_OBJ(fdef))) {
		fprintf(stderr, "internal error: cannot set MsgDef.fields\n");
		goto err;
	}
	rc = true;
err:
	if (ctx->buf > oldbuf)
		(ctx->buf--)->len = 0;
	assert(ctx->buf == oldbuf);
	return rc;
}

// message_def ::= "message" "{" { field_def } "}" ";"
static bool
parse_message_defs(struct ctx *ctx, lexState *l, nbuf_Schema schema)
{
	size_t count = 0;
	nbuf_MsgDef mdef;
	bool rc = false;
	struct nbuf_buf *oldbuf = ctx->buf;

	while (IS_ID("message")) {
		struct nbuf_obj o;
		bool ok;

		ok = (count++ == 0) ? 
			(bool) nbuf_alloc_multi_MsgDef(&mdef, NBUF_OBJ(schema)->buf, 1) :
			(bool) nbuf_alloc(NBUF_OBJ(mdef)->buf, nbuf_obj_size(NBUF_OBJ(mdef)));
		if (!ok)
			goto err;
		NEXT;
		EXPECT(ID);
		o.buf = ctx->buf;
		if (!nbuf_alloc_str(&o, TOKEN(l), TOKENLEN(l)))
			goto err;
		if (!nbuf_MsgDef_set_raw_name(mdef, &o)) {
			fprintf(stderr, "internal error: cannot set MsgDef.name\n");
			return false;
		}
		NEXT;
		EXPECT_C('{'); NEXT;
		if (!parse_field_defs(ctx, l, mdef))
			return false;
		EXPECT_C('}'); NEXT;
		nbuf_next(NBUF_OBJ(mdef));
	}
	if (count > 0) {
		nbuf_advance(NBUF_OBJ(mdef), -count);
		if (!nbuf_resize_arr(NBUF_OBJ(mdef), count) ||
			!nbuf_fix_arr(NBUF_OBJ(mdef), count, ctx->buf)) {
			fprintf(stderr, "internal error: cannot resize Schema.messages\n");
			goto err;
		}
		if (!nbuf_Schema_set_raw_messages(schema, NBUF_OBJ(mdef))) {
			fprintf(stderr, "internal error: cannot set Schema.messages\n");
			goto err;
		}
	}
	rc = true;
err:
	if (ctx->buf > oldbuf)
		(ctx->buf--)->len = 0;
	assert(ctx->buf == oldbuf);
	return rc;
}

static bool
parse_enum_vals(struct ctx *ctx, lexState *l, nbuf_EnumDef edef, long *value)
{
	size_t count = 0;
	nbuf_EnumVal eval;
	bool rc = false;
	struct nbuf_buf *oldbuf = ctx->buf;

	EXPECT(ID);
	do {
		struct nbuf_obj o;
		bool ok;

		ok = (count++ == 0) ? 
			(bool) nbuf_alloc_multi_EnumVal(&eval, NEXT_BUF, 1) :
			(bool) nbuf_alloc(NBUF_OBJ(eval)->buf, nbuf_obj_size(NBUF_OBJ(eval)));
		if (!ok)
			goto err;
		o.buf = ctx->buf;
		if (!nbuf_alloc_str(&o, TOKEN(l), TOKENLEN(l)))
			goto err;
		if (!nbuf_EnumVal_set_raw_symbol(eval, &o)) {
			fprintf(stderr, "internal error: cannot set EnumVal.symbol\n");
			return false;
		}
		NEXT;
		if (IS_C('=')) {
			NEXT;
			EXPECT(INT);
			*value = strtol(TOKEN(l), NULL, 0);
			NEXT;
		}
		if (*value < INT16_MIN || *value > INT16_MAX) {
			nbuf_lexerror(l, "enum value %ld is outside valid range", *value);
			/* not a fatal error */
		}
		if (!nbuf_EnumVal_set_value(eval, *value)) {
			fprintf(stderr, "internal error: cannot set EnumVal.value\n");
			return false;
		}
		++*value;
		nbuf_next(NBUF_OBJ(eval));
		if (!IS_C(','))
			break;
		NEXT;
	} while (IS(ID));
	assert(count > 0);
	nbuf_advance(NBUF_OBJ(eval), -count);
	if (!nbuf_resize_arr(NBUF_OBJ(eval), count) ||
		!nbuf_fix_arr(NBUF_OBJ(eval), count, ctx->buf)) {
		fprintf(stderr, "internal error: cannot resize EnumDef.values\n");
		goto err;
	}
	if (!nbuf_EnumDef_set_raw_values(edef, NBUF_OBJ(eval))) {
		fprintf(stderr, "internal error: cannot set EnumDef.values\n");
		goto err;
	}
	rc = true;
err:
	if (ctx->buf > oldbuf)
		(ctx->buf--)->len = 0;
	assert(ctx->buf == oldbuf);
	return rc;
}

// enum_def ::= "enum" "{" enum_val { "," enum_val } [ "," ] "}" ";"
// enum_val ::= ID [ "=" INT ]
static bool
parse_enum_defs(struct ctx *ctx, lexState *l, nbuf_Schema schema)
{
	size_t count = 0;
	nbuf_EnumDef edef;
	bool rc = false;
	long value = 0;
	struct nbuf_buf *oldbuf = ctx->buf;

	while (IS_ID("enum")) {
		struct nbuf_obj o;
		bool ok;

		ok = (count++ == 0) ? 
			(bool) nbuf_alloc_multi_EnumDef(&edef, NBUF_OBJ(schema)->buf, 1) :
			(bool) nbuf_alloc(NBUF_OBJ(edef)->buf, nbuf_obj_size(NBUF_OBJ(edef)));
		if (!ok)
			goto err;
		NEXT;
		EXPECT(ID);
		o.buf = ctx->buf;
		if (!nbuf_alloc_str(&o, TOKEN(l), TOKENLEN(l)))
			goto err;
		if (!nbuf_EnumDef_set_raw_name(edef, &o)) {
			fprintf(stderr, "internal error: cannot set MsgDef.name\n");
			goto err;
		}
		NEXT;
		EXPECT_C('{'); NEXT;
		ok = parse_enum_vals(ctx, l, edef, &value);
		if (!ok)
			goto err;
		EXPECT_C('}'); NEXT;
		nbuf_next(NBUF_OBJ(edef));
	}
	if (count > 0) {
		nbuf_advance(NBUF_OBJ(edef), -count);
		if (!nbuf_resize_arr(NBUF_OBJ(edef), count) ||
			!nbuf_fix_arr(NBUF_OBJ(edef), count, ctx->buf)) {
			fprintf(stderr, "internal error: cannot resize Schema.enums\n");
			goto err;
		}
		if (!nbuf_Schema_set_raw_enums(schema, NBUF_OBJ(edef))) {
			fprintf(stderr, "internal error: cannot set Schema.enums\n");
			goto err;
		}
	}
	rc = true;
err:
	if (ctx->buf > oldbuf)
		(ctx->buf--)->len = 0;
	assert(ctx->buf == oldbuf);
	return rc;
}

static bool complete_message_defs(struct ctx *ctx, nbuf_Schema schema)
{
	nbuf_MsgDef mdef;
	size_t n;
	bool rc = false;
	struct nbuf_schema_set *ss =
		(struct nbuf_schema_set *) NBUF_OBJ(schema)->buf;

	n = nbuf_Schema_messages(&mdef, schema, 0);
	for (; n--; nbuf_next(NBUF_OBJ(mdef))) {
		nbuf_FieldDef fdef;
		size_t m;
		unsigned ssize = 0, psize = 0;
		unsigned max_align = 0;

		m = nbuf_MsgDef_fields(&fdef, mdef, 0);
		for (; m--; nbuf_next(NBUF_OBJ(fdef))) {
			nbuf_Kind kind = nbuf_FieldDef_kind(fdef);
			unsigned import_id = nbuf_FieldDef_import_id(fdef);
			unsigned type_id = nbuf_FieldDef_type_id(fdef);

			if (import_id == UNRESOLVED_IMPORT_ID) {
				char *typenam = *GET(char *, ctx->typenames, type_id);
				nbuf_Kind base_kind;

				if (!resolve_type(ss, typenam, &base_kind, &import_id, &type_id))
					goto err;
				if (import_id == UNRESOLVED_IMPORT_ID) {
					unsigned lineno = nbuf_FieldDef_offset(fdef);

					fprintf(stderr, "%s:%u: cannot resolve typename '%s'\n",
						nbuf_Schema_src_name(schema, NULL),
						lineno, typenam);
					goto err;
				}
				// kind may be ARR
				kind = (nbuf_Kind) (kind | base_kind);
				nbuf_FieldDef_set_kind(fdef, kind);
				nbuf_FieldDef_set_import_id(fdef, import_id);
				nbuf_FieldDef_set_type_id(fdef, type_id);
			}
			if (nbuf_is_repeated(kind) || kind == nbuf_Kind_STR || kind == nbuf_Kind_MSG) {
				nbuf_FieldDef_set_offset(fdef, psize);
				psize++;
			} else {
				unsigned sz = (kind == nbuf_Kind_ENUM) ? 2 : (1 << type_id);
				unsigned align = (sz > sizeof (nbuf_word_t)) ? sizeof (nbuf_word_t) : sz;
				if (align > max_align)
					max_align = align;
				ssize = ((ssize + align - 1) &~ (align - 1));
				nbuf_FieldDef_set_offset(fdef, ssize);
				ssize += sz;
			}
		}
		if (psize > 0)
			max_align = sizeof (nbuf_word_t);
		ssize = ((ssize + max_align - 1) &~ (max_align - 1));
		nbuf_MsgDef_set_ssize(mdef, ssize);
		nbuf_MsgDef_set_psize(mdef, psize);
	}
	rc = true;
err:
	{
		char **p;
		size_t n;
		FOR_EACH(char *, p, n, ctx->typenames) {
			free(*p);
		}
		ctx->typenames.len = 0;
	}
	return rc;
}

static struct FileState *
find_known_file(struct ctx *ctx, FILE *f, const char *path)
{
	struct FileState *fs;
	size_t n;

	FOR_EACH(struct FileState, fs, n, *ctx->file_states)
		if (samefile(fs, f, path))
			return fs;
	return NULL;
}

static struct nbuf_schema_set *
parse_opened_file(struct ctx *ctx, struct nbuf_buf *textschema, const char *filename)
{
	lexState l[1];
	nbuf_Schema schema;
	struct nbuf_buf binschema;
	struct nbuf_buf imports;
	bool rc = false;
	struct nbuf_schema_set *ss = NULL;
	struct FileState *fs = NULL;
	size_t i;

	memset(&binschema, 0, sizeof binschema);
	memset(&imports, 0, sizeof imports);
	if (!nbuf_init_rw(&binschema, 4096) ||
		!nbuf_alloc_Schema(&schema, &binschema))
		goto err;
	if (!(filename = nbuf_Schema_set_src_name(schema, filename, -1)))
		goto err;
	/* filename is now persistent (no longer in scratch_buf) */
	i = LEN(struct FileState, *ctx->file_states);
	if (!(fs = new_FileState(ctx, filename)))
		goto err;
	nbuf_lexinit(l, filename, textschema->base, textschema->len);
	NEXT;  /* get the first token from input */
	if (!parse_package(ctx, l, schema))
		goto err;
	rc = parse_import(ctx, l, &imports);
	/* parse_import may invalidate fs pointer */
	fs = GET(struct FileState, *ctx->file_states, i);
	if (!rc)
		goto err;
	rc = false;
	fs->ss = ss = (struct nbuf_schema_set *)
		malloc(offsetof(struct nbuf_schema_set, imports) +
		sizeof (ss->imports[0]) * LEN(struct nbuf_schema_set *, imports));
	if (!ss)
		goto err;
	/* steal buffer from binschema to ss->buf */
	ss->buf = binschema;
	memset(&binschema, 0, sizeof binschema);
	ss->nimports = LEN(struct nbuf_schema_set *, imports);
	if (imports.len)
		memcpy(ss->imports, imports.base, imports.len);
	imports.len = 0;

	if (!nbuf_get_Schema(&schema, &ss->buf, 0))
		goto err;
	if (!parse_enum_defs(ctx, l, schema))
		goto err;
	if (!parse_message_defs(ctx, l, schema))
		goto err;
	EXPECT(EOF);

	/* File is parsed.  Deal with unresolved types and
	 * filling in unknown fields. */
	if (!complete_message_defs(ctx, schema))
		goto err;
	rc = true;
err:
	if (fs)
		fs->is_open = false;
	nbuf_clear(&imports);
	nbuf_clear(&binschema);
	return rc ? ss : NULL;
}

struct nbuf_schema_set *
parse_file(struct ctx *ctx, const char *filename)
{
	struct nbuf_buf textschema;  // load_file
	FILE *f = NULL;
	struct FileState *fs = NULL;
	struct nbuf_schema_set *ss = NULL;

	memset(&textschema, 0, sizeof textschema);
	if (++ctx->depth == MAX_DEPTH) {
		fprintf(stderr, "max import depth (%d) exceeded\n", MAX_DEPTH);
		goto err;
	}
	f = nbufc_search_open(&ctx->scratch_buf, ctx->search_path, filename);
	/* filename may be in scratch_buf, and it may have been re-allocated. */
	filename = ctx->scratch_buf.base;
	if (!f) {
		fprintf(stderr, "file '%s' cannot be found.\n", filename);
		goto err;
	}
	if ((fs = find_known_file(ctx, f, filename))) {
		struct FileState *fs_end = GET(struct FileState, *ctx->file_states,
						LEN(struct FileState, *ctx->file_states));
		if (fs->is_open) {
			fprintf(stderr, "error: circular dependency: ");
			do {
				fprintf(stderr, "%s -> ", fs->filename);
			} while (++fs < fs_end);
			fprintf(stderr, "%s\n", filename);
		} else {
			/* not circular dependency, we will reuse the previously
			 * parsed result.
			 */
			ss = fs->ss;
		}
		goto err;
	}
	nbuf_load_fp(&textschema, f);
	fclose(f);
	ss = parse_opened_file(ctx, &textschema, filename);
	nbuf_unload_file(&textschema);
err:
	--ctx->depth;
	return ss;
}

void nbufc_free_compiled(const struct nbufc_compile_opt *opt)
{
	CLR(struct FileState, *opt->outbuf, dtor_FileState);
}

static void initctx(struct ctx *ctx, const struct nbufc_compile_opt *opt)
{
	memset(ctx, 0, sizeof *ctx);
	ctx->buf = ctx->bufs;
	ctx->file_states = opt->outbuf;
	ctx->search_path = opt->search_path;
}

static void finictx(struct ctx *ctx)
{
	size_t i;

	/* Clean up resources. */
	nbuf_clear(&ctx->typenames);
	nbuf_clear(&ctx->scratch_buf);
	for (i = 0; i < MAX_BUFFER; i++)
		nbuf_clear(&ctx->bufs[i]);
}

struct nbuf_schema_set *
nbufc_compile(const struct nbufc_compile_opt *opt, const char *filename)
{
	struct ctx ctx[1];
	struct nbuf_schema_set *ss = NULL;

	initctx(ctx, opt);
	ss = parse_file(ctx, filename);
	finictx(ctx);

	if (!ss)
		nbufc_free_compiled(opt);
	return ss;
}

struct nbuf_schema_set *
nbufc_compile_str(const struct nbufc_compile_opt *opt,
	const char *input, size_t len, const char *filename)
{
	struct ctx ctx[1];
	struct nbuf_schema_set *ss = NULL;
	struct nbuf_buf textschema;

	initctx(ctx, opt);
	nbuf_init_ro(&textschema, input, len);
	ss = parse_opened_file(ctx, &textschema, filename);
	finictx(ctx);

	if (!ss)
		nbufc_free_compiled(opt);
	return ss;
}
