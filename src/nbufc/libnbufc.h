#ifndef NBUF_FIELDTAG_H_
#define NBUF_FIELDTAG_H_

#include <stdint.h>

#include "nbuf.h"
#include "nbuf_schema.nb.h"
#include "libnbuf.h"

/* Schema compiler: compile a text schema into a nbuf.Schema object
 * in the buffer. */
struct nbufc_compile_opt {
	struct nbuf *outbuf;
};
struct nbuf_schema_set *
nbufc_compile(const struct nbufc_compile_opt *opt, const char *filename);
/* Frees memory for the schema set returned from compiler. */
void nbufc_free_compiled(const struct nbufc_compile_opt *opt);

struct nbufc_codegen_opt {
	int dummy;
};
int nbufc_codegen_c(const struct nbufc_codegen_opt *opt, struct nbuf_schema_set *ss);
int nbufc_decode_raw(FILE *out, FILE *in);

#endif  /* NBUF_FIELDTAG_H_ */
