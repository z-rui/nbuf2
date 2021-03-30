/* C++ code generator */

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
	struct nbuf_buf strbuf;
	nbuf_Schema schema;
	int pass;
};

char *nbufc_replace_dots(struct nbuf_buf *, const char *, const char *);
void nbufc_out_upper_ident(FILE *f, const char *s);

static void
out_enum(struct ctx *ctx, nbuf_EnumDef edef)
{
	FILE *f = ctx->f;
	const char *name;
	size_t n;
	nbuf_EnumVal eval;

	name = nbuf_EnumDef_name(edef, NULL);
	fprintf(f, "enum class %s : int16_t {\n", name);
	for (n = nbuf_EnumDef_values(&eval, edef, 0); n--; nbuf_next(NBUF_OBJ(eval))) {
		const char *symbol = nbuf_EnumVal_symbol(eval, NULL);
		uint16_t value = nbuf_EnumVal_value(eval);
		fprintf(f, "\t%s = %d,\n", symbol, value);
	}
	fprintf(f, "};\n\n");
}

static void out_struct(struct ctx *ctx, nbuf_MsgDef mdef)
{
	FILE *f = ctx->f;
	const char *name;
	unsigned ssize, psize;

	name = nbuf_MsgDef_name(mdef, NULL);
	if (ctx->pass == 0) {
		fprintf(f, "struct %s {\n"
			"\tstruct reader;\n"
			"\tstruct writer;\n", name);
		fprintf(f, "\tstatic inline reader get(::nbuf::buffer *buf, size_t offset = 0);\n");
		fprintf(f, "\tstatic inline writer alloc(::nbuf::buffer *buf);\n");
		fprintf(f, "\tstatic inline ::nbuf::pointer_array<writer> alloc(::nbuf::buffer *buf, size_t n);\n");
		fprintf(f, "};\n\n");
	} else if (ctx->pass == 2) {
		ssize = nbuf_MsgDef_ssize(mdef);
		psize = nbuf_MsgDef_psize(mdef);
		fprintf(f, "%s::reader %s::get(::nbuf::buffer *buf, size_t offset) {\n",
			name, name);
		fprintf(f, "\t%s::reader o;\n"
			"\to.get(buf, offset);\n"
			"\treturn o;\n"
			"}\n\n", name);
		fprintf(f, "%s::writer %s::alloc(::nbuf::buffer *buf) {\n", name, name);
		fprintf(f, "\t%s::writer o;\n"
			"\to.alloc(buf, %u, %u);\n"
			"\treturn o;\n"
			"}\n\n", name, ssize, psize);
		fprintf(f, "::nbuf::pointer_array<%s::writer> %s::alloc(::nbuf::buffer *buf, size_t n) {\n", name, name);
		fprintf(f, "\t::nbuf::object o;\n"
			"\tif (!o.alloc(buf, %u, %u, n)) n = 0;\n"
			"\treturn ::nbuf::pointer_array<writer>(o, n);\n"
			"}\n\n",
			ssize, psize);
	}
}

static const char *full_typenam(struct ctx *ctx, const struct nbuf_obj *typedesc)
{
	nbuf_Schema schema;
	size_t typenam_len;
	const char *typenam, *pkg_name;

	typenam = nbuf_EnumDef_name(* (const nbuf_EnumDef *) typedesc, &typenam_len);
	if (!nbuf_get_Schema(&schema, typedesc->buf, 0)) {
		fprintf(stderr, "internal error: cannot load schema "
			"for enum %s.\n", typenam);
		return NULL;
	}
	pkg_name = nbuf_Schema_pkg_name(schema, NULL);
	if (!nbufc_replace_dots(&ctx->strbuf, pkg_name, "::") ||
		!nbuf_add(&ctx->strbuf, "::", 2) ||
		!nbuf_add(&ctx->strbuf, typenam, typenam_len) ||
		!nbuf_add1(&ctx->strbuf, '\0'))
		return NULL;
	return ctx->strbuf.base;
}

