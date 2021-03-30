#include "libnbuf.h"
#include "lex.h"

#include <string.h>
#include <stdlib.h>

struct ctx {
	struct nbuf_buf *buf;
	struct nbuf_buf strbuf;
	lexState l[1];
	Token token;
	int depth, max_depth;
};

#define NEXT ctx->token = nbuf_lex(ctx->l)
#define IS_C(X) (ctx->token == X)
#define IS(X) IS_C(Token_##X)
#define IS_ID(X) (IS(ID) && TOKENLEN(ctx->l) == strlen(X) && strncmp(TOKEN(ctx->l), X, TOKENLEN(ctx->l)) == 0)
#define EXPECT_C(X) if (ctx->token != (Token) X) { nbuf_lexsyntax(ctx->l, (Token) X, ctx->token); goto err; }
#define EXPECT(X) if (ctx->token != Token_##X) { nbuf_lexsyntax(ctx->l, Token_##X, ctx->token); goto err; }

static bool
parse_msg(struct ctx *ctx, struct nbuf_obj *o, nbuf_MsgDef mdef);
static bool
parse_alloced_msg(struct ctx *ctx, struct nbuf_obj *o, nbuf_MsgDef mdef);

static size_t
parse_str(struct ctx *ctx, struct nbuf_obj *o)
{
	size_t oldlen, len;

	oldlen = ctx->buf->len;
	EXPECT(STR);
	o->buf = ctx->buf;
	o->ssize = 1;
	o->psize = 0;
	if (!nbuf_alloc_arr(o, 1))
		goto err;
	for (;;) {
		ctx->buf->len--;  /* remove previous '\0' */
		assert(TOKENLEN(ctx->l) >= 2);
		if (!nbuf_unescape(ctx->buf, TOKEN(ctx->l)+1, TOKENLEN(ctx->l)-2))
			goto err;
		NEXT;
		if (!IS(STR))
			break;
	}
	len = ctx->buf->len - o->offset;
	if (len <= 1) {
		/* do not allocate for empty string */
		ctx->buf->len = oldlen;
	} else if (!nbuf_resize_arr(o, len)) {
		goto err;
	}
	return len;
err:
	return 0;
}

static bool
parse_scalar(struct ctx *ctx, void *ptr, nbuf_Kind kind, unsigned size)
{
	union {
		uint64_t u;
		int64_t i;
		float f;
		double d;
	} u;

	switch (kind) {
	case nbuf_Kind_BOOL:
		if (IS_ID("true")) {
			u.u = 1;
		} else if (IS_ID("false")) {
			u.u = 0;
		} else {
			nbuf_lexerror(ctx->l, "missing 'true' or 'false'");
			goto err;
		}
		* (uint8_t *) ptr = u.u;
	case nbuf_Kind_UINT:
	case nbuf_Kind_SINT:
		EXPECT(INT);
		/* fallthrough */
	case nbuf_Kind_FLT:
		/* In case this is the last token in the input,
		 * it is not guaranteed to end with '\0', so
		 * we need a temporary buffer to avoid buffer
		 * overrun. */
		ctx->strbuf.len = 0;
		if (!(nbuf_add(&ctx->strbuf, TOKEN(ctx->l), TOKENLEN(ctx->l))))
			goto err;
		if (!(nbuf_add1(&ctx->strbuf, '\0')))
			goto err;
		if (kind == nbuf_Kind_FLT) {
			if (!IS(INT))
				EXPECT(FLT);
			if (size == 4)
				u.f = strtof(ctx->strbuf.base, NULL);
			else if (size == 8)
				u.d = strtod(ctx->strbuf.base, NULL);
			else
				goto bad_scalar;
		} else if (kind == nbuf_Kind_SINT) {
			u.i = strtoll(ctx->strbuf.base, NULL, 0);
		} else {
			u.u = strtoull(ctx->strbuf.base, NULL, 0);
		}
		switch (size) {
		case 1: nbuf_set_u8(ptr, u.u); break;
		case 2: nbuf_set_u16(ptr, u.u); break;
		case 4: nbuf_set_u32(ptr, u.u); break;
		case 8: nbuf_set_u64(ptr, u.u); break;
		default: goto bad_scalar;
		}
		break;
	default:
bad_scalar:
		fprintf(stderr, "internal error: bad scalar type (%u, %u)\n",
			kind, size);
		goto err;
	}
	NEXT;
	return true;
err:
	return false;
}

