#include "nbuf_schema.nb.h"
#include "libnbuf.h"

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
"n:\"escape\\000d\\x07\\b\\f\\r\t\\v\""
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
"n: \"escape\\0d\\a\\b\\f\\r\\t\\v\" "
"n: \"\" "
"o { a: false } "
"p { a: true b: false b: true } "
"p { a: false c { a: TRUE c: 0 e: 0 g: 0 i: 0 k: 0 m: \"\" } } ";

static struct nbuf_buf compilebuf;
static struct nbuf_compile_opt copt = {
	.outbuf = &compilebuf,
};
static nbuf_Schema schema;

#define TEST_INIT do { \
	struct nbuf_schema_set *ss; \
	nbuf_init_ex(&compilebuf, 0); \
	ss = nbuf_compile_str(&copt, textschema, sizeof textschema - 1, "<string>"); \
	TEST_ASSERT_(ss != NULL && nbuf_get_Schema(&schema, &ss->buf, 0), \
		"compiling schema succeeds"); \
} while (0)

#define TEST_FINI do { \
	nbuf_free_compiled(&copt); \
} while (0)

#include "acutest.h"

static void check_str_leq(const char *a, size_t lena, const char *b, size_t lenb)
{
	if (!TEST_CHECK(lena == lenb && memcmp(a, b, lena) == 0)) {
		TEST_MSG("Produced: (%u) %.*s", (unsigned) lena, (int) lena, a);
		TEST_MSG("Expected: (%u) %.*s", (unsigned) lenb, (int) lenb, b);
	}
}

static void bad_compile_case(const char *case_name, const char *input)
{
	struct nbuf_buf buf;
	struct nbuf_compile_opt opt = {
		.outbuf = &buf,
	};

	nbuf_init_ex(&buf, 0);
	TEST_CASE(case_name);
	TEST_ASSERT_(!nbuf_compile_str(&opt, input, strlen(input), "<string>"),
		"compile should fail");
	TEST_CHECK(buf.base == NULL);
}

void test_bad_compile(void)
{
	bad_compile_case("disabled import", "import \"foo\";");
	bad_compile_case("unresolved type", "message T { U x; }");
	bad_compile_case("empty enum", "enum T {}");
	bad_compile_case("empty message", "message T {}");
}

void test_parse_print(void)
{
	struct nbuf_buf textbuf, parsebuf;
	struct nbuf_parse_opt paopt = {
		.outbuf = &parsebuf,
		.filename = "<test input>",
	};
	/* Use 'b' to suppress linefeed translation on some strange platform */
	FILE *f = fopen("test.out", "wb+");
	struct nbuf_print_opt propt = {
		.f = f,
		.indent = -1,
		.msg_type_hdr = true,
	};
	nbuf_MsgDef mdef;
	struct nbuf_obj o;

	TEST_ASSERT(nbuf_Schema_messages(&mdef, schema, 0));

	nbuf_init_ex(&parsebuf, 0);
	TEST_CASE("parse");
	TEST_ASSERT(nbuf_parse(&paopt, &o, test_input, sizeof test_input - 1, mdef));

	TEST_CASE("print");
	TEST_ASSERT(nbuf_print(&propt, &o, mdef));
	nbuf_clear(&parsebuf);
	rewind(f);
	TEST_ASSERT(nbuf_load_fp(&textbuf, f));
	fclose(f);
	check_str_leq(textbuf.base, textbuf.len, test_output, sizeof test_output - 1);
	nbuf_unload_file(&textbuf);
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
	struct nbuf_buf parsebuf;
	nbuf_MsgDef mdef;

	TEST_ASSERT(nbuf_Schema_messages(&mdef, schema, 0));

	nbuf_init_ex(&parsebuf, 0);
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
}

void test_depth_limit(void)
{
	struct nbuf_buf parsebuf;
	struct nbuf_parse_opt paopt = {
		.outbuf = &parsebuf,
		.filename = "<test input>",
		.max_depth = 1,
	};
	nbuf_MsgDef mdef, mdef1;
	struct nbuf_obj o;

	TEST_ASSERT(nbuf_Schema_messages(&mdef, schema, 0));
	TEST_ASSERT(nbuf_Schema_messages(&mdef1, schema, 1));

	nbuf_init_ex(&parsebuf, 0);
	TEST_CHECK(!nbuf_parse(&paopt, &o, test_input, sizeof test_input - 1, mdef));

	paopt.max_depth = 4;
	TEST_CHECK(nbuf_parse(&paopt, &o, test_input, sizeof test_input - 1, mdef));
	nbuf_clear(&parsebuf);
}

TEST_LIST = {
	{"bad_compile", test_bad_compile},
	{"parse_print", test_parse_print},
	{"bad_parse", test_bad_parse},
	{"depth_limit", test_depth_limit},
	{NULL, NULL},
};