static void out_scalar_field(struct ctx *ctx, const char *msg_name, const char *fname,
	nbuf_Kind kind, unsigned offset, const struct nbuf_obj *typedesc)
{
	FILE *f = ctx->f;
	const char *typenam = NULL;
	int repeated = nbuf_is_repeated(kind);

	ctx->strbuf.len = 0;
	kind = nbuf_base_kind(kind);
	switch (kind) {
	case nbuf_Kind_BOOL:
		typenam = "bool";
		break;
	case nbuf_Kind_FLT:
		typenam = (typedesc->ssize == 4) ? "float" : "double";
		break;
	case nbuf_Kind_ENUM:
		typenam = full_typenam(ctx, typedesc);
		break;
	default: {
		int n;
		char *p;
		if (!(p = nbuf_alloc(&ctx->strbuf, 32)))
			return;
		n = sprintf(p, "%sint%u_t",
			kind == nbuf_Kind_UINT ? "u" : "", typedesc->ssize * 8);
		if (n == -1)
			return;
		typenam = p;
		break;
	}
	}
	if (!typenam) {
		fprintf(stderr, "internal error: cannot get type name for field %s\n", fname);
		return;
	}

	if (repeated) {
		if (ctx->pass == 0) {
			// Getter.
			fprintf(f, "\t::nbuf::scalar_array<const ::nbuf::scalar<%s>> %s() {\n", typenam, fname);
			fprintf(f, "\t::nbuf::object o;\n"
				"size_t n = ::nbuf::object::pointer_field(&o, %u);\n", offset);
			fprintf(f, "\t\treturn ::nbuf::scalar_array<const ::nbuf::scalar<%s>>(o, n);\n"
				"\t}\n", typenam);
		} else if (ctx->pass == 1) {
			// Allocator.
			fprintf(f, "\t::nbuf::scalar_array<::nbuf::scalar<%s>> alloc_%s(size_t n) {\n", typenam, fname);
			fprintf(f, "\t\tauto o = ::nbuf::scalar_array<::nbuf::scalar<%s>>::alloc(buf, n);\n"
				"\t\tif (o) ::nbuf::object::set_pointer_field(%u, o);\n"
				"\t\treturn o;\n"
				"\t}\n", typenam, offset);
		}

	} else {
		if (ctx->pass == 0) {
			// Getter.
			fprintf(f, "\t%s %s() {\n", typenam, fname);
			fprintf(f, "\t\tconst auto *p = ::nbuf::object::scalar_field<%s>(%u);\n",
				typenam, offset);
			fprintf(f, "\t\tif (p) return static_cast<%s>(*p);\n", typenam);
			fprintf(f, "\t\treturn static_cast<%s>(0);\n\t}\n\n", typenam);
		} else if (ctx->pass == 1) {
			// Setter.
			fprintf(f, "\t::nbuf::scalar<%s> *set_%s(%s v) {\n", typenam, fname, typenam);
			fprintf(f, "\t\tauto *p = ::nbuf::object::scalar_field<%s>(%u);\n",
				typenam, offset);
			fprintf(f, "\t\tif (p) *p = v;\n"
				"\t\treturn p;\n\t}\n\n");
		}
	}
}

