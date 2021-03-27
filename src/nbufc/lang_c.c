/* C code generator */

#include "libnbuf.h"
#include "libnbufc.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* Local context. */
struct ctx {
	FILE *f;  /* output file */
	struct nbuf_schema_set *ss;
	nbuf_Schema schema;
	char *prefix;  /* pre-computed package prefix */
};

/* Replace non-alphanumeric characters with '_' and append '_' in the end.
 * Exception: "" -> "".
 */
static char *
precompute_prefix(const char *pkg_name)
{
	size_t n = strlen(pkg_name);
	char *prefix;
	const char *s = pkg_name;
	char *t;

	prefix = t = (char *) malloc(n + 3);
	if (!prefix)
		return NULL;
	if (pkg_name[0] == '\0') {
		prefix[0] = '\0';
		return prefix;
	}
	if (!isalpha(pkg_name[0]))
		*t++ = '_';
	for (s = pkg_name; *s; ++s)
		*t++ = isalnum(*s) ? *s : '_';
	*t++ = '_';
	*t = '\0';
	return prefix;
}

static void out_upper_ident(FILE *f, const char *s)
{
	int c = 0;

	for (; *s; ++s) {
		if (!isalpha(*s) && c == '\0')
			putc((c = '_'), f);
		if (isalnum(*s))
			putc((c = toupper(*s)), f);
		else if (c != '_')
			putc((c = '_'), f);
	}
}

static void
out_enum(struct ctx *ctx, nbuf_EnumDef edef)
{
	FILE *f = ctx->f;
	const char *name;
	size_t n;
	nbuf_EnumVal eval;

	name = nbuf_EnumDef_name(edef, NULL);
	fprintf(f, "typedef enum {\n");
	for (n = nbuf_EnumDef_values(&eval, edef, 0); n--; nbuf_next(NBUF_OBJ(eval))) {
		const char *symbol = nbuf_EnumVal_symbol(eval, NULL);
		uint16_t value = nbuf_EnumVal_value(eval);
		fprintf(f, "\t%s%s_%s = %d,\n", ctx->prefix, name, symbol, value);
	}
	fprintf(f, "} %s%s;\n", ctx->prefix, name);
	fprintf(ctx->f, "extern const struct nbuf_EnumDef_ %srefl_%s;\n\n",
		ctx->prefix, nbuf_EnumDef_name(edef, NULL));
}

static void out_struct(struct ctx *ctx, nbuf_MsgDef mdef)
{
	FILE *f = ctx->f;
	const char *name;
	unsigned ssize, psize;
	int repeated;

	name = nbuf_MsgDef_name(mdef, NULL);
	ssize = nbuf_MsgDef_ssize(mdef);
	psize = nbuf_MsgDef_psize(mdef);
	fprintf(f, "typedef struct %s%s_ {\n"
		"\tstruct nbuf_obj o;\n"
		"} %s%s;\n", ctx->prefix, name, ctx->prefix, name);
	fprintf(ctx->f, "extern const struct nbuf_MsgDef_ %srefl_%s;\n\n",
		ctx->prefix, nbuf_MsgDef_name(mdef, NULL));

	// Get message from buffer.
	fprintf(f, "static inline size_t\n");
	fprintf(f, "%sget_%s(%s%s *msg, struct nbuf *buf, size_t offset)\n{\n",
		ctx->prefix, name, ctx->prefix, name);
	fprintf(f, "\tstruct nbuf_obj *o = NBUF_OBJ(*msg);\n"
		"\to->buf = buf;\n"
		"\to->offset = offset;\n"
		"\treturn nbuf_get_obj(o);\n"
		"}\n\n");

	// Allocate message in buffer.
	for (repeated = 0; repeated <= 1; ++repeated) {
		fprintf(f, "static inline size_t\n");
		fprintf(f, "%salloc_%s%s(%s%s *msg, struct nbuf *buf%s)\n{\n",
			ctx->prefix, repeated ? "multi_" : "", name,
			ctx->prefix, name, repeated ? ", size_t n" : "");
		fprintf(f, "\tstruct nbuf_obj *o = NBUF_OBJ(*msg);\n"
			"\to->buf = buf;\n"
			"\to->ssize = %u;\n"
			"\to->psize = %u;\n"
			"\treturn %s;\n"
			"}\n\n", ssize, psize, repeated ?
			"nbuf_alloc_arr(o, n)" : "nbuf_alloc_obj(o)");
	}
}

