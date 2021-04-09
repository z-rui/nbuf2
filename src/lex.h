#ifndef NBUF_LEX_H_
#define NBUF_LEX_H_

#include <stddef.h>

typedef struct {
	const char *in_filename;
	const char *input;
	const char *input_end;
	const char *token;
	int lineno;
} lexState;
#define TOKEN(l) ((l)->token)
#define TOKENLEN(l) ((size_t) ((l)->input - (l)->token))

typedef enum {
	Token_EOF = 256,
	Token_ID,
	Token_INT,
	Token_FLT,
	Token_STR,
	Token_UNK,
} Token;

Token nbuf_lex(lexState *l);
void nbuf_lexinit(lexState *l, const char *in_filename, const char *input, size_t input_len);
void
#if __GNUC__ >= 4
__attribute__ ((format (printf, 2, 3)))
#endif
nbuf_lexerror(lexState *l, const char *fmt, ...);
void nbuf_lexsyntax(lexState *l, Token expect, Token seen);

#endif  /* NBUF_LEX_H_ */
