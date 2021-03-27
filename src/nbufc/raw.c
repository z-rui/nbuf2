#include "nbuf.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

/* decode_raw functionality. */

static inline bool read_word(FILE *f, nbuf_word_t *w)
{
	if (fread(w, sizeof *w, 1, f) != 1)
		return false;
	*w = nbuf_word(w);
	return true;
}


static bool hexdump(FILE *out, FILE *in, size_t offset, size_t len)
{
	size_t i, j;
	char buf[16];

	for (i = 0; i < len; i++) {
		int ch = getc(in);

		j = i % sizeof buf;
		if (ch == EOF)
			return false;
		buf[j] = isprint(ch) ? ch : '.';
		if (j == 0)
			fprintf(out, " %08lx:", (unsigned long) offset);
		fprintf(out, " %02x", ch);
		if (j == sizeof buf - 1)
			fprintf(out, "  %.*s\n", (int) sizeof buf, buf);
		++offset;
	}
	if ((j = i % sizeof buf) != 0) {
		fprintf(out, "%*s  %.*s", (int) (sizeof buf - j) * 3, "",
			(int) j, buf);
		putc('\n', out);
	}
	return true;
}

int nbufc_decode_raw(FILE *out, FILE *in)
{
	int rc = 1;

	nbuf_word_t hdr;
	size_t offset = 0;
	while (read_word(in, &hdr)) {
		nbuf_word_t len = 1;
		unsigned ssize = NBUF_SSIZE(hdr), psize = NBUF_PSIZE(hdr);
		unsigned i;
		
		printf("*%08lx: hdr=%08x", (unsigned long) offset, hdr);
		offset += sizeof hdr;
		if (!(hdr & NBUF_HDR_MASK)) {
			printf(" BAD\n");
			goto err;
		}
		if (hdr & NBUF_ARR_MASK) {
			if (!read_word(in, &len))
				goto err;
			offset += sizeof len;
			if (len & NBUF_HDR_MASK)
				goto err;
			printf(" len: 0x%x", len);
		} else if (hdr & NBUF_BARR_MASK) {
			len = hdr & NBUF_BLEN_MASK;
			ssize = 1;
			psize = 0;
			printf(" len: 0x%x", len);
		}
		printf(" p: 0x%x, s: 0x%x\n", psize, ssize);
		if (hdr & NBUF_BARR_MASK) {
			if (!hexdump(out, in, offset, len))
				goto err;
			offset += len;
		} else {
			for (i = 0; i < len; i++) {
				unsigned j;

				for (j = 0; j < psize; j++) {
					nbuf_word_t ptr;
					if (!read_word(in, &ptr))
						goto err;
					printf(" %08lx: [%u] -> ", (unsigned long) offset, j);
					if (ptr == 0)
						printf("null\n");
					else
						printf("%08lx [+%u]\n",
							(unsigned long) (offset + ptr * sizeof (nbuf_word_t)),
							ptr);
					offset += sizeof (nbuf_word_t);
				}
				if (!hexdump(out, in, offset, ssize))
					goto err;
				offset += ssize;
			}
		}
		while (offset % sizeof (nbuf_word_t)) {
			if (getchar() == EOF)
				break;
			++offset;
		}
	}
	rc = 0;
err:
	return rc;
}
