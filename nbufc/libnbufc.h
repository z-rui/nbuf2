#ifndef NBUF_LIBNBUFC_H_
#define NBUF_LIBNBUFC_H_

#include <stdio.h>

#include "nbuf.h"
#include "libnbuf.h"

struct nbufc_codegen_opt {
	int dummy;
};
int nbufc_codegen_c(const struct nbufc_codegen_opt *opt, struct nbuf_schema_set *ss);
int nbufc_codegen_cpp(const struct nbufc_codegen_opt *opt, struct nbuf_schema_set *ss);
int nbufc_decode_raw(FILE *out, FILE *in);

#endif  /* NBUF_LIBNBUFC_H_ */