static bool
parse_enum(struct ctx *ctx, void *ptr, nbuf_EnumDef edef)
{
	nbuf_EnumVal eval;

	if (IS(INT))
		return parse_scalar(ctx, ptr, nbuf_Kind_SINT, /*size=*/2);
	EXPECT(ID);
	if (!nbuf_EnumVal_from_symbol(&eval, edef,
			TOKEN(ctx->l), TOKENLEN(ctx->l))) {
		nbuf_lexerror(ctx->l, "bad enum value '%.*s'",
			(int) TOKENLEN(ctx->l), TOKEN(ctx->l));
		goto err;
	}
	NEXT;
	*(int16_t *) ptr = nbuf_EnumVal_value(eval);
	*(int16_t *) ptr = nbuf_i16(ptr);
	return true;
err:
	return false;
}

static bool
parse_single_field(struct ctx *ctx, struct nbuf_obj *o, const char *fname, nbuf_Kind kind, unsigned offset, const struct nbuf_obj *typespec)
{
	union {
		const struct nbuf_obj *o;
		const nbuf_MsgDef *mdef;
		const nbuf_EnumDef *edef;
	} u = { typespec };

	if (kind == nbuf_Kind_STR) {
		struct nbuf_obj oo;
		size_t len;

		EXPECT_C(':'); NEXT;
		if (!(len = parse_str(ctx, &oo)))
			goto err;
		/* if len == 1, string is empty. */
		if (len > 1 && !nbuf_obj_set_p(o, offset, &oo))
			goto err;
	} else if (kind == nbuf_Kind_MSG) {
		struct nbuf_obj oo;

		EXPECT_C('{'); NEXT;
		if (!parse_msg(ctx, &oo, *u.mdef))
			goto err;
		EXPECT_C('}'); NEXT;
		if (!nbuf_obj_set_p(o, offset, &oo))
			goto err;
	} else if (kind == nbuf_Kind_ENUM) {
		void *ptr = nbuf_obj_s(o, offset, 2);

		EXPECT_C(':'); NEXT;
		if (!ptr) {
			fprintf(stderr, "internal error: cannot get scalar pointer "
				"at offset %u\n", offset);
			goto err;
		}
		if (!parse_enum(ctx, ptr, *u.edef))
			goto err;
	} else {  /* scalar */
		void *ptr = nbuf_obj_s(o, offset, typespec->ssize);

		if (!ptr) {
			fprintf(stderr, "internal error: cannot get scalar pointer "
				"at offset %u\n", offset);
			goto err;
		}
		EXPECT_C(':'); NEXT;
		if (!parse_scalar(ctx, ptr, kind, typespec->ssize))
			goto err;
	}
	if (IS_C(';')) NEXT;
	return true;
err:
	return false;
}

static bool
parse_repeated_field(struct ctx *ctx, struct nbuf_obj *o, const char *fname,
	nbuf_Kind kind, unsigned offset, const struct nbuf_obj *typespec)
{
	union {
		const struct nbuf_obj *o;
		const nbuf_MsgDef *mdef;
		const nbuf_EnumDef *edef;
	} u = { typespec };
	struct nbuf_buf newbuf = {NULL}, *oldbuf = ctx->buf;
	struct nbuf_obj oo, it = {ctx->buf};
	size_t count = 0;
	bool rc = false;

	if (nbuf_obj_p(&oo, o, offset)) {
		nbuf_lexerror(ctx->l,
			"repeated field '%s' is scattered", fname);
		goto err;
	}
	if (kind == nbuf_Kind_MSG) {
		it.ssize = nbuf_MsgDef_ssize(*u.mdef);
		it.psize = nbuf_MsgDef_psize(*u.mdef);
	} else if (kind == nbuf_Kind_ENUM) {
		it.ssize = 2;
		it.psize = 0;
	} else if (kind == nbuf_Kind_STR) {
		it.ssize = 0;
		it.psize = 1;
	} else {
		it.ssize = typespec->ssize;
		it.psize = typespec->psize;
	}
	if (!nbuf_alloc_arr(&it, 1))
		return false;
	if (it.psize > 0) {
		/* elements may allocate; they need to be created
		 * on a new buffer so the array can grow on the old buffer.
		 */
		ctx->buf = &newbuf;
	}
	for (;;) {
		++count;
		if (kind == nbuf_Kind_STR) {
			size_t len;

			EXPECT_C(':'); NEXT;
			if (!(len = parse_str(ctx, &oo)))
				goto err;
			/* if len == 1, string is empty. */
			if (len > 1 && !nbuf_obj_set_p(&it, 0, &oo))
				goto err;
		} else if (kind == nbuf_Kind_MSG) {
			EXPECT_C('{'); NEXT;
			if (!parse_alloced_msg(ctx, &it, *u.mdef))
				goto err;
			EXPECT_C('}'); NEXT;
		} else if (kind == nbuf_Kind_ENUM) {
			EXPECT_C(':'); NEXT;
			if (!parse_enum(ctx, nbuf_obj_base(&it), *u.edef))
				goto err;
		} else {  /* scalar */
			EXPECT_C(':'); NEXT;
			if (!parse_scalar(ctx, nbuf_obj_base(&it), kind,
				it.ssize))
				goto err;
		}
		nbuf_next(&it);
		if (!IS_ID(fname))
			break;
		NEXT;
		// sub-messages should not allocate on this buffer
		assert(it.buf->len == it.offset);
		if (!nbuf_alloc(it.buf, nbuf_obj_size(&it)))
			goto err;
	}
	assert(count > 0);
	nbuf_advance(&it, -count);
	if (!nbuf_resize_arr(&it, count)) {
		fprintf(stderr, "internal error: cannot resize arr\n");
		goto err;
	}
	if (it.psize > 0 && !nbuf_fix_arr(&it, count, &newbuf))
		goto err;
	if (!nbuf_obj_set_p(o, offset, &it))
		goto err;
	rc = true;
err:
	nbuf_clear(&newbuf);
	ctx->buf = oldbuf;
	return rc;
}

