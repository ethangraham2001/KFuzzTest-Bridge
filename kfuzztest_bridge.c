// SPDX-License-Identifier: GPL-2.0
/*
 * KFuzzTest tool for sending inputs into a KFuzzTest harness.
 *
 * Copyright 2025 Google LLC
 */

/**
 * Let's try and encode this...
 * struct thing {
 *	char *foo;
 *	size_t size;
 * }
 */

#include <stdio.h>
#include "kfuzztest_input_lexer.h"

const char *input =
	"data { arr[u8, 42] } foo { ptr[data] } bar { u32 a, ptr[foo] }";

enum token_type expected[] = {
	TOKEN_IDENTIFIER,  TOKEN_LBRACE,     TOKEN_KEYWORD_ARR,
	TOKEN_LBRACKET,	   TOKEN_KEYWORD_U8, TOKEN_COMMA,
	TOKEN_INTEGER,	   TOKEN_RBRACKET,   TOKEN_RBRACE,
	TOKEN_IDENTIFIER,  TOKEN_LBRACE,     TOKEN_KEYWORD_PTR,
	TOKEN_LBRACKET,	   TOKEN_IDENTIFIER, TOKEN_RBRACKET,
	TOKEN_RBRACE,	   TOKEN_IDENTIFIER, TOKEN_LBRACE,
	TOKEN_KEYWORD_U32, TOKEN_IDENTIFIER, TOKEN_COMMA,
	TOKEN_KEYWORD_PTR, TOKEN_LBRACKET,   TOKEN_IDENTIFIER,
	TOKEN_RBRACKET,	   TOKEN_RBRACE,     TOKEN_EOF,
};

int main(void)
{
	struct token **tokens;
	size_t num_tokens;
	int ret;
	int i;

	ret = tokenize(input, &tokens, &num_tokens);
	if (ret)
		return ret;

	if (num_tokens != COUNT_OF(expected)) {
		printf("got unexpected size %zu vs %zu\n", num_tokens,
		       COUNT_OF(expected));
		return 1;
	}
	for (i = 0; i < num_tokens; i++) {
		if (expected[i] != tokens[i]->type) {
			printf("mismatch in token %d\n", i);
			return 1;
		}
	}

	printf("got expected output.\n");
	return 0;
}
