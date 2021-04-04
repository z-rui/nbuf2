#include "libnbuf.h"
#include "libnbufc.h"
#include "util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
# include <fcntl.h>
# include <io.h>
#endif

struct ctx {
	const char *progname;
	struct nbuf_schema_set *ss;
};

static void
usage(struct ctx *ctx, bool quit)
{
	fprintf(stderr, "usage: %s [options] [schema]\n", ctx->progname);
	fprintf(stderr, "Options\n"
		"  -help     show this help\n"
		"  -I=<dir>  add directory to search path\n"
		"  -c_out    compile schema and generate C source files\n"
		"  -cpp_out  compile schema and generate C++ source files\n"
		"  -encode=<msg_type>\n"
		"            encode a text message into binary\n"
		"  -decode=<msg_type>\n"
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
cpp_out(struct ctx *ctx, const char *path)
{
	struct nbufc_codegen_opt opt;

	memset(&opt, 0, sizeof opt);
	return nbufc_codegen_cpp(&opt, ctx->ss);
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
	struct nbuf_print_opt opt = {
		.f = stdout,
		.indent = 2,
		.max_depth = 0,
		.msg_type_hdr = true,
		.loose_escape = true,
	};
	struct nbuf_buf buf;
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
#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
#endif
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
	struct nbuf_buf buf, outbuf;
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
#ifdef _WIN32
	_setmode(_fileno(stdout), _O_BINARY);
#endif
	if (nbuf_save_fp(&outbuf, stdout))
		rc = 0;
err:
	nbuf_clear(&outbuf);
	nbuf_unload_file(&buf);
	if (rc)
		fprintf(stderr, "encode failed\n");
	return rc;
}

#define ARG0(X, Y) if (strcmp(arg, X) == 0) { Y; }
#define ARG1(X, Y) { \
	size_t _len = strlen(X); \
	if (strncmp(arg, X, _len) == 0 && (arg[_len] == '\0' || arg[_len] == '=')) { \
		arg = arg[_len] ? &arg[_len+1] : *argv++; \
		if (arg == NULL) \
			goto missing_arg; \
		Y; \
	} \
}

#define MAXINCDIR 63

int main(int argc, char *argv[])
{
	struct ctx ctx[1];
	const char *arg;
	const char *msg_type = NULL;
	enum {
		NONE, C_OUT, CPP_OUT, BIN_OUT, DECODE, DECODE_RAW, ENCODE,
	} action = NONE;
	struct nbuf_buf outbuf = {NULL};
	struct nbufc_compile_opt opt = {
		.outbuf = &outbuf,
	};
	int rc = 1;
	const char *search_path[MAXINCDIR+1];
	size_t search_path_count = 0;

	memset(ctx, 0, sizeof ctx);
	ctx->progname = *argv++;
	while ((arg = *argv++)) {
		if (*arg != '-' || (*++arg == '-' && *++arg == '\0')) {
			arg = *argv;
			goto end_of_opt;
		}
		ARG0("c_out", action = C_OUT; break);
		ARG0("cpp_out", action = CPP_OUT; break);
		ARG0("bin_out", action = BIN_OUT; break);
		ARG1("decode", action = DECODE; msg_type = arg; break);
		ARG0("decode_raw", action = DECODE_RAW; goto skip_schema);
		ARG1("encode", action = ENCODE; msg_type = arg; break);
		ARG1("I", {
			if (search_path_count >= MAXINCDIR) {
				fprintf(stderr, "too many -I options\n");
				return 1;
			}
			search_path[search_path_count++] = arg;
			continue;
		});
		ARG0("help", show_usage: usage(ctx, true));
		fprintf(stderr, "unknown option -%s\n", arg);
		goto show_usage;
missing_arg:
		fprintf(stderr, "missing argument for option -%s\n", arg);
		goto show_usage;
	}
	if (arg && (arg = *argv) && *arg == '-') {
		fprintf(stderr, "extra option %s\n", arg);
		goto show_usage;
	}
end_of_opt:
	if (arg == NULL) {
		fprintf(stderr, "missing schema\n");
		goto show_usage;
	}
	search_path[search_path_count] = NULL;
	opt.search_path = search_path;
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
	case CPP_OUT:
		rc = cpp_out(ctx, arg);
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
