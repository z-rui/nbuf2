#include "libnbuf.h"
#include "libnbufc.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
	struct nbuf_schema_set *ss;
	struct nbufc_codegen_opt opt;

	if (argc != 2)
		return 1;
	memset(&opt, 0, sizeof opt);
	ss = nbuf_compile_schema(argv[1]);
	if (!ss)
		return 1;
	return nbufc_codegen_c(&opt, ss);
}
