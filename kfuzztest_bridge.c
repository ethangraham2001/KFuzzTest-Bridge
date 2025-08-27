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
	"data { arr[u8, size] } foo { ptr[data] } bar { u32 a, ptr[foo] }";

int main(void)
{
	struct token *tokens;
	size_t num_tokens;
	int ret;
	int i;

	ret = tokenize(input, &tokens, &num_tokens);
	if (ret)
		return ret;

	for (i = 0; i < num_tokens; i++)
		printf("%s, ", token_names[tokens[i].type]);
}
