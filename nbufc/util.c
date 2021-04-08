#include "nbuf.h"
#include "util.h"
#include "libnbufc.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void nbufc_out_path_ident(FILE *f, const char *s)
{
	char ch;

	while ((ch = *s++) != '\0')
		isalnum(ch) ? putc(ch, f) : fprintf(f, "_%02x", ch);
}
