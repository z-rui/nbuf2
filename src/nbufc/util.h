#ifndef NBUFC_UTIL_H_
#define NBUFC_UTIL_H_

size_t nbufc_baselen(const char *p);

#pragma GCC visibility push(hidden)
void nbufc_cstrdump(FILE *f, const char *base, size_t len);
void nbufc_out_path_ident(FILE *f, const char *s);
FILE *nbufc_search_open(struct nbuf_buf *buf,
	const char *const dirs[], const char *filename);
#pragma GCC visibility pop

#endif  /* NBUFC_UTIL_H_ */
