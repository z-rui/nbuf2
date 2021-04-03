#ifndef LIBNBUF_H_
#define LIBNBUF_H_

#include "nbuf.h"
#include "nbuf_schema.nb.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convenience functions for loading/saving files.
 *
 * nbuf_load_file may use mmap() to load the file content, so it has
 * to be released by nbuf_unload_file().
 */
size_t nbuf_load_fp(struct nbuf_buf *buf, FILE *f);
size_t nbuf_load_fd(struct nbuf_buf *buf, int fd);
size_t nbuf_load_file(struct nbuf_buf *buf, const char *filename);
void nbuf_unload_file(struct nbuf_buf *buf);

size_t nbuf_save_fp(struct nbuf_buf *buf, FILE *f);
size_t nbuf_save_fd(struct nbuf_buf *buf, int fd);
size_t nbuf_save_file(struct nbuf_buf *buf, const char *filename);

/* Reflection */

#ifndef NBUF_SS_IMPORTS
# define NBUF_SS_IMPORTS 1
#endif
/* This structure stores the schema in generated code.
 * Imported schemas are stored in the imports array, whose length
 * will be overriden by the generated code by defining NBUF_SS_IMPORTS
 * before including this file.
 */
struct nbuf_schema_set {
	struct nbuf_buf buf;
	size_t nimports;
	struct nbuf_schema_set *imports[NBUF_SS_IMPORTS];
};

static inline nbuf_Kind nbuf_base_kind(nbuf_Kind kind)
{
	return (nbuf_Kind) (kind & (nbuf_Kind_ARR - 1));
}

static inline bool nbuf_is_repeated(nbuf_Kind kind)
{
	return (kind & nbuf_Kind_ARR) != 0;
}

static inline bool nbuf_is_scalar(nbuf_Kind kind)
{
	kind = (nbuf_Kind) (kind &~ nbuf_Kind_ARR);
	return kind != nbuf_Kind_STR && kind != nbuf_Kind_MSG;
}

nbuf_Kind nbuf_get_field_type(struct nbuf_obj *o, nbuf_FieldDef fdef);

static inline size_t
nbuf_refl_alloc_msg(struct nbuf_obj *o, struct nbuf_buf *buf, nbuf_MsgDef mdef)
{
	o->buf = buf;
	o->ssize = nbuf_MsgDef_ssize(mdef);
	o->psize = nbuf_MsgDef_psize(mdef);
	return nbuf_alloc_obj(o);
}

static inline size_t
nbuf_refl_alloc_multi_msg(struct nbuf_obj *o, struct nbuf_buf *buf, nbuf_MsgDef mdef, size_t n)
{
	o->buf = buf;
	o->ssize = nbuf_MsgDef_ssize(mdef);
	o->psize = nbuf_MsgDef_psize(mdef);
	return nbuf_alloc_arr(o, n);
}

size_t nbuf_EnumVal_from_value(nbuf_EnumVal *eval, nbuf_EnumDef edef, int value);
size_t nbuf_EnumVal_from_symbol(nbuf_EnumVal *eval, nbuf_EnumDef edef, const char *name, size_t len);
bool nbuf_lookup_defined_type(nbuf_Schema schema, const char *name, nbuf_Kind *kind, unsigned *type_id);
bool nbuf_lookup_builtin_type(const char *name, nbuf_Kind *kind, unsigned *type_id);
bool nbuf_lookup_field(nbuf_FieldDef *fdef, nbuf_MsgDef mdef,
	const char *name, size_t len);

/* Text format printer */
struct nbuf_print_opt {
	/* Output file */
	FILE *f;
	/* Number of spaces for each level of indentation.
	 * A negative value will output everything on a single line.
	 */
	int indent;
	/* Max number of nested messages.  This is used to prevent stack
	 * overflow.
	 * If set to 0, the default value will be used.
	 */
	int max_depth;
	/* Write a comment on the first line indicating the message type.
	 */
	bool msg_type_hdr;
	/* Do not escape characters >= 128.
	 * May be useful if the string contains non-ASCII printable characters.
	 */
	bool loose_escape;
};

/* Prints an object whose type is specified by mdef.
 */
bool nbuf_print(const struct nbuf_print_opt *opt,
	const struct nbuf_obj *o,
	nbuf_MsgDef mdef);

/* Text format parser. */
struct nbuf_parse_opt {
	/* Schema */
	struct nbuf_buf *outbuf;
	int max_depth;
	const char *filename;
};

/* Parses an objet whose type is specified by mdef.
 *
 * The input is given in input, input_len.
 */
bool nbuf_parse(struct nbuf_parse_opt *opt, struct nbuf_obj *o,
	const char *input, size_t input_len, nbuf_MsgDef mdef);

size_t nbuf_unescape(struct nbuf_buf *buf, const char *s, size_t len);

#define NBUF_PRINT_LOOSE_ESCAPE 0x80000000U
void nbuf_print_escaped(FILE *f, const char *s, size_t len, unsigned flags);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* LIBNBUF_H_ */
