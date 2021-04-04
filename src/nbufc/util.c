#include "nbuf.h"
#include "util.h"
#include "libnbufc.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

void nbufc_out_path_ident(FILE *f, const char *s)
{
	char ch;

	while ((ch = *s++) != '\0')
		isalnum(ch) ? putc(ch, f) : fprintf(f, "_%02x", ch);
}

FILE *nbufc_search_open(struct nbuf_buf *buf, const char *const dirs[], const char *filename)
{
	size_t filename_len = strlen(filename);
	const char *dir;
	size_t dir_len;
	char *p;
	FILE *f;

	buf->len = 0;
	if ((p = nbuf_alloc(buf, filename_len + 1)) == NULL)
		return NULL;
	memcpy(p, filename, filename_len + 1);
	if ((f = fopen(buf->base, "r")) != NULL)
		return f;
	if (filename[0] == '/' || !dirs)
		return NULL;
	while ((dir = *dirs++) != NULL) {
		buf->len = 0;
		dir_len = strlen(dir);
		if ((p = nbuf_alloc(buf, dir_len + filename_len + 2)) == NULL)
			return NULL;
		memcpy(p, dir, dir_len); p += dir_len;
		if (dir_len && p[-1] != '/')
			*p++ = '/';
		memcpy(p, filename, filename_len + 1);
		if ((f = fopen(buf->base, "r")) != NULL)
			return f;
	}
	return NULL;
}