static void out_ptr_accessor(struct ctx *ctx,
	const char *msg_name, const char *fname, unsigned offset)
{
	FILE *f = ctx->f;

	fprintf(f, "static inline size_t\n");
	fprintf(f, "%s%s_raw_%s(struct nbuf_obj *o, %s%s msg)\n{\n"
		"\treturn nbuf_obj_p(o, NBUF_OBJ(msg), %u);\n"
		"}\n\n",
		ctx->prefix, msg_name, fname, ctx->prefix, msg_name, offset);
	fprintf(f, "static inline size_t\n");
	fprintf(f, "%s%s_set_raw_%s(%s%s msg, const struct nbuf_obj *o)\n{\n"
		"\treturn nbuf_obj_set_p(NBUF_OBJ(msg), %u, o);\n"
		"}\n\n",
		ctx->prefix, msg_name, fname, ctx->prefix, msg_name, offset);
}

static void out_size(struct ctx *ctx,
	const char *msg_name, const char *fname)
{
	FILE *f = ctx->f;

	fprintf(f, "static inline size_t\n");
	fprintf(f, "%s%s_%s_size(%s%s msg)\n{\n"
		"\tstruct nbuf_obj o;\n"
		"\treturn %s%s_raw_%s(&o, msg);\n"
		"}\n\n",
		ctx->prefix, msg_name, fname, ctx->prefix, msg_name,
		ctx->prefix, msg_name, fname);
}

static void out_scalar_field(struct ctx *ctx, const char *msg_name, const char *fname,
	nbuf_Kind kind, unsigned offset, const struct nbuf_obj *typedesc)
{
	FILE *f = ctx->f;
	const char *typenam = NULL;
	char empty[] = "";
	char *typenam_prefix = empty;
	char qbuf[16];
	int repeated = nbuf_is_repeated(kind);
	unsigned sz = typedesc->ssize;
	nbuf_Schema schema;

	kind = nbuf_base_kind(kind);
	switch (kind) {
	case nbuf_Kind_BOOL:
		*qbuf = 'u';
		typenam = "bool";
		break;
	case nbuf_Kind_FLT:
		*qbuf = 'f';
		typenam = (sz == 4) ? "float" : "double";
		break;
	case nbuf_Kind_ENUM:
		*qbuf = 'i';
		typenam = nbuf_EnumDef_name(* (const nbuf_EnumDef *) typedesc, NULL);
		if (!nbuf_get_Schema(&schema, typedesc->buf, 0)) {
			fprintf(stderr, "internal error: cannot load schema "
				"for enum %s.\n", typenam);
			return;
		}
		typenam_prefix = precompute_prefix(nbuf_Schema_pkg_name(schema, NULL));
		sz = 2;
		break;
	default:
		sprintf(qbuf, "%sint%u_t",
			kind == nbuf_Kind_UINT ? "u" : "", sz * 8);
		typenam = qbuf;
		break;
	}

	if (repeated) {
		// Allocator.
		out_ptr_accessor(ctx, msg_name, fname, offset);
		out_size(ctx, msg_name, fname);
		fprintf(f, "static inline void *\n");
		fprintf(f, "%s%s_alloc_%s(%s%s msg, size_t n)\n{\n",
			ctx->prefix, msg_name, fname, ctx->prefix, msg_name);
		fprintf(f, "\tstruct nbuf_obj o = {NBUF_OBJ(msg)->buf, 0, %u, 0};\n"
			"\tif (NBUF_OBJ(msg)->psize <= %u || !(n = nbuf_alloc_arr(&o, n)) ||\n"
			"\t\t\t!%s%s_set_raw_%s(msg, &o))\n"
			"\t\treturn NULL;\n"
			"\treturn nbuf_obj_base(&o);\n"
			"}\n\n", sz, offset, ctx->prefix, msg_name, fname);
	}

	// Getter.
	fprintf(f, "static inline %s%s\n", typenam_prefix, typenam);
	fprintf(f, "%s%s_%s(%s%s msg%s)\n{\n",
		ctx->prefix, msg_name, fname, ctx->prefix, msg_name,
		repeated ? ", size_t i" : "");
	if (repeated) {
		fprintf(f, "\tstruct nbuf_obj o;\n"
			"\tconst void *p = (i >= %s%s_raw_%s(&o, msg) ?\n"
			"\t\tNULL : (nbuf_advance(&o, i), nbuf_obj_base(&o)));\n",
			ctx->prefix, msg_name, fname);
	} else {
		fprintf(f, "\tconst void *p = nbuf_obj_s(NBUF_OBJ(msg), %u, %u);\n",
			offset, sz);
	}
	fprintf(f, "\treturn (%s%s) (p ? nbuf_%c%u(p) : 0);\n}\n\n",
		typenam_prefix, typenam, *qbuf, sz * 8);

	// Setter.
	fprintf(f, "static inline void *\n");
	fprintf(f, "%s%s_set_%s(%s%s msg%s, %s%s val)\n{\n",
		ctx->prefix, msg_name, fname, ctx->prefix, msg_name,
		repeated ? ", size_t i" : "", typenam_prefix, typenam);
	if (repeated) {
		fprintf(f, "\tstruct nbuf_obj o;\n"
			"\tvoid *p = (i >= %s%s_raw_%s(&o, msg) ?\n"
			"\t\tNULL : (nbuf_advance(&o, i), nbuf_obj_base(&o)));\n",
			ctx->prefix, msg_name, fname);
	} else {
		fprintf(f, "\tvoid *p = nbuf_obj_s(NBUF_OBJ(msg), %u, %u);\n",
			offset, sz);
	}
	fprintf(f, "\tif (p) nbuf_set_%c%u(p, %sval);\n",
		*qbuf, sz * 8,
		(kind == nbuf_Kind_BOOL) ? "!!" :
		(kind == nbuf_Kind_ENUM) ? "(int16_t) " : "");
	fprintf(f, "\treturn p;\n}\n\n");
	if (typenam_prefix != empty)
		free(typenam_prefix);
}

