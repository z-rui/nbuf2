#include "libnbuf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#if _POSIX_C_SOURCE
# include <fcntl.h>
# include <sys/mman.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

size_t nbuf_load_file(struct nbuf *buf, const char *filename)
{
#if _POSIX_MAPPED_FILES && !defined __SANITIZE_ADDRESS__
	int fd;
	struct stat statbuf;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror(filename);
		return 0;
	}
	if (fstat(fd, &statbuf) == -1) {
		perror(filename);
		close(fd);
		return 0;
	}
	buf->len = statbuf.st_size;
	buf->cap = 0;
	buf->base = buf->len ?
		(char *) mmap(NULL, buf->len, PROT_READ, MAP_SHARED, fd, 0) :
		NULL;
	close(fd);
	if (buf->base == MAP_FAILED) {
		buf->base = NULL;
		perror("mmap");
		return 0;
	}
#else  /* portable C */
	FILE *f;

	f = fopen(filename, "r");
	if (!f) {
		perror(filename);
		return 0;
	}
	fseek(f, 0, SEEK_END);
	buf->base = NULL;
	buf->len = ftell(f);
	buf->cap = 0;
	if (buf->len > 0 && (buf->base = malloc(buf->len)) != NULL) {
		rewind(f);
		buf->len = fread(buf->base, 1, buf->len, f);
		if (buf->len == 0) {
			free(buf->base);
			buf->base = NULL;
		}
	}
#endif
	return buf->len;
}

void nbuf_unload_file(struct nbuf *buf)
{
#if _POSIX_MAPPED_FILES && !defined __SANITIZE_ADDRESS__
	if (buf->base && munmap(buf->base, buf->len) == -1)
		perror("munmap");
#else
	free(buf->base);
#endif
	buf->base = NULL;
	buf->len = buf->cap = 0;
}

size_t nbuf_save_file(struct nbuf *buf, const char *filename)
{
	FILE *f;
	int rc = 0;

	f = fopen(filename, "wb");
	if (!f) {
		perror("fopen");
		return 0;
	}
	if (fwrite(buf->base, 1, buf->len, f) != buf->len) {
		perror("fwrite");
	} else {
		rc = buf->len;
	}
	fclose(f);
	return rc;
}

size_t nbuf_unescape(struct nbuf *buf, const char *s, size_t len)
{
	size_t oldlen = buf->len;
	const char *end = s + len;

	while (s < end) {
		int n;
		int base;
		char *p;

		if (!(p = nbuf_alloc(buf, 1)))
			goto nomem;
		if (*s != '\\') {
			*p = *s++;
			continue;
		}
		++s;  /* skip '\\' */

		n = 0;
		switch (end - s) {
		default:  /* >= 3 */
			if ((*s == 'x') && isxdigit(s[1]) && isxdigit(s[2])) {
				/* \xxx */
				++s;  /* skip 'x' */
				n = 2;
				base = 16;
				break;
			}
			/* fallthrough all the way down */
			if ('0' <= s[n] && s[n] <= '7') ++n;
		case 2:
			if ('0' <= s[n] && s[n] <= '7') ++n;
		case 1:
			if ('0' <= s[n] && s[n] <= '7') ++n;
			if (n > 0)  /* \ooo */
				base = 8;
		case 0:
			break;
		}
		if (n > 0) {
			char buf[4];
			memcpy(buf, s, n);
			/* need to cut off after n digits */
			buf[n] = '\0';
			s += n;
			*(unsigned char *) p = (unsigned char) strtol(buf, NULL, base);
			continue;
		}
		switch (*s) {
		case 'a': *p = '\a'; break;
		case 'b': *p = '\b'; break;
		case 'f': *p = '\f'; break;
		case 'n': *p = '\n'; break;
		case 'r': *p = '\r'; break;
		case 't': *p = '\t'; break;
		case 'v': *p = '\v'; break;
		default: *p = *s; break;
		}
		++s;
	}
	if (!nbuf_add1(buf, '\0'))
		goto nomem;
	return buf->len - oldlen;
nomem:
	return 0;
}

void nbuf_print_escaped(FILE *f, const char *s, size_t len, unsigned flags)
{
	size_t max_col = flags;
	size_t col = 0;

	putc('"', f);
	while (len--) {
		unsigned char ch = *s++;

		if (max_col && col + 6 > max_col) {
			fputs("\"\n\"", f);
			col = 0;
		}
		switch (ch) {
		case '\a': ch = 'a'; goto char_esc;
		case '\b': ch = 'b'; goto char_esc;
		case '\f': ch = 'f'; goto char_esc;
		case '\n': ch = 'n'; goto char_esc;
		case '\r': ch = 'r'; goto char_esc;
		case '\t': ch = 't'; goto char_esc;
		case '\v': ch = 'v'; goto char_esc;
		case '"': case '\\':
char_esc:
			putc('\\', f);
			putc(ch, f);
			col += 2;
			break;
		default:
			if (isprint(ch)) {
				putc(ch, f);
				col++;
			} else {
				int n = fprintf(f,
					(len && isdigit(*s)) ?
					"\\%03o" : "\\%o", ch);;
				col += (n <= 0) ? 1 : n;
			}
			break;
		}
	}
	putc('"', f);
}

