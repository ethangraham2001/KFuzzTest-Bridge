#ifndef KFUZZTEST_INPUT_LEXER_H
#define KFUZZTEST_INPUT_LEXER_H 1

#include <stdint.h>
#include <stdlib.h>

enum token_type {
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_LBRACKET,
	TOKEN_RBRACKET,
	TOKEN_COMMA,

	TOKEN_KEYWORD_PTR,
	TOKEN_KEYWORD_ARR,
	TOKEN_KEYWORD_U8,
	TOKEN_KEYWORD_U16,
	TOKEN_KEYWORD_U32,
	TOKEN_KEYWORD_U64,

	TOKEN_IDENTIFIER,
	TOKEN_INTEGER,

	TOKEN_EOF,
	TOKEN_ERROR,
};

static const char *token_names[] = {
	"TOKEN_LBRACE",	     "TOKEN_RBRACE",	  "TOKEN_LBRACKET",
	"TOKEN_RBRACKET",    "TOKEN_COMMA",	  "TOKEN_KEYWORD_PTR",
	"TOKEN_KEYWORD_ARR", "TOKEN_KEYWORD_U8",  "TOKEN_KEYWORD_U16",
	"TOKEN_KEYWORD_U32", "TOKEN_KEYWORD_U64", "TOKEN_IDENTIFIER",
	"TOKEN_INTEGER",     "TOKEN_EOF",	  "TOKEN_ERROR",
};

struct token {
	enum token_type type;
	union {
		uint64_t integer;
		struct {
			const char *start;
			size_t length;
		} identifier;
	} data;
	int position;
};

int tokenize(const char *input, struct token **tokens, size_t *num_tokens);

#endif /* KFUZZTEST_INPUT_LEXER_H */
