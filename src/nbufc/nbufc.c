#include "libnbuf.h"
#include "libnbufc.h"
#include "util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct ctx {
	const char *progname;
	struct nbuf_schema_set *ss;
};

static void
usage(struct ctx *ctx, bool quit)
{
	fprintf(stderr, "usage: %s [options] [schema]\n", ctx->progname);
	fprintf(stderr, "Options\n"
		"  -c_out    compile schema and generate C source files\n"
		"  -encode <msg_type>\n"
		"            encode a text message into binary\n"
		"  -decode <msg_type>\n"
		"            decode a binary message into text\n"
		"  -decode_raw\n"
		"            dump a binary message in raw format "
		"(schema is not needed)\n");
	if (quit)
		exit(1);
}

/* Removes extension (if any) from in_filename, and appends suffix.
 */
static char *construct_out_filename(const char *in_filename, const char *suffix)
{
	char *p;
	size_t len;

	len = nbufc_baselen(in_filename);
	p = (char *) malloc(len + strlen(suffix) + 1);
	if (!p)
		return NULL;
	memcpy(p, in_filename, len);
	strcpy(p + len, suffix);
	return p;
}

static int
c_out(struct ctx *ctx, const char *path)
{
	struct nbufc_codegen_opt opt;

	memset(&opt, 0, sizeof opt);
	return nbufc_codegen_c(&opt, ctx->ss);
}

static int
bin_out(struct ctx *ctx, const char *path)
{
	char *newpath;

	newpath = construct_out_filename(path, ".nb");
	if (!newpath) {
		fprintf(stderr,
			"internal error: cannot construct output filename\n");
		return 1;
	}
	if (!nbuf_save_file(&ctx->ss->buf, newpath)) {
		fprintf(stderr, "cannot write to %s", newpath);
		return 1;
	}
	fprintf(stderr, "%s\n", newpath);
	free(newpath);
	return 0;
}

static int
decode(struct ctx *ctx, const char *msg_type)
{
	nbuf_MsgDef mdef;
	struct nbuf_print_opt opt;
	struct nbuf buf;
	struct nbuf_obj o;
	nbuf_Schema schema;
	nbuf_Kind kind;
	unsigned type_id;

	if (!nbuf_get_Schema(&schema, &ctx->ss->buf, 0)) {
		fprintf(stderr, "error: cannot load schema\n");
		return 1;
	}
	if (!nbuf_lookup_defined_type(schema, msg_type, &kind, &type_id) ||
		kind != nbuf_Kind_MSG ||
		!nbuf_Schema_messages(&mdef, schema, type_id)) {
		fprintf(stderr, "error: '%s' is not a message type name\n", msg_type);
		return 1;
	}
	if (!nbuf_load_fp(&buf, stdin)) {
		fprintf(stderr, "error: cannot read input\n");
		return 1;
	}
	o.buf = &buf;
	o.offset = 0;
	if (!nbuf_get_obj(&o)) {
		fprintf(stderr, "error: cannot get root object\n");
		goto err;
	}
	opt.f = stdout;
	opt.indent = 2;
	opt.max_depth = 0;
	opt.msg_type_hdr = true;
	if (!nbuf_print(&opt, &o, mdef))
		goto err;
	nbuf_unload_file(&buf);
	return 0;
err:
	nbuf_unload_file(&buf);
	fprintf(stderr, "decode failed\n");
	return 1;
}

static int
encode(struct ctx *ctx, const char *msg_type)
{
	nbuf_MsgDef mdef;
	struct nbuf buf, outbuf;
	int rc = 1;
	nbuf_Schema schema;
	nbuf_Kind kind;
	unsigned type_id;
	struct nbuf_obj dummy;
	struct nbuf_parse_opt opt = {
		.outbuf = &outbuf,
		.max_depth = 500,
		.filename = "<stdin>",
	};

	memset(&buf, 0, sizeof buf);
	memset(&outbuf, 0, sizeof outbuf);

	if (!nbuf_get_Schema(&schema, &ctx->ss->buf, 0)) {
		fprintf(stderr, "cannot load schema\n");
		return 1;
	}
	if (!nbuf_lookup_defined_type(schema, msg_type, &kind, &type_id) ||
		kind != nbuf_Kind_MSG ||
		!nbuf_Schema_messages(&mdef, schema, type_id)) {
		fprintf(stderr, "'%s' is not a message type name\n", msg_type);
		return 1;
	}
	if (!nbuf_load_fp(&buf, stdin)) {
		fprintf(stderr, "cannot read input\n");
		goto err;
	}
	nbuf_init_rw(&outbuf, 4096);
	if (!nbuf_parse(&opt, &dummy, buf.base, buf.len, mdef))
		goto err;
	if (fwrite(outbuf.base, 1, outbuf.len, stdout) == outbuf.len)
		rc = 0;
err:
	nbuf_clear(&outbuf);
	nbuf_unload_file(&buf);
	if (rc)
		fprintf(stderr, "encode failed\n");
	return rc;
}

int main(int argc, char *argv[])
{
	struct ctx ctx[1];
	const char *arg;
	const char *msg_type = NULL;
	enum {
		NONE, C_OUT, BIN_OUT, DECODE, DECODE_RAW, ENCODE,
	} action = NONE;
	struct nbuf outbuf = {NULL};
	struct nbufc_compile_opt opt = {
		.outbuf = &outbuf,
	};
	int rc = 1;

	memset(ctx, 0, sizeof ctx);
	ctx->progname = *argv++;
	while ((arg = *argv++)) {
		if (*arg != '-')
			goto end_of_opt;
		++arg;
		if (*arg == '-' && *++arg == '\0')
			goto end_of_opt;
		if (strcmp(arg, "c_out") == 0) {
			action = C_OUT;
			break;
		} else if (strcmp(arg, "bin_out") == 0) {
			action = BIN_OUT;
			break;
		} else if (strcmp(arg, "decode") == 0) {
			action = DECODE;
			if (!(msg_type = *argv++)) {
				fprintf(stderr, "missing value for option -decode\n");
				usage(ctx, true);
			}
			break;
		} else if (strcmp(arg, "decode_raw") == 0) {
			action = DECODE_RAW;
			goto skip_schema;
		} else if (strcmp(arg, "encode") == 0) {
			action = ENCODE;
			if (!(msg_type = *argv++)) {
				fprintf(stderr, "missing value for option -encode\n");
				usage(ctx, true);
			}
			break;
		} else {
			fprintf(stderr, "unknown option -%s\n", arg);
			usage(ctx, true);
		}
	}
	if (arg && (arg = *argv) && *arg == '-') {
		fprintf(stderr, "extra option %s\n", arg);
		usage(ctx, true);
	}
end_of_opt:
	if (arg == NULL) {
		fprintf(stderr, "missing schema\n");
		usage(ctx, true);
	}
	ctx->ss = nbufc_compile(&opt, arg);
	if (!ctx->ss) {
		fprintf(stderr, "%s: compilation failed\n", arg);
		goto out;
	}
skip_schema:
	switch (action) {
	case C_OUT:
		rc = c_out(ctx, arg);
		break;
	case BIN_OUT:
		rc = bin_out(ctx, arg);
		break;
	case DECODE_RAW:
		rc = nbufc_decode_raw(stdout, stdin);
		break;
	case DECODE:
		rc = decode(ctx, msg_type);
		break;
	case ENCODE:
		rc = encode(ctx, msg_type);
		break;
	default:
		fprintf(stderr, "no errors found.\n");
		rc = 0;
		break;
	}
out:
	nbufc_free_compiled(&opt);
	return rc;
}
