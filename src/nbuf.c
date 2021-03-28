#include "nbuf.h"

#include <stdio.h>
#include <stdlib.h>

char *
nbuf_init_rw(struct nbuf *buf, size_t cap)
{
	if (cap == 0)
		cap = 1;
	buf->base = (char *) malloc(cap);
	if (buf->base) {
		memset(buf->base, 0, cap);
		buf->len = 0;
		buf->cap = cap;
	} else {
		buf->len = buf->cap = 0;
	}
	return buf->base;
}

void nbuf_clear(struct nbuf *buf)
{
	if (buf->cap)
		free(buf->base);
	buf->base = NULL;
	buf->len = buf->cap = 0;
}

char *nbuf_alloc_ex(struct nbuf *buf, size_t newlen)
{
	size_t newcap;
	char *newbase;

	/* Buffer must be writable. */
	assert(!(buf->base != NULL && buf->cap == 0));
	newcap = buf->cap;
	if (newcap < sizeof (nbuf_word_t))
		newcap = sizeof (nbuf_word_t);
	for (; newcap < newlen; newcap += newcap / 2)
		;
	newbase = (char *) realloc(buf->base, newcap);
	if (newbase == NULL) {
		fprintf(stderr, "nbuf: realloc() failed\n");
		return NULL;
	}
	memset(newbase + buf->len, 0, newcap - buf->len);
	buf->base = newbase;
	buf->cap = newcap;
	newbase += buf->len;
	buf->len = newlen;
	return newbase;
}

size_t
nbuf_fix_arr(struct nbuf_obj *o, nbuf_word_t len,
	const struct nbuf *newbuf)
{
	nbuf_word_t *p = NULL;
	struct nbuf_obj it = *o;
	size_t i, j;

	if (newbuf->len > 0) {
		p = (nbuf_word_t *) nbuf_alloc_aligned(o->buf,
			newbuf->len, sizeof (nbuf_word_t));
		if (!p) return 0;
		memcpy(p, newbuf->base, newbuf->len);
	}
	for (i = 0; i < len; i++) {
		nbuf_word_t *pptr = (nbuf_word_t *) nbuf_obj_base(&it);
		for (j = 0; j < it.psize; j++) {
			nbuf_word_t rel_ptr = nbuf_word(pptr);
			if (rel_ptr == 0)
				continue;
			assert(p != NULL);
			rel_ptr = ~rel_ptr + (p - pptr);
			assert((size_t) ((char *) (pptr + rel_ptr) - it.buf->base) < it.buf->len);
			*pptr++ = nbuf_word(&rel_ptr);
		}
		nbuf_next(&it);
	}
	return len;
}

size_t
nbuf_resize_arr(struct nbuf_obj *o, nbuf_word_t len)
{
	size_t hdr_offset = nbuf_obj_hdr_offset(o);
	nbuf_word_t hdr;

	assert(hdr_offset + sizeof hdr <= o->buf->len);
	hdr = nbuf_word(o->buf->base + hdr_offset);
	assert(hdr & NBUF_HDR_MASK);

	/* Only array at the end of the buffer can be resized. */
	assert((char *) nbuf_obj_base(o) + len * nbuf_obj_size(o) ==
		o->buf->base + o->buf->len);
	if (hdr & NBUF_BARR_MASK) {
		if ((len & NBUF_BLEN_MASK) != len)
			return 0;
		hdr = (hdr &~ NBUF_BLEN_MASK) | len;
		*(nbuf_word_t *) (o->buf->base + hdr_offset) = nbuf_word(&hdr);
	} else {
		if ((len &~ NBUF_HDR_MASK) != len)
			return 0;
		hdr_offset += sizeof (hdr);
		assert(hdr_offset + sizeof (nbuf_word_t) <= o->buf->len);
		*(nbuf_word_t *) (o->buf->base + hdr_offset) = nbuf_word(&len);
	}
	return len;
}
