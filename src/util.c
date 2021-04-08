#include "config.h"
#include "libnbuf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#if defined _WIN32
# include <io.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <windows.h>
#elif HAVE_UNISTD_H
# include <fcntl.h>
# if HAVE_MMAP
#  include <sys/mman.h>
# endif
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

#if HAVE_UNISTD_H || defined _WIN32
static size_t nbuf_load_fd_read(struct nbuf_buf *buf, int fd)
{
	size_t readsz;
	ssize_t n;

	nbuf_init_ex(buf, 0);
	do {
		if (!nbuf_alloc(buf, BUFSIZ))
			goto err;
		buf->len -= BUFSIZ;
		readsz = buf->cap - buf->len;
#ifdef _WIN32
		n = _read(fd, buf->base + buf->len, readsz);
#else
		n = read(fd, buf->base + buf->len, readsz);
#endif
		if (n == -1) {
			perror("read");
			goto err;
		}
		buf->len += n;
	} while (n);
	return buf->len;
err:
	nbuf_clear(buf);
	return 0;
}
#endif

#if HAVE_UNISTD_H || defined _WIN32
size_t nbuf_load_fd(struct nbuf_buf *buf, int fd)
{
#ifdef _WIN32
	struct _stat statbuf;
	if (_fstat(fd, &statbuf) == -1) {
		perror("_fstat");
		return 0;
	}
#else
	struct stat statbuf;
	if (fstat(fd, &statbuf) == -1) {
		perror("fstat");
		return 0;
	}
#endif

	buf->base = NULL;
	buf->len = statbuf.st_size;
	buf->cap = 0;
#ifndef __SANITIZE_ADDRESS__
	/* Will not ues mmap if using sanitizers,
	 * so memory leak will be detected.
	 */
#ifdef _WIN32
	if (buf->len && S_ISREG(statbuf.st_mode)) {
		HANDLE hFile, hFileMapping;
		hFile = (HANDLE) _get_osfhandle(fd);
		if (hFile == INVALID_HANDLE_VALUE)
			goto bad_handle;
		hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
		if (hFileMapping == INVALID_HANDLE_VALUE)
			goto bad_handle;
		buf->base = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
		CloseHandle(hFileMapping);
		if (buf->base == NULL) {
bad_handle:
			buf->base = NULL;
			buf->len = 0;
		}
		return buf->len;
	}
#elif HAVE_MMAP
	if (buf->len && S_ISREG(statbuf.st_mode)) {
		buf->base = mmap(NULL, buf->len, PROT_READ, MAP_SHARED, fd, 0);
		if (buf->base == MAP_FAILED) {
			buf->base = NULL;
			buf->len = 0;
			perror("mmap");
		}
		return buf->len;
	}
#endif
#endif  /* __SANITIZE_ADDRESS__ */
	return nbuf_load_fd_read(buf, fd);
}
#endif

size_t nbuf_load_fp(struct nbuf_buf *buf, FILE *f)
{
#if defined _WIN32
	return nbuf_load_fd(buf, _fileno(f));
#elif HAVE_UNISTD_H
	return nbuf_load_fd(buf, fileno(f));
#else
	/* generic implementation */
	int ch;

	if (!nbuf_init_ex(buf, BUFSIZ))
		return 0;
	while ((ch = getc(f)) != EOF)
		if (!nbuf_add1(buf, ch))
			goto err;
	if (ferror(f)) {
		perror("nbuf_load_fp");
err:
		nbuf_clear(buf);
		return 0;
	}
#endif
	return buf->len;
}

size_t nbuf_load_file(struct nbuf_buf *buf, const char *filename)
{
	FILE *f;
	size_t rc = 0;

	f = fopen(filename, "r");
	if (!f) {
		perror(filename);
		return 0;
	}
	rc = nbuf_load_fp(buf, f);
	fclose(f);
	return rc;
}

void nbuf_unload_file(struct nbuf_buf *buf)
{
#ifdef _WIN32
	if (buf->base != NULL && buf->cap == 0) {
		/* Mapped File */
		if (!UnmapViewOfFile(buf->base))
			perror("nbuf_unload_file: cannot unmap");
	} else
#elif HAVE_MMAP
	if (buf->base != NULL && buf->cap == 0) {
		/* mmap-ed region */
		if (munmap(buf->base, buf->len) == -1)
			perror("nbuf_unload_file: cannot unmap");
	} else
#endif
	free(buf->base);
	buf->base = NULL;
	buf->len = buf->cap = 0;
}

#if HAVE_UNISTD_H || defined _WIN32
size_t nbuf_save_fd(struct nbuf_buf *buf, int fd)
{
	if (write(fd, buf->base, buf->len) != buf->len) {
		perror("write");
		return 0;
	}
	return buf->len;
}
#endif

size_t nbuf_save_fp(struct nbuf_buf *buf, FILE *f)
{
	if (fwrite(buf->base, 1, buf->len, f) != buf->len) {
		perror("fwrite");
		return 0;
	}
	return buf->len;
}

size_t nbuf_save_file(struct nbuf_buf *buf, const char *filename)
{
	FILE *f;
	int rc = 0;

	f = fopen(filename, "wb");
	if (!f) {
		perror("fopen");
		return 0;
	}
	rc = nbuf_save_fp(buf, f);
	fclose(f);
	return rc;
}

size_t nbuf_unescape(struct nbuf_buf *buf, const char *s, size_t len)
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
	bool loose_escape = flags & NBUF_PRINT_LOOSE_ESCAPE;
	size_t max_col = (flags &= ~NBUF_PRINT_LOOSE_ESCAPE);
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
			if (isprint(ch) || (loose_escape && (ch & 0x80))) {
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

size_t nbuf_baselen(const char *p)
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
