#ifndef _NBUF_H_
#define _NBUF_H_

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* We use little endian as the serialized integer formst.
 * Thus, on a big-endian platform, we need to perform conversion
 *
 * If the nbuf_bswap{16,32,64} are not provided at compile time,
 * the following will choose an appropriate implementation from
 * GCC or MSVC.
 */

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
 #define nbuf_bswap16 __builtin_bswap16
 #define nbuf_bswap32 __builtin_bswap32
 #define nbuf_bswap64 __builtin_bswap64
#elif defined _MSC_VER
 #define nbuf_bswap16 _byteswap_ushort
 #define nbuf_bswap32 _byteswap_ulong
 #define nbuf_bswap64 _byteswap_uint64
#endif

/* If nothing is available, use the generic implementations below.
 * Note that GCC -O2 is probably clever enough to compile them into a native
 * instruction. */
#ifndef nbuf_bswap16
static inline uint16_t nbuf_bswap16(uint16_t x)
{
	return (x << 8) | (x >> 8);
}
#endif
#ifndef nbuf_bswap32
static inline uint32_t nbuf_bswap32(uint32_t x)
{
	return ((uint32_t) nbuf_bswap16(x & 0xFFFF) << 16) | nbuf_bswap16(x >> 16);
}
#endif
#ifndef nbuf_bswap64
static inline uint64_t nbuf_bswap64(uint64_t x)
{
	return ((uint64_t) nbuf_bswap32(x & 0xFFFFFFFF) << 32) | nbuf_bswap32(x >> 32);
}
#endif

/* A way to tell if we are running on big-endian platform */
#if defined __BIG_ENDIAN__
# define nbuf_is_big_endian() (1)
#elif defined __BYTE_ORDER__ && defined __ORDER_BIG_ENDIAN__
# define nbuf_is_big_endian() (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined __BYTE_ORDER && defined __BIG_ENDIAN
# define nbuf_is_big_endian() (__BYTE_ORDER == __BIG_ENDIAN)
#else
/* Generic implmentation.  Hopefully an optimizing compiler will compile
 * it into a constant. */
static inline int nbuf_is_big_endian(void)
{
  union { uint16_t x; uint8_t y; } u = { 1 };
  return !u.y;
}
#endif

/* Functions for reading an unsigned integer from ptr, and if on a big-endian platform,
 * swap endianness.
 */
static inline uint8_t nbuf_u8(const void *ptr) {
	return *(const uint8_t *) ptr;
}
static inline uint16_t nbuf_u16(const void *ptr) {
	uint16_t v = *(const uint16_t *) ptr;
	return (nbuf_is_big_endian()) ? nbuf_bswap16(v) : v;
}
static inline uint32_t nbuf_u32(const void *ptr) {
	uint32_t v = *(const uint32_t *) ptr;
	return (nbuf_is_big_endian()) ? nbuf_bswap32(v) : v;
}
static inline uint64_t nbuf_u64(const void *ptr) {
	/* uint64_t may not be aligned in the buffer. */
	uint32_t lo = nbuf_u32((const uint32_t *) ptr);
	uint32_t hi = nbuf_u32((const uint32_t *) ptr + 1);
	return ((uint64_t) hi << 32) | lo;
}
static inline void nbuf_set_u8(void *ptr, uint8_t v) {
	*(uint8_t *) ptr = v;
}
static inline void nbuf_set_u16(void *ptr, uint16_t v) {
	*(uint16_t *) ptr = (nbuf_is_big_endian()) ? nbuf_bswap16(v) : v;
}
static inline void nbuf_set_u32(void *ptr, uint32_t v) {
	*(uint32_t *) ptr = (nbuf_is_big_endian()) ? nbuf_bswap32(v) : v;
}
static inline void nbuf_set_u64(void *ptr, uint64_t v) {
	uint32_t lo = v;
	uint32_t hi = v >> 32;
	/* uint64_t may not be aligned in the buffer. */
	*(uint32_t *) ptr = nbuf_u32(&lo);
	*((uint32_t *) ptr + 1) = nbuf_u32(&hi);
}

