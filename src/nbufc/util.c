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
	size_t last_dir_len = 0;
	FILE *f;

	buf->len = 0;
	if (!nbuf_alloc(buf, filename_len + 1))
		return NULL;
	memcpy(buf->base, filename, filename_len + 1);
	if ((f = fopen(buf->base, "r")) != NULL)
		return f;
	if (filename[0] == '/' || !dirs)
		return NULL;
	while ((dir = *dirs++) != NULL) {
		size_t dir_len = strlen(dir);
		size_t newlen;

		if (dir_len && dir[dir_len-1] == '/')
			--dir_len;
		newlen = dir_len + filename_len + 2;
		if (newlen > buf->len && !nbuf_alloc(buf, newlen - buf->len))
			return NULL;
		buf->len = newlen;
		memmove(buf->base + dir_len + 1, buf->base + last_dir_len, filename_len + 1);
		memcpy(buf->base, dir, dir_len);
		buf->base[dir_len] = '/';
		if ((f = fopen(buf->base, "r")) != NULL)
			return f;
		last_dir_len = dir_len + 1;  /* including trailing '/' */
	}
	return NULL;
}
