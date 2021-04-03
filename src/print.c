#include "libnbuf.h"

#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <stdbool.h>

struct ctx {
	FILE *f;
	int indent, curr_indent;
	int depth, max_depth;
	char nl;
	unsigned print_flags;
};

static bool
print_msg(struct ctx *ctx, const struct nbuf_obj *o, nbuf_MsgDef mdef);

static void
indent(struct ctx *ctx)
{
	int curr_indent = ctx->curr_indent;

	while (curr_indent-- > 0)
		putc(' ', ctx->f);
}

static void
indent_fname(struct ctx *ctx, const char *fname, size_t len)
{
	indent(ctx);
	fwrite(fname, 1, len, ctx->f);
}

static bool
print_scalar(struct ctx *ctx,
	const void *ptr, nbuf_Kind kind, unsigned size)
{
	switch (kind) {
	case nbuf_Kind_UINT:
		switch (size) {
		case 1: fprintf(ctx->f, "%" PRIu8, nbuf_u8(ptr)); break;
		case 2: fprintf(ctx->f, "%" PRIu16, nbuf_u16(ptr)); break;
		case 4: fprintf(ctx->f, "%" PRIu32, nbuf_u32(ptr)); break;
		case 8: fprintf(ctx->f, "%" PRIu64, nbuf_u64(ptr)); break;
		default: goto bad;
		}
		break;
	case nbuf_Kind_SINT:
		switch (size) {
		case 1: fprintf(ctx->f, "%" PRId8, nbuf_i8(ptr)); break;
		case 2: fprintf(ctx->f, "%" PRId16, nbuf_i16(ptr)); break;
		case 4: fprintf(ctx->f, "%" PRId32, nbuf_i32(ptr)); break;
		case 8: fprintf(ctx->f, "%" PRId64, nbuf_i64(ptr)); break;
		default: goto bad;
		}
		break;
	case nbuf_Kind_FLT:
		switch (size) {
		case 4: fprintf(ctx->f, "%.*g", FLT_DIG+1, nbuf_f32(ptr)); break;
		case 8: fprintf(ctx->f, "%.*lg", DBL_DIG+1, nbuf_f64(ptr)); break;
		default: goto bad;
		}
		break;
	case nbuf_Kind_BOOL:
		fprintf(ctx->f, "%s", nbuf_u8(ptr) ? "true" : "false");
		break;
	default:
bad:
		fprintf(ctx->f, "/* bad scalar: kind=%u, size=%u */",
			kind, size);
		return false;
	}
	return true;
}

static bool
print_enum(struct ctx *ctx, const void *ptr, nbuf_EnumDef edef)
{
	nbuf_EnumVal eval;
	int16_t val;

	val = nbuf_i16(ptr);
	if (!nbuf_EnumVal_from_value(&eval, edef, val))
		fprintf(ctx->f, "%d", val);
	else
		fprintf(ctx->f, "%s", nbuf_EnumVal_symbol(eval, NULL));
	return true;
}