/* Functions for reading other scalar types from ptr.
 * Note:
 * 1. signed integers are assumed to use 2's compliment; I didn't find
 *    a cheap and portable way to convert 2's compliment into signed.
 * 2. float and double are assumed to be IEEE-754 single and double precision
 *    numbers.
 * If the platform doesn't match the assumptions, they will end up with
 * an incompatible wire format.
 */
#define nbuf_scalar(prefix, bits, typ) \
static inline typ nbuf_##prefix##bits(const void *ptr) \
{ \
	union { typ x; uint##bits##_t i; } u; \
	u.i = nbuf_u##bits(ptr); \
	return u.x; \
} \
static inline void nbuf_set_##prefix##bits(void *ptr, typ v) \
{ \
	union { typ x; uint##bits##_t i; } u = { v }; \
	nbuf_set_u##bits(ptr, u.i); \
}

nbuf_scalar(i, 8, int8_t)
nbuf_scalar(i, 16, int16_t)
nbuf_scalar(i, 32, int32_t)
nbuf_scalar(i, 64, int64_t)
nbuf_scalar(f, 32, float)
nbuf_scalar(f, 64, double)

#undef nbuf_scalar

typedef uint32_t nbuf_word_t;
typedef uint32_t nbuf_word_off_t;
#define nbuf_word nbuf_u32
#define nbuf_set_word nbuf_set_u32
#define NBUF_ALLOC_ALIGN(size)  (((size) + sizeof (nbuf_word_t) - 1) &~ (sizeof (nbuf_word_t) - 1))


/** Buffer API */
struct nbuf_buf {
	char *base;
	size_t len, cap;
	char *(*realloc)(struct nbuf_buf *buf, size_t newlen);
};

/* Initializes a read-only buffer.
 * The buffer will not own memory.
 * It's undefined behavior if mutable API is used with read-only buffers.
 */
static inline void nbuf_init_ro(struct nbuf_buf *buf, const char *base, size_t len)
{
	buf->base = (char *) base;
	buf->len = len;
	buf->cap = 0;
	buf->realloc = NULL;
}

/* Initializes a read-write buffer.
 *
 * New space can be allocated at the end of the buffer by calling
 * `nbuf_alloc`.  The maximum bytes that can be allocated is `cap`.
 *
 * The memory of `cap` bytes is preallocated at `base`.
 */
static inline void nbuf_init_rw(struct nbuf_buf *buf, char *base, size_t cap)
{
	buf->base = base;
	buf->len = 0;
	buf->cap = cap;
	buf->realloc = NULL;
}

/* Clears a read-write buffer.
 * This frees the memory owned by the buffer.
 */
static inline void nbuf_clear(struct nbuf_buf *buf)
{
	if (buf->realloc) {
		buf->realloc(buf, 0);
	} else {
		buf->base = NULL;
		buf->len = buf->cap = 0;
	}
}

/* Allocates `size` bytes at the end of the buffer.
 * This may reallocate memory.  Any pointer pointing to
 * the previous buffer region may be invalidated.
 *
 * Returns a pointer at the beginning of the newly allocated
 * memory region, or NULL on failure.
 */
static inline char *nbuf_alloc(struct nbuf_buf *buf, size_t size)
{
	size_t newlen;
	char *newbase;

	newlen = buf->len + size;
	if (buf->cap < newlen)
		return buf->realloc ? buf->realloc(buf, newlen) : NULL;
	newbase = buf->base + buf->len;
	buf->len += size;
	return newbase;
}

/* Allocates `size` bytes aligned at `align` bytes.
 * `align` must be a power of 2.
 */
static inline char *
nbuf_alloc_aligned(struct nbuf_buf *buf, size_t size, size_t align)
{
	size_t pad;
	char *p;

	pad = buf->len & (align - 1);
	if (pad)
		pad = align - pad;
	if (!(p = nbuf_alloc(buf, size + pad)))
		return NULL;
	return p + pad;
}

/* Appends `len` bytes, starting at `s`, to the end of the buffer.
 */
static inline char *
nbuf_add(struct nbuf_buf *buf, const char *s, size_t len)
{
	char *p = nbuf_alloc(buf, len);
	if (!p) return NULL;
	memcpy(p, s, len);
	return p;
}

/* Appends a single byte `c` into buffer.
 */
static inline char *
nbuf_add1(struct nbuf_buf *buf, char c)
{
	char *p = nbuf_alloc(buf, 1);
	if (!p) return NULL;
	*p = c;
	return p;
}


/** Raw object API
 * An object consists of a scalar part and a pointer part.
 * The offset is the byte offset in the buffer of the first pointer,
 * or if there's no pointer, the first byte of the scalar part.
 *
 * An object may be:
 * - Single message
 * - One element in repeated messages
 * - String
 * - Repeated scalars/strings
 *
 * For a single message, the object is preceded by an object header.
 * The header specifies the scalar part size (ssize) and pointer part
 * size (psize).
 *
 * A repeated message with n elements are objects allocated contiguously
 * in the buffer, preceded by a single array header.
 * The header the array length in addition to ssize and psize.
 * The length is returned by the API calls.  A length of 0 is invalid,
 * and the caller should treat it as a non-existent.
 *
 * For repeated scalars, they are treated as a repeated message, each of
 * which has a single field of that scalar.
 *
 * A string is treated as repeated uint8, with a guaranteed trailing '\0'
 * (included in the length) for compability with C routines.  The string
 * can include '\0' character in the middle, but the user needs to get
 * the length from array length, as strlen() would return a wrong length.
 *
 * Repeated strings are treated as repeated messages with a single field of
 * string.  That is, there will be an array of pointers, each pointing to a
 * string.
 */

struct nbuf_obj {
	struct nbuf_buf *buf;
	uint32_t offset;
	uint16_t ssize, psize;
};

#define NBUF_HDR_MASK  0x80000000U
#define NBUF_BARR_MASK 0x40000000U
#define NBUF_BLEN_MASK 0x3fffffffU
#define NBUF_ARR_MASK 0x20000000U
#define NBUF_SSIZE_LSB 14
#define NBUF_SSIZE_MSB 28
#define NBUF_PSIZE_LSB 0
#define NBUF_PSIZE_MSB 13
#define NBUF_HDR(ssize, psize) (NBUF_HDR_MASK | \
	((ssize) << NBUF_SSIZE_LSB) | \
	((psize) << NBUF_PSIZE_LSB))

#define NBUF_BITFIELD(x, msb, lsb) \
	(((x) >> (lsb)) & ((2u << ((msb) - (lsb))) - 1))
#define NBUF_SSIZE(hdr) NBUF_BITFIELD(hdr, NBUF_SSIZE_MSB, NBUF_SSIZE_LSB)
#define NBUF_PSIZE(hdr) NBUF_BITFIELD(hdr, NBUF_PSIZE_MSB, NBUF_PSIZE_LSB)

static inline void *
nbuf_obj_base(const struct nbuf_obj *o)
{
	return o->buf->base + o->offset;
}

/* Returns the allocation size of an object */
static inline size_t
nbuf_obj_size(const struct nbuf_obj *o)
{
	return o->psize * sizeof (nbuf_word_t) + o->ssize;
}

/* Loads object from buffer at given offset.
 *
 * Caller should initialize buf and offset.
 * offset points to the object header, which specifies
 * ssize and psize.
 *
 * For non-repeated non-strings, returns 1 on success, 0 otherwise.
 * For repeated objects or strings, the length is also obtained from the
 * header and returned.
 */
static inline size_t
nbuf_get_obj(struct nbuf_obj *o)
{
	nbuf_word_t hdr, len;
	size_t sz, maxsz;

	if (o->offset >= o->buf->len)
		goto err;
	maxsz = o->buf->len - o->offset;

	if ((sz = sizeof hdr) > maxsz)
		goto err;
	hdr = nbuf_word(o->buf->base + o->offset);
	o->offset += sizeof hdr;
	if (!(hdr & NBUF_HDR_MASK))
		goto err;
	if (hdr & NBUF_BARR_MASK) {
		o->ssize = 1;
		o->psize = 0;
		len = hdr & NBUF_BLEN_MASK;
	} else {
		o->ssize = NBUF_SSIZE(hdr);
		o->psize = NBUF_PSIZE(hdr);
		if (hdr & NBUF_ARR_MASK) {
			if ((sz += sizeof hdr) > maxsz)
				goto err;
			len = nbuf_word(o->buf->base + o->offset);
			o->offset += sizeof hdr;
		} else {
			len = 1;
		}
	}
	if ((sz += len * nbuf_obj_size(o)) > maxsz)
		goto err;
	return len;
err:
	// Malformed input.
	o->ssize = o->psize = 0;
	return 0;
}

/* Returns the offset of the header given an object
 * The object must be non-repeated, or the first of repeated ones.
 */
static inline size_t
nbuf_obj_hdr_offset(const struct nbuf_obj *o)
{
	nbuf_word_t hdr;
	assert(o->offset >= sizeof hdr);
	hdr = nbuf_word(o->buf->base + o->offset - sizeof hdr);
	if (hdr & NBUF_HDR_MASK)
		return o->offset - sizeof hdr;
	/* is an array */
	assert(o->offset >= 2 * sizeof hdr);
	return o->offset - 2 * sizeof hdr;
}

/* Allocate a single object with header.
 * Caller must initialize buf, ssize and psize.
 *
 * Returns 0 iff allocation fails.
 */
static inline size_t
nbuf_alloc_obj(struct nbuf_obj *o)
{
	size_t alloc_size = nbuf_obj_size(o);
	char *p;

	alloc_size += sizeof (nbuf_word_t);
	p = nbuf_alloc_aligned(o->buf, alloc_size, sizeof (nbuf_word_t));
	if (!p)
		goto err;
	o->offset = p - o->buf->base;
	nbuf_set_word(o->buf->base + o->offset, NBUF_HDR(o->ssize, o->psize));
	o->offset += sizeof (nbuf_word_t);
	return alloc_size;
err:
	o->offset = 0;
	o->ssize = o->psize = 0;
	return 0;
}

/* Allocates repeated objects with an array header.
 * Caller must initialize buf, ssize and psize.
 *
 * Byte arrays are treated specially, they use a 4-byte header instead of
 * an 8-byte header for other kinds of arrays.  This is to optimize space
 * utilization for strings.
 *
 * Returns 0 iff allocation fails.
 */
static inline size_t
nbuf_alloc_arr(struct nbuf_obj *o, nbuf_word_t len)
{
	nbuf_word_t hdr;
	size_t alloc_size = nbuf_obj_size(o);
	int byte_arr = o->psize == 0 && o->ssize == 1;
	char *p;

	alloc_size *= len;
	alloc_size += sizeof hdr + (byte_arr ? 0 : sizeof len);
	p = nbuf_alloc_aligned(o->buf, alloc_size, sizeof (nbuf_word_t));
	if (!p)
		goto err;
	o->offset = p - o->buf->base;
	if (byte_arr)
		hdr = len | NBUF_BARR_MASK | NBUF_HDR_MASK;
	else
		hdr = NBUF_HDR(o->ssize, o->psize) | NBUF_ARR_MASK;
	nbuf_set_word(o->buf->base + o->offset, hdr);
	o->offset += sizeof (nbuf_word_t);
	if (!byte_arr) {
		nbuf_set_word(o->buf->base + o->offset, len);
		o->offset += sizeof (nbuf_word_t);
	}
	return alloc_size;
err:
	o->offset = 0;
	o->ssize = o->psize = 0;
	return 0;
}

/* Resizes an array.  Use with caution.
 *
 * This function is used to construct arrays where the length cannot be known
 * before hands.  Otherwise, use nbuf_alloc_arr.
 * The idea is to create an array of length 1, and if new elements need to be
 * added, allocate space at the end of the buffer, thus extending the array.
 * When finished, call this function to adjust the array length in the header.
 *
 * Be careful that only an array at the end of the buffer can be resized.
 * Otherwise, there's no room to extend the array.
 *
 * If the array contains sub-objects, the user should not allocate them on
 * the same buffer until the array length is finalized.
 *
 * If the user allocates the sub-objects on a new buffer, and uses
 * nbuf_obj_set_p to set pointers in the current buffer, the pointers will
 * have a special format indicating an external buffer.  After resizing the
 * array, `nbuf_fix_arr` should be called to fix the pointers.
 *
 * The text format parser uses this function to parse repeated fields, since
 * the length cannot be determined until all the fields have been parsed.
 */
size_t
nbuf_resize_arr(struct nbuf_obj *o, nbuf_word_t len);

/* Fix pointers to external buffers.  Use with caution.
 *
 * The pointers used to point to objects in `newbuf`.  This function will
 * append the content of `newbuf` to `o`'s buffer, and fix the pointers.
 *
 * The text format parser uses this function after parsing repeated message
 * fields, since they may need to allocate.
 */
size_t
nbuf_fix_arr(struct nbuf_obj *o, nbuf_word_t len, const struct nbuf_buf *newbuf);

/* Allocates a string.
 *
 * This is a specialized version of nbuf_alloc_arr.  It takes care of length
 * calculation.
 * The array length will be the string length + 1, to include an extra
 * trailing '\0'.
 * If len == -1, strlen() will be called to determine the length.
 * If str != NULL, its content will be copied into the buffer.
 * Returns the pointer to the newly-allocated string, or NULL if allocation
 * failed.
 */
static inline char *
nbuf_alloc_str(struct nbuf_obj *o, const char *str, size_t len)
{
	char *p;

	if (len + 1 == 0)
		len = strlen(str);
	o->ssize = 1;
	o->psize = 0;
	if (!nbuf_alloc_arr(o, len + 1))
		return NULL;
	p = o->buf->base + o->offset;
	if (str != NULL)
		memcpy(p, str, len);
	p[len] = '\0';
	return p;
}

/* Returns pointer for a string object.
 * Returns an empty string if n == 0.  Otherwise, returns the pointer at
 * the object's offset and sets *lenp to (n-1).
 * If lenp == NULL, the length is not returned.
 */
static inline const char *
nbuf_obj2str(const struct nbuf_obj *o, size_t n, size_t *lenp)
{
	if (!n) {
		if (lenp) *lenp = 0;
		return "";
	}
	if (lenp) *lenp = n - 1;
	return o->buf->base + o->offset;
}

/* Gets the pointer of a scalar field.
 *
 * Returns NULL if the offset would overrun the scalar part.
 */
static inline char *
nbuf_obj_s(const struct nbuf_obj *o, size_t byte_offset, size_t sz)
{
	size_t offset = o->offset + o->psize * sizeof (nbuf_word_t) + byte_offset;

	if (byte_offset + sz > o->ssize)
		return NULL;
	/* Malformed input should have been rejected by nbuf_get_obj */
	assert(offset + sz <= o->buf->len);
	return o->buf->base + offset;
}

/* Gets a pointer field.
 *
 * See nbuf_get_obj for return code.
 */
static inline size_t
nbuf_obj_p(struct nbuf_obj *oo, const struct nbuf_obj *o, size_t index)
{
	size_t ptr_offset;
	nbuf_word_t rel_ptr;

	oo->buf = o->buf;
	if (index >= o->psize)
		goto err;
	ptr_offset = o->offset + index * sizeof (nbuf_word_t);
	/* Malformed input should have been rejected by nbuf_get_obj */
	assert(ptr_offset + sizeof rel_ptr <= o->buf->len);
	rel_ptr = nbuf_word(o->buf->base + ptr_offset);
	if (rel_ptr == 0)
		goto err;
	oo->offset = ptr_offset + rel_ptr * sizeof (nbuf_word_t);
	return nbuf_get_obj(oo);
err:
	/* null pointer */
	oo->offset = 0;
	oo->ssize = oo->psize = 0;
	return 0;
}

/* Sets a pointer field.
 *
 * If rhs == NULL, clears the pointer.
 * Otherwise, the pointer will point to rhs.
 *
 * If rhs is in another buffer, pointer will be set up differently so that
 * they will be fixed later.
 */
#include <stdio.h>
static inline size_t
nbuf_obj_set_p(const struct nbuf_obj *o, size_t index, const struct nbuf_obj *rhs)
{
	size_t ptr_offset;
	nbuf_word_t rel_ptr = 0;

	if (index >= o->psize)
		return 0;
	ptr_offset = o->offset + index * sizeof (nbuf_word_t);
	/* Malformed input should have been rejected by nbuf_get_obj */
	assert(ptr_offset + sizeof rel_ptr <= o->buf->len);
	if (rhs != NULL) {
		rel_ptr = nbuf_obj_hdr_offset(rhs);
		if (o->buf == rhs->buf)
			rel_ptr -= ptr_offset;
		assert(rel_ptr % sizeof (nbuf_word_t) == 0);
		rel_ptr /= sizeof (nbuf_word_t);
		if (o->buf != rhs->buf)
			rel_ptr = ~rel_ptr;
	}
	nbuf_set_word(o->buf->base + ptr_offset, rel_ptr);
	return 1;
}

/* Moves n elements after the current element in an array.
 *
 * Since nbuf_obj does not track array length, the caller must ensure
 * this will not overrun the array.
 */
static inline void
nbuf_advance(struct nbuf_obj *o, ptrdiff_t n)
{
	o->offset += nbuf_obj_size(o) * n;
}

/* This is equivalent to nbuf_advance(o, 1).
 * Useful in loops when iterating over all elements in the array.
 */
static inline void
nbuf_next(struct nbuf_obj *o)
{
	o->offset += nbuf_obj_size(o);
}

/* Returns the difference (lhs-rhs) between two objects in the same array,
 * in units of objects.
 */
static inline ptrdiff_t
nbuf_obj_ptrdiff(const struct nbuf_obj *lhs, const struct nbuf_obj *rhs)
{
	ptrdiff_t d;
	size_t osize = nbuf_obj_size(lhs);

	assert(lhs->buf == rhs->buf);
	assert(lhs->ssize == rhs->ssize);
	assert(lhs->psize == rhs->psize);
	d = lhs->offset - rhs->offset;
	assert(d % osize == 0);
	d /= osize;
	return d;
}

/* Generated code defines wrappers a nbuf_obj.  To defeat C's typing system
 * and pass those values into a function expecting a nbuf_obj, use the
 * following macro.
 *
 * Example:
 *   nbuf_MsgDef mdef;
 *   size_t n = nbuf_Schema_message(&mdef, schema, 0);
 *   while (n--) {
 *     // process mdef
 *     nbuf_next(NBUF_OBJ(mdef));
 *   }
 */
#define NBUF_OBJ(_o) (&(_o).o)

#ifdef __cplusplus
}  /* extern "C" */
#endif

/* vim: set ft=c: */
#endif  /* _NBUF_H_ */