static void out_msg_field(struct ctx *ctx, const char *msg_name, const char *fname,
	nbuf_Kind kind, unsigned offset, nbuf_MsgDef mdef)
{
	FILE *f = ctx->f;
	int repeated = nbuf_is_repeated(kind);
	const char *typenam;

	ctx->strbuf.len = 0;
	typenam = full_typenam(ctx, NBUF_OBJ(mdef));
	if (!typenam) {
		fprintf(stderr, "internal error: cannot get type name for field %s\n", fname);
		return;
	}

	if (ctx->pass == 0) {
		if (repeated) {
			fprintf(f, "\tinline ::nbuf::pointer_array<%s::reader> %s() const;\n", typenam, fname);
		} else {
			fprintf(f, "\tinline %s::reader %s() const;\n", typenam, fname);
		}
	} else if (ctx->pass == 1) {
		if (repeated) {
			fprintf(f, "\tinline ::nbuf::pointer_array<%s::writer> %s() const;\n", typenam, fname);
			fprintf(f, "\tinline ::nbuf::pointer_array<%s::writer> alloc_%s(size_t n) const;\n", typenam, fname);
		} else {
			fprintf(f, "\tinline %s::writer %s() const;\n", typenam, fname);
			fprintf(f, "\tinline %s::writer alloc_%s() const;\n", typenam, fname);
		}
	} else if (ctx->pass == 2) {
		int pass;
		if (repeated) {
			for (pass = 0; pass <= 1; pass++) {
				const char *cls = pass ? "writer" : "reader";
				fprintf(f, "::nbuf::pointer_array<%s::%s> %s::%s::%s() const {\n",
					typenam, cls, msg_name, cls, fname);
				fprintf(f, "\t::nbuf::object o;\n");
				fprintf(f, "\tsize_t n = ::nbuf::object::pointer_field(&o, %u);\n", offset);
				fprintf(f, "\treturn ::nbuf::pointer_array<%s::%s>(o, n);\n"
					"}\n\n",
					typenam, cls);
			}
			fprintf(f, "::nbuf::pointer_array<%s::writer> %s::writer::alloc_%s(size_t n) const {\n",
				typenam, msg_name, fname);
			fprintf(f, "\tauto o = %s::alloc(buf, n);\n", typenam);
			fprintf(f, "\tif (o)\n");
			fprintf(f, "\t\t::nbuf::object::set_pointer_field(%u, o);\n"
				"\treturn o;\n"
				"}\n\n", offset);
		} else {
			for (pass = 0; pass <= 1; pass++) {
				const char *cls = pass ? "writer" : "reader";
				fprintf(f, "%s::%s %s::%s::%s() const {\n",
					typenam, cls, msg_name, cls, fname);
				fprintf(f, "\t%s::%s o;\n", typenam, cls);
				fprintf(f, "\t::nbuf::object::pointer_field(&o, %u);\n", offset);
				fprintf(f, "\treturn o;\n"
					"}\n\n");
			}
			fprintf(f, "%s::writer %s::writer::alloc_%s() const {\n",
				typenam, msg_name, fname);
			fprintf(f, "\tauto o = %s::alloc(buf);\n", typenam);
			fprintf(f, "\tif (o)\n");
			fprintf(f, "\t\t::nbuf::object::set_pointer_field(%u, o);\n"
				"\treturn o;\n"
				"}\n\n", offset);
		}
	}
}

static void out_str_field(struct ctx *ctx, const char *msg_name, const char *fname,
	nbuf_Kind kind, unsigned offset)
{
	FILE *f = ctx->f;
	int repeated = nbuf_is_repeated(kind);

	if (ctx->pass == 0) {
		const char *typenam = repeated ? "string_array" : "string";
		// Getter.
		fprintf(f, "\t::nbuf::%s %s() {\n", typenam, fname);
		fprintf(f, "\t\t::nbuf::object o;\n"
			"\t\tsize_t n = ::nbuf::object::pointer_field(&o, %u);\n", offset);
		fprintf(f, "\t\treturn ::nbuf::%s(o, n);\n"
			"\t\t}\n", typenam);
	} else if (ctx->pass == 1) {
		if (repeated) {
			// Allocator.
			fprintf(f, "\t::nbuf::string_array alloc_%s(size_t n) {\n", fname);
			fprintf(f, "\t\treturn ::nbuf::string_array::alloc(buf, n);\n"
				"\t}\n");
		} else {
			// Setter.
			fprintf(f, "\ttemplate <typename U>\n");
			fprintf(f, "\t::nbuf::string set_%s(const U &s) {\n", fname);
			fprintf(f, "\t\treturn ::nbuf::object::set_string_field(%u, s);\n"
				"\t}\n", offset);
		}
	}
}