static void out_msg_field(struct ctx *ctx, const char *msg_name, const char *fname,
	nbuf_Kind kind, unsigned offset, nbuf_MsgDef mdef)
{
	FILE *f = ctx->f;
	int repeated = nbuf_is_repeated(kind);
	nbuf_Schema schema;
	char *field_prefix;
	const char *field_typenam;

	field_typenam = nbuf_MsgDef_name(mdef, NULL);
	if (!nbuf_get_Schema(&schema, NBUF_OBJ(mdef)->buf, 0)) {
		fprintf(stderr, "internal error: cannot load schema "
			"for message %s.\n", field_typenam);
		return;
	}
	field_prefix = precompute_prefix(nbuf_Schema_pkg_name(schema, NULL));

	out_ptr_accessor(ctx, msg_name, fname, offset);

	// Getter.
	fprintf(f, "static inline size_t\n");
	fprintf(f, "%s%s_%s(struct %s%s_ *field, %s%s msg%s)\n{\n",
		ctx->prefix, msg_name, fname, field_prefix, field_typenam,
		ctx->prefix, msg_name, repeated ? ", size_t i" : "");
	fprintf(f, "\tstruct nbuf_obj *o = (struct nbuf_obj *) field;\n"
		"\tsize_t n = %s%s_raw_%s(o, msg);\n",
		ctx->prefix, msg_name, fname);
	if (repeated)
		fprintf(f, "\treturn (i >= n) ? 0 : "
			"(nbuf_advance(o, i), n - i);\n");
	else
		fprintf(f,"\treturn n;\n");
	fprintf(f, "}\n\n");

	if (repeated)
		out_size(ctx, msg_name, fname);

	// It's not possible to set indiviual elements if repeated.
	// Leave only set_raw_*.

	// Allocator.
	fprintf(f, "static inline size_t\n");
	fprintf(f, "%s%s_alloc_%s(struct %s%s_ *field, %s%s msg%s)\n{\n",
		ctx->prefix, msg_name, fname, field_prefix, field_typenam,
		ctx->prefix, msg_name, repeated ? ", size_t n" : "");
	fprintf(f, "\treturn %salloc_%s%s(field, NBUF_OBJ(msg)->buf%s) ? \n"
		"\t\t%s%s_set_raw_%s(msg, (struct nbuf_obj *) field) : 0;\n"
		"}\n\n",
		field_prefix, repeated ? "multi_" : "", field_typenam,
		repeated ? ", n" : "", ctx->prefix, msg_name, fname);

	free(field_prefix);
}