static bool
print_field(struct ctx *ctx, const struct nbuf_obj *o, nbuf_FieldDef fdef)
{
	struct nbuf_obj oo;
	union {
		struct nbuf_obj o;
		nbuf_EnumDef edef;
		nbuf_MsgDef mdef;
	} u;
	size_t fname_len;
	const char *fname = nbuf_FieldDef_name(fdef, &fname_len);
	size_t len, slen;
	const void *ptr;
	bool ok;
	bool rc = false;

	unsigned offset = nbuf_FieldDef_offset(fdef);
	nbuf_Kind kind = nbuf_get_field_type(&u.o, fdef);
	nbuf_Kind base_kind = nbuf_base_kind(kind);

	switch ((int) kind) {
	case nbuf_Kind_UINT|nbuf_Kind_ARR:
	case nbuf_Kind_SINT|nbuf_Kind_ARR:
	case nbuf_Kind_FLT|nbuf_Kind_ARR:
	case nbuf_Kind_ENUM|nbuf_Kind_ARR:
	case nbuf_Kind_BOOL|nbuf_Kind_ARR:
		if (!(len = nbuf_obj_p(&oo, o, offset)))
			break;
		for (;;) {
			ptr = nbuf_obj_base(&oo);
print_one_scalar:
			indent_fname(ctx, fname, fname_len);
			fprintf(ctx->f, ": ");
			ok = (base_kind == nbuf_Kind_ENUM) ?
				print_enum(ctx, ptr, u.edef) :
				print_scalar(ctx, ptr, base_kind, u.o.ssize);
			if (!ok)
				goto err;
			putc(ctx->nl, ctx->f);
			if (--len == 0)
				break;
			nbuf_next(&oo);
		}
		break;
	case nbuf_Kind_ENUM:
		if (!(ptr = nbuf_obj_s(o, offset, 2)))
			break;
		len = 1;
		goto print_one_scalar;
	case nbuf_Kind_UINT:
	case nbuf_Kind_SINT:
	case nbuf_Kind_FLT:
	case nbuf_Kind_BOOL:
		if (!(ptr = nbuf_obj_s(o, u.o.offset, u.o.ssize)))
			break;
		len = 1;
		goto print_one_scalar;
	case nbuf_Kind_MSG:
	case nbuf_Kind_MSG|nbuf_Kind_ARR:
		if (!(len = nbuf_obj_p(&oo, o, offset)))
			break;
		for (;;) {
			indent_fname(ctx, fname, fname_len);
			fprintf(ctx->f, " {");
			putc(ctx->nl, ctx->f);
			ctx->curr_indent += ctx->indent;
			ok = print_msg(ctx, &oo, u.mdef);
			if (!ok)
				goto err;
			ctx->curr_indent -= ctx->indent;
			indent(ctx);
			putc('}', ctx->f);
			putc(ctx->nl, ctx->f);
			if (--len == 0)
				break;
			nbuf_next(&oo);
		}
		break;
	case nbuf_Kind_STR|nbuf_Kind_ARR:
		if (!(len = nbuf_obj_p(&oo, o, offset)))
			break;
		for (;;) {
			struct nbuf_obj ooo;
			slen = nbuf_obj_p(&ooo, &oo, 0);
			ptr = nbuf_obj2str(&ooo, slen, &slen);
print_one_str:
			indent_fname(ctx, fname, fname_len);
			fprintf(ctx->f, ": ");
			nbuf_print_escaped(ctx->f, (const char *) ptr, slen,
				ctx->print_flags);
			putc(ctx->nl, ctx->f);
			if (--len == 0)
				break;
			nbuf_next(&oo);
		}
		break;
	case nbuf_Kind_STR:
		slen = nbuf_obj_p(&oo, o, offset);
		ptr = nbuf_obj2str(&oo, slen, &slen);
		len = 1;
		goto print_one_str;
	default:
		fprintf(ctx->f, "%s: /* bad field */%c", fname, ctx->nl);
		break;
	}
	rc = true;
err:
	return rc;
}

static bool
print_msg(struct ctx *ctx, const struct nbuf_obj *o, nbuf_MsgDef mdef)
{
	nbuf_FieldDef fdef;
	bool rc = false;
	size_t n;

	if (ctx->depth >= ctx->max_depth) {
		fprintf(ctx->f, "%*s/* print depth limit (%d) exceeded */%c",
			ctx->curr_indent, "", ctx->max_depth, ctx->nl);
		return true;
	}
	++ctx->depth;

	for (n = nbuf_MsgDef_fields(&fdef, mdef, 0); n--;
		nbuf_next(NBUF_OBJ(fdef))) {
		if (!print_field(ctx, o, fdef))
			goto err;
	}
	rc = true;
err:
	--ctx->depth;
	return rc;
}

bool nbuf_print(const struct nbuf_print_opt *opt,
	const struct nbuf_obj *o, nbuf_MsgDef mdef)
{
	struct ctx ctx = {
		.f = opt->f,
		.indent = (opt->indent < 0) ? 0 : opt->indent,
		.curr_indent = 0,
		.depth = 0,
		.max_depth = (opt->max_depth > 0) ? opt->max_depth : 500,
		.nl = (opt->indent < 0) ? ' ' : '\n',
		.print_flags = (opt->loose_escape) ? NBUF_PRINT_LOOSE_ESCAPE : 0,
	};

	if (opt->msg_type_hdr) {
		nbuf_Schema schema;
		if (!nbuf_get_Schema(&schema, NBUF_OBJ(mdef)->buf, 0))
			return false;
		const char *pkg_name = nbuf_Schema_pkg_name(schema, NULL);
		fprintf(opt->f, "# %s%s%s\n", pkg_name,
			*pkg_name ? "." : "", nbuf_MsgDef_name(mdef, NULL));
	}

	return print_msg(&ctx, o, mdef);
}
