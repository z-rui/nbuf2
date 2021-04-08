#ifndef NBUF_FIELDTAG_H_
#define NBUF_FIELDTAG_H_

#include <stdint.h>

#include "nbuf.h"
#include "nbuf_schema.nb.h"
#include "libnbuf.h"

/* Schema compiler: compile a text schema into a nbuf.Schema object
 * in the buffer. */
struct nbufc_compile_opt {
	struct nbuf_buf *outbuf;
	/* NULL-terminated search paths.
	 * The provided filename is tried first.
	 * If filename begins with '/', no other attempts are made.
	 * Otherwise, if it cannot be opened, then each search path
	 * is prepended to the filename.  The first successfully opened
	 * file will be compiled.
	 *
	 * Set this to NULL will disable import statements.
	 */
	const char *const *search_path;
};
struct nbuf_schema_set *
nbufc_compile(const struct nbufc_compile_opt *opt, const char *filename);
struct nbuf_schema_set *
nbufc_compile_str(const struct nbufc_compile_opt *opt,
	const char *input, size_t input_len, const char *filename);
/* Frees memory for the schema set returned from compiler. */
void nbufc_free_compiled(const struct nbufc_compile_opt *opt);

struct nbufc_codegen_opt {
	int dummy;
};
int nbufc_codegen_c(const struct nbufc_codegen_opt *opt, struct nbuf_schema_set *ss);
int nbufc_codegen_cpp(const struct nbufc_codegen_opt *opt, struct nbuf_schema_set *ss);
int nbufc_decode_raw(FILE *out, FILE *in);

#endif  /* NBUF_FIELDTAG_H_ */
