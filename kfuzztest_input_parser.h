#ifndef KFUZZTEST_INPUT_PARSER_H
#define KFUZZTEST_INPUT_PARSER_H 1

#include <stdlib.h>

enum ast_node_type {
	NODE_PROGRAM,
	NODE_REGION,
	NODE_ARRAY,
	NODE_PRIMITIVE,
	NODE_POINTER,
};

struct ast_node; /* Forward declaration. */

struct ast_program {
	struct ast_node **members;
	size_t num_members;
};

struct ast_region {
	const char *name;
	struct ast_node **members;
	size_t num_members;
};

struct ast_pointer {
	const char *points_to;
};

struct ast_array {
	int elem_size;
	size_t num_elems;
};

struct ast_primitive {
	int byte_width;
};

struct ast_node {
	enum ast_node_type type;
	union {
		struct ast_program program;
		struct ast_region region;
		struct ast_primitive primitive;
		struct ast_array array;
		struct ast_pointer pointer;
	} data;
};

struct parser {
	struct token **tokens;
	size_t token_count;
	size_t curr_token;
};

struct ast_node *parse(struct token **tokens, size_t token_count);

#endif /* KFUZZTEST_INPUT_PARSER_H */
