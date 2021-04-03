#ifndef NBUFC_UTIL_H_
#define NBUFC_UTIL_H_

#include <stdbool.h>

#ifdef HAVE_UNISTD_H
# include <sys/stat.h>
#endif

struct FileId {
#if HAVE_UNISTD_H
	dev_t dev;
	ino_t ino;
#else
	/* Generic implmentation - use path only
	 * Does not own the string.
	 */
	const char *path;
#endif
};

bool nbuf_fileid(struct FileId *file_id, const char *path);
bool nbuf_samefile(struct FileId, struct FileId);
size_t nbufc_baselen(const char *p);
void nbufc_cstrdump(FILE *f, const char *base, size_t len);
void nbufc_out_path_ident(FILE *f, const char *s);


#endif  /* NBUFC_UTIL_H_ */
