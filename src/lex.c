#include "lex.h"

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

void
nbuf_lexinit(lexState *l, const char *in_filename, const char *input, size_t input_len)
{
	l->in_filename = in_filename;
	l->input = input;
	l->input_end = input + input_len;
	l->lineno = 1;
	l->token = NULL;
}

void
nbuf_lexerror(lexState *l, const char *fmt, ...)
{
	va_list argp;

	fprintf(stderr, "error:%s:%d: ", l->in_filename, l->lineno);
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
	fprintf(stderr, "\n");
}

#define QBUF_SIZ 16

static const char *token_to_str(char *qbuf, Token tok)
{
	static const char *const names[] = {
		"<end of input>", "identifier",
		"integer", "float", "string",
	};
	if (tok < Token_EOF) {
		if (isprint(tok))
			sprintf(qbuf, "'%c'", tok);
		else
			sprintf(qbuf, "'\\%03o'", tok);
		return qbuf;
	}
	if (tok < Token_UNK)
		return names[tok - Token_EOF];
	return "<unknown>";
}

void
nbuf_lexsyntax(lexState *l, Token expect, Token seen)
{
	char q1[QBUF_SIZ], q2[QBUF_SIZ];
	const char *s1, *s2;

	s1 = token_to_str(q1, expect);
	if (seen == Token_ID) {
		size_t n = TOKENLEN(l);

		if (n <= QBUF_SIZ - 3) {
			sprintf(q2, "'%.*s'", (int) n, TOKEN(l));
		} else {
			sprintf(q2, "'%.10s...'", TOKEN(l));
		}
		s2 = q2;
	} else {
		s2 = token_to_str(q2, seen);
	}
	nbuf_lexerror(l, "missing %s, got %s", s1, s2);
}


#define EOI(l) (l->input == l->input_end)
#define GETC(l) (EOI(l) ? Token_EOF : *l->input++)
#define UNGETC(l, c) ((c) == Token_EOF ? (void) 0 : (void) --l->input)

/* skip until line end */
static void
skipline(lexState *l)
{
	int ch;

	while ((ch = GETC(l)) != Token_EOF && ch != '\n')
		;
	UNGETC(l, ch);
}

/* skip until end of long C-style comment */
static void
skiplong(lexState *l)
{
	int ch;

	ch = GETC(l);
	while (ch != Token_EOF)
		if (ch == '*') {
			if ((ch = GETC(l)) == '/')
				break;
		} else {
			if (ch == '\n')
				l->lineno++;
			ch = GETC(l);
		}
	if (ch == Token_EOF)
		nbuf_lexerror(l, "comment is not closed at end of input");
}

#define CLEAR (l->token = l->input)
#define NEXT (ch = GETC(l))
#define FIN UNGETC(l, ch)

#define isodigit(ch) ('0' <= (ch) && (ch) <= '7')

/* Scans a decimal/octal/hexadecimal integer, or a floating point number
 * -?0[0-7]+         -> oct
 * -?0x[0-9a-fA-F]+  -> hex
 * -?[1-9][0-9]+     -> dec
 * 1.2, 1e100, etc.  -> float
 * FIXME: does not support inf/nan.
 */
static Token
scannum(lexState *l, int ch)
{
	int base = 10;
	Token token = Token_INT;

	if (ch == '-')
		NEXT;
	if (ch == '0') {
		NEXT;
		if (ch == 'x' || ch == 'X') {
			base = 16;
			do
				NEXT;
			while (isxdigit(ch));
		} else if (isodigit(ch)) {
			base = 8;
			do
				NEXT;
			while (isodigit(ch));
		}
	} else {
		do
			NEXT;
		while (isdigit(ch));
	}
	if (base == 10) {
		if (ch == '.') {
			token = Token_FLT;
			do
				NEXT;
			while (isdigit(ch));
		}
		if (ch == 'e' || ch == 'E') {
			token = Token_FLT;
			NEXT;
			if (ch == '+' || ch == '-')
				NEXT;
			while (isdigit(ch))
				NEXT;
		}
	}
	if (isalnum(ch)) {
		nbuf_lexerror(l, "malformed number: %.*s",
			(int) TOKENLEN(l), TOKEN(l));
		token = Token_UNK;
	}
	FIN;
	return token;
}

static Token
scanident(lexState *l, int ch)
{
	NEXT;
	while (isalnum(ch) || ch == '_')
		NEXT;
	FIN;
	return Token_ID;
}

static void
skipspaces(lexState *l)
{
	int ch;

	do
		if ((ch = GETC(l)) == '\n')
			l->lineno++;
	while (isspace(ch));
	UNGETC(l, ch);
}

static Token
scanstr(lexState *l, int ch)
{
	int delim = ch;

	NEXT;
	while (ch != delim) {
		if (EOI(l)) {
			nbuf_lexerror(l, "string is not closed at end of input");
			return Token_UNK;
		}
		if (ch == '\\') {
			NEXT;
		} else if (ch == '\n') {
			nbuf_lexerror(l, "newline within string literal");
			l->lineno++;
		}
		NEXT;
	}
	return Token_STR;
}

Token
nbuf_lex(lexState *l)
{
	int ch;

reinput:
	skipspaces(l);
	if (EOI(l))
		return Token_EOF;
	CLEAR;
	ch = GETC(l);
	switch (ch) {
	case '/':
		ch = GETC(l);
		if (ch == '*') {
			skiplong(l);
		} else if (ch == '/') {
			skipline(l);
		} else {
			UNGETC(l, ch);
			return (Token) '/';
		}
		goto reinput;
	case '#':
		skipline(l);
		goto reinput;
	case '"':
		return scanstr(l, ch);
	case '-':
		ch = GETC(l);
		UNGETC(l, ch);
		if (isdigit(ch))
			return scannum(l, '-');
		return (Token) '-';
	default:
		if (isdigit(ch))
			return scannum(l, ch);
		else if (isalpha(ch) || ch == '_')
			return scanident(l, ch);
		break;
	}
	return (Token) ch;
}
