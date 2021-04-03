#include "config.h"
#include "nbuf.h"
#include "util.h"
#include "libnbufc.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

size_t nbufc_baselen(const char *p)
{
	const char *s, *dot = NULL;
	for (s = p; *s; ++s) {
		if (*s == '/')
			dot = NULL;
		else if (*s == '.')
			dot = s;
	}
	return (dot ? dot : s) - p;
}

#pragma GCC visibility push(hidden)

void nbufc_out_path_ident(FILE *f, const char *s)
{
	char ch;

	while ((ch = *s++) != '\0')
		isalnum(ch) ? putc(ch, f) : fprintf(f, "_%02x", ch);
}

bool nbuf_fileid(struct FileId *file_id, const char *path)
{
#if HAVE_UNISTD_H
	struct stat statbuf;

	if (stat(path, &statbuf) == -1) {
		perror(path);
		return false;
	}
	file_id->dev = statbuf.st_dev;
	file_id->ino = statbuf.st_ino;
#else
	file_id->path = path;
#endif
	return true;
}

bool nbuf_samefile(struct FileId x, struct FileId y)
{
#if HAVE_UNISTD_H
	return x.dev == y.dev && x.ino == y.ino;
#else
	return strcmp(x.path, y.path) == 0;
#endif
}

#pragma GCC visibility pop