static void out_str_field(struct ctx *ctx, const char *msg_name, const char *fname,
	nbuf_Kind kind, unsigned offset)
{
	FILE *f = ctx->f;
	int repeated = nbuf_is_repeated(kind);

	out_ptr_accessor(ctx, msg_name, fname, offset);

	// Getter.
	fprintf(f, "static inline const char *\n");
	fprintf(f, "%s%s_%s(%s%s msg%s, size_t *lenp)\n{\n",
		ctx->prefix, msg_name, fname,
		ctx->prefix, msg_name, repeated ? ", size_t i" : "");
	fprintf(f, "\tstruct nbuf_obj o;\n"
		"\tsize_t n = %s%s_raw_%s(&o, msg);\n",
		ctx->prefix, msg_name, fname);
	if (repeated)
		fprintf(f, "\tn = (i >= n) ? 0 : "
			"(nbuf_advance(&o, i), nbuf_obj_p(&o, &o, 0));\n");
	fprintf(f, "\treturn nbuf_obj2str(&o, n, lenp);\n}\n\n");

	if (repeated)
		out_size(ctx, msg_name, fname);

	// Setter.
	fprintf(f, "static inline size_t\n");
	fprintf(f, "%s%s_set_%s(%s%s msg%s, const char *str, size_t len)\n{\n",
		ctx->prefix, msg_name, fname, ctx->prefix, msg_name,
		repeated ? ", size_t i" : "");
	fprintf(f, "\tstruct nbuf_obj o = {NBUF_OBJ(msg)->buf}%s;\n",
		repeated ? ", oo" : "");
	if (repeated)
		fprintf(f, "\tif (%s%s_raw_%s(&oo, msg) <= i)\n"
			"\t\treturn 0;\n"
			"\tnbuf_advance(&oo, i);\n",
			ctx->prefix, msg_name, fname);
	fprintf(f, "\tif (!nbuf_alloc_str(&o, str, len))\n"
		"\t\treturn 0;\n");
	if (repeated)
		fprintf(f, "\tnbuf_obj_set_p(&oo, 0, &o);\n");
	else
		fprintf(f, "\treturn %s%s_set_raw_%s(msg, &o);\n",
			ctx->prefix, msg_name, fname);
	fprintf(f, "}\n\n");

	if (repeated) {
		// Allocator.
		fprintf(f, "static inline size_t\n");
		fprintf(f, "%s%s_alloc_%s(%s%s msg, size_t n)\n{\n",
			ctx->prefix, msg_name, fname, ctx->prefix, msg_name);
		fprintf(f, "\tstruct nbuf_obj o = {NBUF_OBJ(msg)->buf, 0, 0, 1};\n"
			"\tif (!nbuf_alloc_arr(&o, n))\n"
			"\t\treturn 0;\n"
			"\treturn %s%s_set_raw_%s(msg, &o);\n"
			"}\n\n", ctx->prefix, msg_name, fname);
	}
}

