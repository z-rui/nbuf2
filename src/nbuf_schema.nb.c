/* Generated by nbufc.  DO NOT EDIT!
 * source: nbuf_schema.nbuf
 */

#define NBUF_SS_NAME nbuf_schema_file_nbuf_5fschema_2enbuf
#define NBUF_SS_IMPORTS 0
#include "nbuf_schema.nb.h"
#include "libnbuf.h"

const struct nbuf_schema_set NBUF_SS_NAME = {
	/*.buf=*/{ /*.base=*/(char *)
"\4\0\0\200\n\0\0\0\3\0\0\0\v\0\0\0<\0\0\0\21\0\0\300nbuf_schema.nbuf\0\0\0"
"\0\5\0\0\300nbuf\0\0\0\0\2\0\0\240\1\0\0\0\2\0\0\0\4\0\0\0\5\0\0\300Kind\0"
"\0\0\0\1\0\1\240\t\0\0\0\22\0\0\0\0\0\0\0\23\0\0\0\1\0\0\0\24\0\0\0\2\0\0"
"\0\25\0\0\0\3\0\0\0\26\0\0\0\4\0\0\0\27\0\0\0\5\0\0\0\27\0\0\0\6\0\0\0\27"
"\0\0\0\a\0\0\0\27\0\0\0\b\0\0\0\5\0\0\300VOID\0\0\0\0\5\0\0\300BOOL\0\0\0"
"\0\5\0\0\300ENUM\0\0\0\0\5\0\0\300UINT\0\0\0\0\5\0\0\300SINT\0\0\0\0\4\0\0"
"\300FLT\0\4\0\0\300MSG\0\4\0\0\300STR\0\4\0\0\300ARR\0\2\0\1\240\5\0\0\0="
"\0\0\0?\0\0\0\0\0\4\0Z\0\0\0\\\0\0\0\0\0\2\0h\0\0\0j\0\0\0\4\0\1\0v\0\0\0"
"x\0\0\0\4\0\2\0\220\0\0\0\223\0\0\0\b\0\1\0\5\0\0\300Kind\0\0\0\0\1\0\1\240"
"\t\0\0\0\22\0\0\0\0\0\0\0\23\0\0\0\1\0\0\0\24\0\0\0\2\0\0\0\25\0\0\0\3\0\0"
"\0\26\0\0\0\4\0\0\0\27\0\0\0\5\0\0\0\27\0\0\0\6\0\0\0\27\0\0\0\a\0\0\0\27"
"\0\0\0\b\0\0\0\5\0\0\300VOID\0\0\0\0\5\0\0\300BOOL\0\0\0\0\5\0\0\300ENUM\0"
"\0\0\0\5\0\0\300UINT\0\0\0\0\5\0\0\300SINT\0\0\0\0\4\0\0\300FLT\0\4\0\0\300"
"MSG\0\4\0\0\300STR\0\4\0\0\300ARR\0\a\0\0\300Schema\0\0\1\0\2\240\4\0\0\0"
"\f\0\0\0\a\0\0\0\0\0\0\0\r\0\0\0\a\0\0\0\0\0\1\0\16\0\0\0\16\0\0\0\1\0\2\0"
"\16\0\0\0\16\0\0\0\3\0\3\0\t\0\0\300pkg_name\0\0\0\300\t\0\0\300src_name\0"
"NUM\6\0\0\300enums\0NT\t\0\0\300messages\0\0\0\0\b\0\0\300EnumDef\0\1\0\2"
"\240\2\0\0\0\6\0\0\0\a\0\0\0\0\0\0\0\6\0\0\0\16\0\0\0\2\0\1\0\5\0\0\300na"
"me\0ame\a\0\0\300values\0\0\b\0\0\300EnumVal\0\1\0\2\240\2\0\0\0\6\0\0\0\a"
"\0\0\0\0\0\0\0\6\0\0\0\3\0\0\0\1\0\0\0\a\0\0\300symbol\0e\6\0\0\300value\0"
"\0\0\a\0\0\300MsgDef\0\0\1\0\2\240\4\0\0\0\f\0\0\0\a\0\0\0\0\0\0\0\f\0\0\0"
"\16\0\0\0\4\0\1\0\f\0\0\0\3\0\0\0\1\0\0\0\f\0\0\0\3\0\0\0\1\0\2\0\5\0\0\300"
"name\0l\0e\a\0\0\300fields\0_\6\0\0\300ssize\0\0\300\6\0\0\300psize\0\0\0"
"\t\0\0\300FieldDef\0\0\0\0\1\0\2\240\5\0\0\0\17\0\0\0\a\0\0\0\0\0\0\0\17\0"
"\0\0\2\0\0\0\0\0\0\0\17\0\0\0\3\0\0\0\1\0\2\0\20\0\0\0\3\0\0\0\1\0\4\0\20"
"\0\0\0\3\0\0\0\1\0\6\0\5\0\0\300name\0l\0e\5\0\0\300kind\0s\0_\n\0\0\300i"
"mport_id\0\0\300\b\0\0\300type_id\0\a\0\0\300offset",
		/*.len=*/1035, /*.cap=*/0},
	/*.nimports=*/0, /*.imports=*/{
	}
};

const nbuf_EnumDef nbuf_refl_Kind = {{(struct nbuf_buf *) &NBUF_SS_NAME, 64, 0, 2}};
const nbuf_MsgDef nbuf_refl_Schema = {{(struct nbuf_buf *) &NBUF_SS_NAME, 264, 4, 2}};
const nbuf_MsgDef nbuf_refl_EnumDef = {{(struct nbuf_buf *) &NBUF_SS_NAME, 276, 4, 2}};
const nbuf_MsgDef nbuf_refl_EnumVal = {{(struct nbuf_buf *) &NBUF_SS_NAME, 288, 4, 2}};
const nbuf_MsgDef nbuf_refl_MsgDef = {{(struct nbuf_buf *) &NBUF_SS_NAME, 300, 4, 2}};
const nbuf_MsgDef nbuf_refl_FieldDef = {{(struct nbuf_buf *) &NBUF_SS_NAME, 312, 4, 2}};
