// SPDX-License-Identifier: GPL-2.0
/*
 * KFuzzTest tool for sending inputs into a KFuzzTest harness.
 *
 * Copyright 2025 Google LLC
 */
#include <stdio.h>

#include "kfuzztest_input_lexer.h"
#include "kfuzztest_input_parser.h"
#include "kfuzztest_encoder.h"
#include "debug.h"

const char *input = "bar { u32 ptr[foo] }; foo { ptr[data] }; data { arr[u8, 42] };";

enum token_type expected[] = {
	TOKEN_IDENTIFIER, TOKEN_LBRACE,	     TOKEN_KEYWORD_ARR, TOKEN_LBRACKET,	   TOKEN_KEYWORD_U8,
	TOKEN_COMMA,	  TOKEN_INTEGER,     TOKEN_RBRACKET,	TOKEN_RBRACE,	   TOKEN_IDENTIFIER,
	TOKEN_LBRACE,	  TOKEN_KEYWORD_PTR, TOKEN_LBRACKET,	TOKEN_IDENTIFIER,  TOKEN_RBRACKET,
	TOKEN_RBRACE,	  TOKEN_IDENTIFIER,  TOKEN_LBRACE,	TOKEN_KEYWORD_U32, TOKEN_KEYWORD_PTR,
	TOKEN_LBRACKET,	  TOKEN_IDENTIFIER,  TOKEN_RBRACKET,	TOKEN_RBRACE,	   TOKEN_EOF,
};

const char *usage_str = "usage: "
			"./kfuzztest-bridge <program-description> <fuzz-target-name> <input-file>\n"
			"for more detailed information see <docs>";

static void usage()
{
	printf("%s\n", usage_str);
}

int main(int argc, char *argv[])
{
	struct token **tokens;
	size_t num_tokens;
	int ret;
	int i;

	if (argc != 4) {
		usage();
		return -1;
	}

	ret = tokenize(argv[1], &tokens, &num_tokens);
	if (ret)
		return ret;

	struct ast_node *result = parse(tokens, num_tokens);
	visualize_ast(result);

	struct rand_stream *r = new_rand_stream(argv[3], 1024);
	size_t num_bytes;
	char *bytes = encode(result, r, &num_bytes);
	print_bytes(bytes, num_bytes);
	return 0;
}