static void out_accessors(struct ctx *ctx, nbuf_MsgDef mdef)
{
	const char *name;
	nbuf_FieldDef fdef;
	size_t n;

	name = nbuf_MsgDef_name(mdef, NULL);
	for (n = nbuf_MsgDef_fields(&fdef, mdef, 0); n--; nbuf_next(NBUF_OBJ(fdef))) {
		union {
			struct nbuf_obj o;
			nbuf_EnumDef edef;
			nbuf_MsgDef mdef;
		} u;
		nbuf_Kind kind = nbuf_get_field_type(&u.o, fdef);
		nbuf_Kind base_kind = nbuf_base_kind(kind);
		const char *fname = nbuf_FieldDef_name(fdef, NULL);
		unsigned offset = nbuf_FieldDef_offset(fdef);

		switch (base_kind) {
		case nbuf_Kind_BOOL:
		case nbuf_Kind_UINT:
		case nbuf_Kind_SINT:
		case nbuf_Kind_FLT:
		case nbuf_Kind_ENUM:
			out_scalar_field(ctx, name, fname, kind, offset, &u.o);
			break;
		case nbuf_Kind_MSG:
			out_msg_field(ctx, name, fname, kind, offset, u.mdef);
			break;
		case nbuf_Kind_STR:
			out_str_field(ctx, name, fname, kind, offset);
			break;
		default:
			fprintf(stderr, "internal error: bad scalar kind %u\n", kind);
			break;
		}
	}
}

static void out_inc(struct ctx *ctx)
{
	size_t n = ctx->ss->nimports;
	struct nbuf_schema_set **import = ctx->ss->imports;

	for (; n--; ++import) {
		nbuf_Schema schema;
		const char *src_name;
		size_t basename_len;

		nbuf_get_Schema(&schema, &(*import)->buf, 0);
		src_name = nbuf_Schema_src_name(schema, NULL);
		basename_len = nbufc_baselen(src_name);
		fprintf(ctx->f, "#include \"%.*s.nb.h\"\n",
			(int) basename_len, src_name);
	}
	fprintf(ctx->f, "\n");
}

#define SCHEMA_FILE_PREFIX "nbuf_schema_file_"

static void out_body(struct ctx *ctx)
{
	nbuf_EnumDef edef;
	nbuf_MsgDef mdef;
	const char *src_name;
	size_t n;

	for (n = nbuf_Schema_enums(&edef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(edef)))
		out_enum(ctx, edef);
	for (n = nbuf_Schema_messages(&mdef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(mdef)))
		out_struct(ctx, mdef);
	for (n = nbuf_Schema_messages(&mdef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(mdef)))
		out_accessors(ctx, mdef);
	src_name = nbuf_Schema_src_name(ctx->schema, NULL);
	fprintf(ctx->f, "extern const struct nbuf_schema_set " SCHEMA_FILE_PREFIX);
	nbufc_out_path_ident(ctx->f, src_name);
	fprintf(ctx->f, ";\n");
}


static void out_refl_c(struct ctx *ctx)
{
	size_t i, n;
	nbuf_EnumDef edef;
	nbuf_MsgDef mdef;
	unsigned long buflen = ctx->ss->buf.len;

	/* Do not use specify the length here as C++ compiler will add a trailing '\0'
	 * and complain for not enough space.
	 * However, if the buffer happens to end with '\0', we will reduce the length
	 * by one and let the compiler add it back.
	 */
	if (buflen && ctx->ss->buf.base[buflen-1] == '\0')
		buflen--;
	fprintf(ctx->f, "const struct nbuf_schema_set NBUF_SS_NAME = {\n");
	fprintf(ctx->f, "\t/*.buf=*/{ /*.base=*/(char *)\n");
	nbuf_print_escaped(ctx->f, ctx->ss->buf.base, buflen, 78);
	fprintf(ctx->f, ",\n\t\t/*.len=*/%lu, /*.cap=*/0},\n"
		"\t/*.nimports=*/%u, /*.imports=*/{\n",
		(unsigned long) ctx->ss->buf.len, (unsigned) ctx->ss->nimports);
	for (i = 0; i < ctx->ss->nimports; i++) {
		nbuf_Schema schema;
		const char *import_src;
		nbuf_get_Schema(&schema, &ctx->ss->imports[i]->buf, 0);
		import_src = nbuf_Schema_src_name(schema, NULL);
		fprintf(ctx->f, "\t\t(struct nbuf_schema_set *) &" SCHEMA_FILE_PREFIX);
		nbufc_out_path_ident(ctx->f, import_src);
		fprintf(ctx->f, ",\n");
	}
	fprintf(ctx->f, "\t}\n};\n\n");

	for (n = nbuf_Schema_enums(&edef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(edef))) {
		fprintf(ctx->f, "const nbuf_EnumDef %srefl_%s = "
			"{{(struct nbuf *) &NBUF_SS_NAME, %u, %u, %u}};\n",
			ctx->prefix, nbuf_EnumDef_name(edef, NULL),
			NBUF_OBJ(edef)->offset, NBUF_OBJ(edef)->ssize,
			NBUF_OBJ(edef)->psize);
	}
	for (n = nbuf_Schema_messages(&mdef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(mdef))) {
		fprintf(ctx->f, "const nbuf_MsgDef %srefl_%s = "
			"{{(struct nbuf *) &NBUF_SS_NAME, %u, %u, %u}};\n",
			ctx->prefix, nbuf_MsgDef_name(mdef, NULL),
			NBUF_OBJ(mdef)->offset, NBUF_OBJ(mdef)->ssize,
			NBUF_OBJ(mdef)->psize);
	}
}