/* msg ::= { ID ':' (INT|STR|FLT) }
 * msg ::= { ID '{' msg '}' }
 */
static bool
parse_alloced_msg(struct ctx *ctx, struct nbuf_obj *o, nbuf_MsgDef mdef)
{
	while (IS(ID)) {
		const char *fname = TOKEN(ctx->l);
		size_t len = TOKENLEN(ctx->l);
		nbuf_FieldDef fdef;
		nbuf_Kind kind;
		unsigned offset;
		union {
			struct nbuf_obj o;
			nbuf_EnumDef edef;
			nbuf_MsgDef mdef;
		} u;
		bool ok;

		if (!nbuf_lookup_field(&fdef, mdef, fname, len)) {
			nbuf_lexerror(ctx->l, "unknown field '%.*s'", (int) len, fname);
			return false;
		}
		fname = nbuf_FieldDef_name(fdef, NULL);
		offset = nbuf_FieldDef_offset(fdef);
		kind = nbuf_get_field_type(&u.o, fdef);
		if (kind == -1) {
			nbuf_lexerror(ctx->l,
				"cannot determine type for field '%s'", fname);
			return false;
		}
		NEXT;
		ok = nbuf_is_repeated(kind) ? 
			parse_repeated_field(ctx, o, fname, nbuf_base_kind(kind), offset, &u.o) :
			parse_single_field(ctx, o, fname, kind, offset, &u.o);
		if (!ok)
			return false;
	}
	return true;
}

static bool
parse_msg(struct ctx *ctx, struct nbuf_obj *o, nbuf_MsgDef mdef)
{
	o->buf = ctx->buf;
	o->ssize = nbuf_MsgDef_ssize(mdef);
	o->psize = nbuf_MsgDef_psize(mdef);
	if (!nbuf_alloc_obj(o))
		return false;
	return parse_alloced_msg(ctx, o, mdef);
}

bool nbuf_parse(struct nbuf_parse_opt *opt, struct nbuf_obj *o,
	const char *input, size_t input_len, nbuf_MsgDef mdef)
{
	struct ctx ctx[1] = {{
		.buf = opt->outbuf,
		.depth = 0,
		.max_depth = (opt->max_depth > 0) ? opt->max_depth : 500,
	}};
	bool rc = false;
	size_t oldlen = ctx->buf->len;

	nbuf_lexinit(ctx->l,
		opt->filename ? opt->filename : "<string>",
		input, input_len);
	NEXT;
	if (!parse_msg(ctx, o, mdef))
		goto err;
	EXPECT(EOF);
	rc = true;
err:
	if (!rc) {
		/* restore buffer state as if nothing happened. */
		ctx->buf->len = oldlen;
	}
	nbuf_clear(&ctx->strbuf);
	return rc;
}
