#include "acutest.h"

#include "nbuf_schema.nb.h"
#include "libnbuf.h"
#include "libnbufc.h"

static const char schema_file[] = "test.nbuf";

static const char textschema[] =
"/** some long \n"
" comments **/\n"
"package test;"
"enum TriState {"
"  UNKNOWN = -1, FALSE, TRUE # some comments\n"
"}"
"message Msg { // some comments\n"
"  TriState a;"
"  TriState[] b;"
"  uint8 c;"
"  int8[] d;"
"  uint16 e;"
"  int16[] f;"
"  uint32 g;"
"  int32[] h;"
"  uint64 i;"
"  int64[] j;"
"  float k;"
"  double[] l;"
"  string m;"
"  string[] n;"
"  SubMsg o;"
"  SubMsg[] p;"
"}"
"message SubMsg {"
"  bool a;"
"  bool[] b;"
"  Msg c;"
"}";

static const char test_input[] =
"b:UNKNOWN "
"b:1 "
"b:FALSE "
"b:-99 "
"c:0xfe "
"d:-128 "
"d:0177 "
"e:0xfffe "
"f:-32768 "
"f:077777 "
"g:0xfffdfffe "
"h:-2147483648 "
"h:017777777777 "
"i:0xfffbfffcfffdfffe "
"j:-9223372036854775808 "
"j:777777777777777777777 "
"k:1 "
"l:2.7182818 "
"l:-3.1415927e+16 "
"m:\"str\" \"cat\""
"n:\"multi\\nline\""
"n:\"escape\\000d\""
"n:\"\""
"o{}"
"p{a: true b: false b: true}"
"p{c{a:TRUE}}";

static const char test_output[] =
"# test.Msg\n"
"a: FALSE "
"b: UNKNOWN "
"b: TRUE "
"b: FALSE "
"b: -99 "
"c: 254 "
"d: -128 "
"d: 127 "
"e: 65534 "
"f: -32768 "
"f: 32767 "
"g: 4294836222 "
"h: -2147483648 "
"h: 2147483647 "
"i: 18445618160917676030 "
"j: -9223372036854775808 "
"j: 9223372036854775807 "
"k: 1 "
"l: 2.7182818 "
"l: -3.1415927e+16 "
"m: \"strcat\" "
"n: \"multi\\nline\" "
"n: \"escape\\0d\" "
"n: \"\" "
"o { a: false } "
"p { a: true b: false b: true } "
"p { a: false c { a: TRUE c: 0 e: 0 g: 0 i: 0 k: 0 m: \"\" } } ";

static void check_str_leq(const char *a, size_t lena, const char *b, size_t lenb)
{
	if (!TEST_CHECK(lena == lenb && memcmp(a, b, lena) == 0)) {
		TEST_MSG("Produced: (%u) %.*s", (unsigned) lena, (int) lena, a);
		TEST_MSG("Expected: (%u) %.*s", (unsigned) lenb, (int) lenb, b);
	}
}

void test_codegen(void)
{
	static struct nbuf_buf textbuf = {
		.base = (char *) textschema,
		.len = sizeof textschema - 1,
	};
	struct nbuf_schema_set *ss;
	struct nbuf_buf compilebuf = {NULL};
	struct nbufc_compile_opt copt = {
		.outbuf = &compilebuf,
	};
	struct nbufc_codegen_opt gopt;
	nbuf_Schema schema;
	nbuf_MsgDef mdef;

	memset(&gopt, 0, sizeof gopt);
	nbuf_save_file(&textbuf, schema_file);
	ss = nbufc_compile(&copt, schema_file);
	TEST_ASSERT_(ss != NULL, "compiling %s returns a valid schema set", schema_file);
	TEST_ASSERT(nbuf_get_Schema(&schema, &ss->buf, 0));
	TEST_ASSERT(nbuf_Schema_messages(&mdef, schema, 0));

	TEST_CHECK(nbufc_codegen_c(&gopt, ss) == 0);
	TEST_CHECK(nbufc_codegen_cpp(&gopt, ss) == 0);

	nbufc_free_compiled(&copt);
}


void test_parse_print(void)
{
	struct nbuf_buf textbuf = {
		.base = (char *) textschema,
		.len = sizeof textschema - 1,
	};
	struct nbuf_schema_set *ss;
	struct nbuf_buf compilebuf = {NULL}, parsebuf = {NULL};
	struct nbufc_compile_opt copt = {
		.outbuf = &compilebuf,
	};
	struct nbuf_parse_opt paopt = {
		.outbuf = &parsebuf,
		.filename = "<test input>",
	};
	FILE *f = fopen("test.out", "w+");
	struct nbuf_print_opt propt = {
		.f = f,
		.indent = -1,
		.msg_type_hdr = true,
	};
	nbuf_Schema schema;
	nbuf_MsgDef mdef;
	struct nbuf_obj o;

	nbuf_save_file(&textbuf, schema_file);

	TEST_CASE("compile");
	ss = nbufc_compile(&copt, schema_file);
	TEST_ASSERT_(ss != NULL, "compiling %s returns a valid schema set", schema_file);
	TEST_ASSERT(nbuf_get_Schema(&schema, &ss->buf, 0));
	TEST_ASSERT(nbuf_Schema_messages(&mdef, schema, 0));

	TEST_CASE("parse");
	TEST_ASSERT(nbuf_parse(&paopt, &o, test_input, sizeof test_input - 1, mdef));

	TEST_CASE("print");
	TEST_ASSERT(nbuf_print(&propt, &o, mdef));
	nbuf_clear(&parsebuf);
	rewind(f);
	TEST_ASSERT(nbuf_load_fp(&textbuf, f));
	check_str_leq(textbuf.base, textbuf.len, test_output, sizeof test_output - 1);

	fclose(f);
	nbuf_clear(&parsebuf);
	nbufc_free_compiled(&copt);
}

