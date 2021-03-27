#include "libnbuf.h"

#include <stdlib.h>

size_t nbuf_EnumVal_from_symbol(nbuf_EnumVal *eval, nbuf_EnumDef edef, const char *name, size_t len)
{
	size_t n;

	n = nbuf_EnumDef_values(eval, edef, 0);
	while (n--) {
		size_t symbol_len;
		const char *symbol = nbuf_EnumVal_symbol(*eval, &symbol_len);
		if (len == symbol_len && memcmp(name, symbol, len) == 0)
			return 1;
		nbuf_next(NBUF_OBJ(*eval));
	}
	return 0;
}

size_t nbuf_EnumVal_from_value(nbuf_EnumVal *eval, nbuf_EnumDef edef, int value)
{
	size_t n;

	n = nbuf_EnumDef_values(eval, edef, 0);
	while (n--) {
		if (nbuf_EnumVal_value(*eval) == value)
			return 1;
		nbuf_next(NBUF_OBJ(*eval));
	}
	return 0;
}

static struct nbuf_schema_set *
get_schema_set(struct nbuf_schema_set *ss, unsigned import_id)
{
	if (import_id > 0) {
		assert(import_id-1 < ss->nimports);
		ss = ss->imports[import_id-1];
	}
	return ss;
}

nbuf_Kind nbuf_get_field_type(struct nbuf_obj *o, nbuf_FieldDef fdef)
{
	nbuf_Kind kind = nbuf_FieldDef_kind(fdef);
	nbuf_Kind base_kind = nbuf_base_kind(kind);
	unsigned type_id = nbuf_FieldDef_type_id(fdef);

	switch (base_kind) {
	case nbuf_Kind_ENUM:
	case nbuf_Kind_MSG: {
		unsigned import_id = nbuf_FieldDef_import_id(fdef);
		struct nbuf *buf = NBUF_OBJ(fdef)->buf;
		struct nbuf_schema_set *ss =
			get_schema_set((struct nbuf_schema_set *) buf,
			import_id);
		nbuf_Schema schema;
		union {
			struct nbuf_obj *o;
			nbuf_EnumDef *edef;
			nbuf_MsgDef *mdef;
		} u = { o };

		if (!nbuf_get_Schema(&schema, &ss->buf, 0))
			goto err;
		if (!((base_kind == nbuf_Kind_ENUM) ?
				nbuf_Schema_enums(u.edef, schema, type_id) :
				nbuf_Schema_messages(u.mdef, schema, type_id)))
			goto err;
		return kind;
	}
	case nbuf_Kind_UINT:
	case nbuf_Kind_SINT:
	case nbuf_Kind_FLT:
	case nbuf_Kind_BOOL:
	case nbuf_Kind_STR:
		o->offset = nbuf_FieldDef_offset(fdef);
		o->ssize = 1 << type_id;
		o->psize = 0;
		return kind;
	default:
		break;
	}
err:
	return (nbuf_Kind) -1;
}

bool nbuf_lookup_defined_type(nbuf_Schema schema, const char *name, nbuf_Kind *kind, unsigned *type_id)
{
	size_t i, ecount, mcount;
	nbuf_EnumDef edef;
	nbuf_MsgDef mdef;
	const char *def_name;

	ecount = nbuf_Schema_enums(&edef, schema, 0);
	mcount = nbuf_Schema_messages(&mdef, schema, 0);
	for (i = 0; i < ecount; i++) {
		def_name = nbuf_EnumDef_name(edef, NULL);
		if (strcmp(def_name, name) == 0) {
			*kind = nbuf_Kind_ENUM;
			*type_id = i;
			return true;
		}
		nbuf_next(NBUF_OBJ(edef));
	}
	for (i = 0; i < mcount; i++) {
		def_name = nbuf_MsgDef_name(mdef, NULL);
		if (strcmp(def_name, name) == 0) {
			*kind = nbuf_Kind_MSG;
			*type_id = i;
			return true;
		}
		nbuf_next(NBUF_OBJ(mdef));
	}
	return false;
}

bool nbuf_lookup_builtin_type(const char *name, nbuf_Kind *kind, unsigned *type_id)
{
	char ch = name[0];
	long n;
	char *endp;

	switch (ch) {
	case 'u':
		name++;
		/* fallthrough */
	case 'i':
		if (strncmp(name, "int", 3) != 0)
			return false;
		n = strtol(name+3, &endp, 10);
		if (*endp != '\0')
			return false;
		switch (n) {
		case 8: *type_id = 0; break;
		case 16: *type_id = 1; break;
		case 32: *type_id = 2; break;
		case 64: *type_id = 3; break;
		default: return false;
		}
		*kind = (ch == 'u') ? nbuf_Kind_UINT : nbuf_Kind_SINT;
		return true;
	case 'f':
		if (strcmp(name, "float") == 0) {
			*kind = nbuf_Kind_FLT;
			*type_id = 2;
			return true;
		}
		break;
	case 'd':
		if (strcmp(name, "double") == 0) {
			*kind = nbuf_Kind_FLT;
			*type_id = 3;
			return true;
		}
		break;
	case 'b':
		if (strcmp(name, "bool") == 0) {
			*kind = nbuf_Kind_BOOL;
			*type_id = 0;
			return true;
		}
		break;
	case 's':
		if (strcmp(name, "string") == 0) {
			*kind = nbuf_Kind_STR;
			*type_id = 0;
			return true;
		}
		break;
	}
	return false;
}

bool nbuf_lookup_field(nbuf_FieldDef *fdef, nbuf_MsgDef mdef,
	const char *name, size_t len)
{
	size_t n;

	n = nbuf_MsgDef_fields(fdef, mdef, 0);
	if (len + 1 == 0)
		len = strlen(name);
	while (n--) {
		size_t l;
		const char *fname = nbuf_FieldDef_name(*fdef, &l);

		if (len == l && memcmp(fname, name, l) == 0)
			return true;
		nbuf_next(NBUF_OBJ(*fdef));
	}
	return false;
}
