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
#include "kfuzztest_input_parser.h"

const char *input = "data { arr[u8, 42] } foo { ptr[data] } bar { u32 ptr[foo] }";

enum token_type expected[] = {
	TOKEN_IDENTIFIER, TOKEN_LBRACE,	     TOKEN_KEYWORD_ARR, TOKEN_LBRACKET,	   TOKEN_KEYWORD_U8,
	TOKEN_COMMA,	  TOKEN_INTEGER,     TOKEN_RBRACKET,	TOKEN_RBRACE,	   TOKEN_IDENTIFIER,
	TOKEN_LBRACE,	  TOKEN_KEYWORD_PTR, TOKEN_LBRACKET,	TOKEN_IDENTIFIER,  TOKEN_RBRACKET,
	TOKEN_RBRACE,	  TOKEN_IDENTIFIER,  TOKEN_LBRACE,	TOKEN_KEYWORD_U32, TOKEN_KEYWORD_PTR,
	TOKEN_LBRACKET,	  TOKEN_IDENTIFIER,  TOKEN_RBRACKET,	TOKEN_RBRACE,	   TOKEN_EOF,
};

// Forward declaration for the recursive helper function
static void visualize_node(struct ast_node *node, int indent);

/**
 * @brief Prints a simple text representation of the AST.
 * @param node The root node of the AST to visualize.
 */
void visualize_ast(struct ast_node *node)
{
	if (!node) {
		printf("AST is NULL.\n");
		return;
	}
	visualize_node(node, 0);
}

/**
 * @brief Recursive helper to print a node and its children.
 * @param node The current node to print.
 * @param indent The current indentation level.
 */
static void visualize_node(struct ast_node *node, int indent)
{
	// 1. Print the indentation for the current level
	for (int i = 0; i < indent; i++) {
		printf("  ");
	}

	if (!node) {
		printf("(NULL Node)\n");
		return;
	}

	// 2. Switch on the node type to print its details
	switch (node->type) {
	case NODE_PROGRAM: {
		struct ast_program *prog = &node->data.program;
		printf("Program (%zu regions):\n", prog->num_members);
		for (size_t i = 0; i < prog->num_members; i++) {
			visualize_node(prog->members[i], indent + 1);
		}
		break;
	}
	case NODE_REGION: {
		struct ast_region *region = &node->data.region;
		printf("Region '%s' (%zu members):\n", region->name, region->num_members);
		for (size_t i = 0; i < region->num_members; i++) {
			visualize_node(region->members[i], indent + 1);
		}
		break;
	}
	case NODE_POINTER: {
		struct ast_pointer *ptr = &node->data.pointer;
		printf("Pointer -> '%s'\n", ptr->points_to);
		break;
	}
	case NODE_PRIMITIVE: {
		struct ast_primitive *prim = &node->data.primitive;
		printf("Primitive (width: %d)\n", prim->byte_width);
		break;
	}
	case NODE_ARRAY: {
		struct ast_array *arr = &node->data.array;
		printf("array (num_elems: %zu, width: %d))\n", arr->num_elems, arr->elem_size);
	}
	// Add cases for NODE_ARRAY etc. as you implement them
	default:
		printf("Unknown Node Type\n");
		break;
	}
}

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
		printf("got unexpected size %zu vs %zu\n", num_tokens, COUNT_OF(expected));
		return 1;
	}
	for (i = 0; i < num_tokens; i++) {
		if (expected[i] != tokens[i]->type) {
			printf("mismatch in token %d\n", i);
			return 1;
		}
	}
	printf("got expected lexer output.\n");

	struct ast_node *result = parse(tokens, num_tokens);
	visualize_ast(result);

	return 0;
}