int nbufc_codegen_c(const struct nbufc_codegen_opt *opt, struct nbuf_schema_set *ss)
{
	FILE *f;
	size_t n;
	int rc = 1;
	struct ctx ctx[1];
	const char *src_name;
	char *out_filename = NULL;
	const char suffix[] = ".nb.h";

	memset(ctx, 0, sizeof ctx);
	if (!nbuf_get_Schema(&ctx->schema, &ss->buf, 0))
		goto err;
	ctx->prefix = precompute_prefix(nbuf_Schema_pkg_name(ctx->schema, NULL));
	if (!ctx->prefix)
		goto err;
	ctx->ss = ss;

	src_name = nbuf_Schema_src_name(ctx->schema, NULL);
	n = nbufc_baselen(src_name);

	/* .nb.h */
	out_filename = (char *) malloc(n + sizeof suffix);
	if (!out_filename)
		goto err;
	memcpy(out_filename, src_name, n);
	strcpy(out_filename + n, suffix);

	ctx->f = f = fopen(out_filename, "w");
	if (!f) {
		perror("fopen");
		goto err;
	}

	/* Prologue. */
	fprintf(f, "/* Generated by nbufc.  DO NOT EDIT!\n"
		" * source: %s\n"
		" */\n\n", src_name);
	fprintf(f, "#ifndef ");
	out_upper_ident(f, out_filename);
	fprintf(f, "_\n#define ");
	out_upper_ident(f, out_filename);
	fprintf(f, "_\n\n");
	fprintf(f, "#include \"nbuf.h\"\n");

	out_inc(ctx);
	out_body(ctx);

	/* Epilogue */
	fprintf(f, "#endif  /* ");
	out_upper_ident(f, out_filename);
	fprintf(f, "_ */\n");

	rc = ferror(f);
	fclose(f);
	fprintf(stderr, "%s%s\n", rc ? "error generating " : "", out_filename);
	if (rc) goto err;

	/* .nb.c */
	out_filename[n + sizeof suffix-2] = 'c';
	ctx->f = f = fopen(out_filename, "w");
	if (!f) {
		perror("fopen");
		goto err;
	}
	/* Prologue. */
	fprintf(f, "/* Generated by nbufc.  DO NOT EDIT!\n"
		" * source: %s\n"
		" */\n\n", src_name);
	fprintf(ctx->f, "#define NBUF_SS_NAME " SCHEMA_FILE_PREFIX);
	nbufc_out_path_ident(ctx->f, src_name);
	fprintf(ctx->f, "\n");
	/* Trick: this macro overrides modifies the definition of
	 * struct nbuf_schema_set in libnbuf.h, so the array length will
	 * match.
	 */
	fprintf(ctx->f, "#define NBUF_SS_IMPORTS %u\n", (unsigned) ctx->ss->nimports);
	fprintf(f, "#include \"%.*s%s\"\n"
		"#include \"libnbuf.h\"\n\n", (int) n, src_name, suffix);
	out_refl_c(ctx);
	rc = ferror(f);
	fclose(f);
	fprintf(stderr, "%s%s\n", rc ? "error generating " : "", out_filename);
err:
	free(out_filename);
	free(ctx->prefix);
	return rc;
}