static void out_accessors(struct ctx *ctx, nbuf_MsgDef mdef)
{
	const char *name;
	nbuf_FieldDef fdef;
	size_t n;

	name = nbuf_MsgDef_name(mdef, NULL);
	if (ctx->pass == 0) {
		fprintf(ctx->f, "struct %s::reader : ::nbuf::object {\n", name);
	} else if (ctx->pass == 1) {
		fprintf(ctx->f, "struct %s::writer : %s::reader {\n", name, name);
	}
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
			if (ctx->pass < 2)
				out_scalar_field(ctx, name, fname, kind, offset, &u.o);
			break;
		case nbuf_Kind_MSG:
			if (ctx->pass < 3)
				out_msg_field(ctx, name, fname, kind, offset, u.mdef);
			break;
		case nbuf_Kind_STR:
			if (ctx->pass < 2)
				out_str_field(ctx, name, fname, kind, offset);
			break;
		default:
			fprintf(stderr, "internal error: bad scalar kind %u\n", kind);
			break;
		}
	}
	if (ctx->pass == 0 || ctx->pass == 1) {
		fprintf(ctx->f, "};\n\n");
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
		fprintf(ctx->f, "#include \"%.*s.nb.hpp\"\n",
			(int) basename_len, src_name);
	}
	fprintf(ctx->f, "\n");
}

static void begin_namespace(struct ctx *ctx, const char *pkg_name)
{
	const char *s = pkg_name;

	if (*s != '\0') {
		char ch;
		do {
			fprintf(ctx->f, "namespace ");
			for (;;) {
				ch = *s++;
				if (ch == '\0' || ch == '.') {
					fprintf(ctx->f, " {\n");
					break;
				}
				putc(ch, ctx->f);
			}
		} while (ch != '\0');
	}
}

static void end_namespace(struct ctx *ctx, const char *pkg_name)
{
	const char *s = pkg_name;

	if (*s != '\0') {
		char ch;
		do {
			const char *p = s;
			for (;;) {
				ch = *s++;
				if (ch == '\0' || ch == '.') {
					fprintf(ctx->f, "}  // namespace %.*s\n",
						(int) (s-p), p);
					break;
				}
			}
		} while (ch != '\0');
	}
}

static void out_body(struct ctx *ctx)
{
	const char *pkg_name = nbuf_Schema_pkg_name(ctx->schema, NULL);
	nbuf_EnumDef edef;
	nbuf_MsgDef mdef;
	size_t n;

	begin_namespace(ctx, pkg_name);
	for (n = nbuf_Schema_enums(&edef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(edef)))
		out_enum(ctx, edef);
	for (n = nbuf_Schema_messages(&mdef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(mdef)))
		out_struct(ctx, mdef);
	for (ctx->pass = 0; ctx->pass <= 1; ctx->pass++) {
		for (n = nbuf_Schema_messages(&mdef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(mdef)))
			out_accessors(ctx, mdef);
	}
	for (n = nbuf_Schema_messages(&mdef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(mdef)))
		out_struct(ctx, mdef);
	for (n = nbuf_Schema_messages(&mdef, ctx->schema, 0); n--; nbuf_next(NBUF_OBJ(mdef)))
		out_accessors(ctx, mdef);
	end_namespace(ctx, pkg_name);
}

int nbufc_codegen_cpp(const struct nbufc_codegen_opt *opt, struct nbuf_schema_set *ss)
{
	FILE *f;
	size_t n;
	int rc = 1;
	struct ctx ctx[1];
	const char *src_name;
	char *out_filename = NULL;
	const char suffix[] = ".nb.hpp";

	memset(ctx, 0, sizeof ctx);
	if (!nbuf_get_Schema(&ctx->schema, &ss->buf, 0))
		goto err;
	ctx->ss = ss;

	src_name = nbuf_Schema_src_name(ctx->schema, NULL);
	n = nbufc_baselen(src_name);

	/* .nb.hpp */
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
	fprintf(f, "// Generated by nbufc.  DO NOT EDIT!\n"
		"// source: %s\n"
		"//\n\n", src_name);
	fprintf(f, "#ifndef ");
	nbufc_out_upper_ident(f, out_filename);
	fprintf(f, "_\n#define ");
	nbufc_out_upper_ident(f, out_filename);
	fprintf(f, "_\n\n");
	fprintf(f, "#include \"nbuf.hpp\"\n");

	out_inc(ctx);
	out_body(ctx);

	/* Epilogue */
	fprintf(f, "#endif  // ");
	nbufc_out_upper_ident(f, out_filename);
	fprintf(f, "_\n");

	rc = ferror(f);
	fclose(f);
	fprintf(stderr, "%s%s\n", rc ? "error generating " : "", out_filename);
err:
	nbuf_clear(&ctx->strbuf);
	free(out_filename);
	return rc;
}