static void bad_parse_case(struct nbuf_buf *parsebuf, nbuf_MsgDef mdef, const char *case_name, const char *input)
{
	struct nbuf_parse_opt paopt = {
		.outbuf = parsebuf,
		.filename = "<test input>",
	};
	struct nbuf_obj o;

	TEST_CASE(case_name);
	TEST_CHECK(!nbuf_parse(&paopt, &o, input, strlen(input), mdef));
}

void test_bad_parse(void)
{
	static struct nbuf_buf textbuf = {
		.base = (char *) textschema,
		.len = sizeof textschema - 1,
	};
	struct nbuf_schema_set *ss;
	struct nbuf_buf compilebuf = {NULL}, parsebuf = {NULL};
	struct nbufc_compile_opt copt = {
		.outbuf = &compilebuf,
	};
	nbuf_Schema schema;
	nbuf_MsgDef mdef;

	nbuf_save_file(&textbuf, schema_file);
	ss = nbufc_compile(&copt, schema_file);
	TEST_ASSERT_(ss != NULL, "compiling %s returns a valid schema set", schema_file);
	TEST_ASSERT(nbuf_get_Schema(&schema, &ss->buf, 0));
	TEST_ASSERT(nbuf_Schema_messages(&mdef, schema, 0));

	bad_parse_case(&parsebuf, mdef, "bad enum", "a: true");
	bad_parse_case(&parsebuf, mdef, "bad number", "c:1d:2");
	bad_parse_case(&parsebuf, mdef, "bad bool", "o { a: FALSE }");
	bad_parse_case(&parsebuf, mdef, "bad int", "c: 1.414");
	bad_parse_case(&parsebuf, mdef, "bad float", "k: false");
	bad_parse_case(&parsebuf, mdef, "bad string", "m: 0");
	bad_parse_case(&parsebuf, mdef, "bad scalar", "b { false }");
	bad_parse_case(&parsebuf, mdef, "bad field", "xyzzy: 0");
	bad_parse_case(&parsebuf, mdef, "bad msg", "o: 0");
	bad_parse_case(&parsebuf, mdef, "nonterminating msg", "o { a: true");
	bad_parse_case(&parsebuf, mdef, "nonterminating string", "m: \"bad string...");
	bad_parse_case(&parsebuf, mdef, "nonterminating comment", "m: /*bad comment...");
	bad_parse_case(&parsebuf, mdef, "scattered repeated field", "d: 0 c: 1 d: 2");

	nbuf_clear(&parsebuf);
	nbufc_free_compiled(&copt);
}

void test_depth_limit(void)
{
	static struct nbuf_buf textbuf = {
		.base = (char *) textschema,
		.len = sizeof textschema - 1,
	};
	struct nbuf_schema_set *ss;
	struct nbuf_buf compilebuf = {NULL}, parsebuf = {NULL};
	struct nbufc_compile_opt copt = {
		.outbuf = &compilebuf,
	};
	struct nbuf_parse_opt paopt = {
		.outbuf = &parsebuf,
		.filename = "<test input>",
		.max_depth = 1,
	};
	nbuf_Schema schema;
	nbuf_MsgDef mdef, mdef1;
	struct nbuf_obj o;

	nbuf_save_file(&textbuf, schema_file);
	ss = nbufc_compile(&copt, schema_file);
	TEST_ASSERT_(ss != NULL, "compiling %s returns a valid schema set", schema_file);
	TEST_ASSERT(nbuf_get_Schema(&schema, &ss->buf, 0));
	TEST_ASSERT(nbuf_Schema_messages(&mdef, schema, 0));
	TEST_ASSERT(nbuf_Schema_messages(&mdef1, schema, 1));

	TEST_CASE("parse");
	TEST_CHECK(!nbuf_parse(&paopt, &o, test_input, sizeof test_input - 1, mdef));

	paopt.max_depth = 4;
	TEST_CHECK(nbuf_parse(&paopt, &o, test_input, sizeof test_input - 1, mdef));
	nbuf_clear(&parsebuf);

	nbufc_free_compiled(&copt);
}

TEST_LIST = {
	{"codegen", test_codegen},
	{"parse_print", test_parse_print},
	{"bad_parse", test_bad_parse},
	{"depth_limit", test_depth_limit},
	{NULL, NULL},
};
