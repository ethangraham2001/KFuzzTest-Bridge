#include <asm-generic/errno-base.h>

#include <string.h>
#include <stdio.h>

#include "kfuzztest_input_lexer.h"
#include "kfuzztest_input_parser.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static struct token *peek(struct parser *p)
{
	return p->tokens[p->curr_token];
}

static struct token *advance(struct parser *p)
{
	struct token *tok = peek(p);
	p->curr_token++;
	return tok;
}

static struct token *consume(struct parser *p, enum token_type type, const char *err_msg)
{
	if (peek(p)->type != type) {
		printf("parser failure: %s\n", err_msg);
		return NULL;
	}
	return advance(p);
}

static bool match(struct parser *p, enum token_type t)
{
	struct token *tok = peek(p);
	return tok->type == t;
}

static int parse_primitive(struct parser *p, struct ast_node **node_ret)
{
	struct ast_node *ret;
	struct token *tok;
	int byte_width;

	tok = advance(p);
	byte_width = primitive_byte_width(tok->type);
	if (!byte_width)
		return -EINVAL;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return -ENOMEM;

	ret->type = NODE_PRIMITIVE;
	ret->data.primitive.byte_width = byte_width;
	*node_ret = ret;
	return 0;
}

static int parse_ptr(struct parser *p, struct ast_node **node_ret)
{
	const char *points_to;
	struct ast_node *ret;
	struct token *tok;
	if (!consume(p, TOKEN_KEYWORD_PTR, "expected 'ptr'"))
		return -EINVAL;
	if (!consume(p, TOKEN_LBRACKET, "expected '['"))
		return -EINVAL;

	tok = advance(p);
	if (tok->type != TOKEN_IDENTIFIER)
		return -EINVAL;

	if (!consume(p, TOKEN_RBRACKET, "expected ']'"))
		return -EINVAL;

	ret = malloc(sizeof(*ret));
	ret->type = NODE_POINTER;

	points_to = strndup(tok->data.identifier.start, tok->data.identifier.length);
	if (!points_to) {
		free(ret);
		return -EINVAL;
	}

	ret->data.pointer.points_to = points_to;
	*node_ret = ret;
	return 0;
}

static int parse_arr(struct parser *p, struct ast_node **node_ret)
{
	struct token *type, *num_elems;
	struct ast_node *ret;

	if (!consume(p, TOKEN_KEYWORD_ARR, "expected 'arr'") || !consume(p, TOKEN_LBRACKET, "expected '['"))
		return -EINVAL;

	type = advance(p);
	if (!is_primitive(type))
		return -EINVAL;

	if (!consume(p, TOKEN_COMMA, "expected ','"))
		return -EINVAL;

	num_elems = advance(p);
	if (num_elems->type != TOKEN_INTEGER)
		return -EINVAL;

	if (!consume(p, TOKEN_RBRACKET, "expected ']'"))
		return -EINVAL;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return -ENOMEM;

	ret->type = NODE_ARRAY;
	ret->data.array.num_elems = num_elems->data.integer;
	ret->data.array.elem_size = primitive_byte_width(type->type);
	*node_ret = ret;
	return 0;
}

static int parse_type(struct parser *p, struct ast_node **node_ret)
{
	if (is_primitive(peek(p)))
		return parse_primitive(p, node_ret);

	if (peek(p)->type == TOKEN_KEYWORD_PTR)
		return parse_ptr(p, node_ret);

	if (peek(p)->type == TOKEN_KEYWORD_ARR)
		return parse_arr(p, node_ret);

	return -EINVAL;
}

static int parse_region(struct parser *p, struct ast_node **node_ret)
{
	struct token *tok, *identifier;
	struct ast_region *region;
	struct ast_node *node;
	struct ast_node *ret;
	int err;
	int i;

	identifier = consume(p, TOKEN_IDENTIFIER, "expected identifier");
	if (!identifier)
		return -EINVAL;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return -ENOMEM;

	tok = advance(p);
	if (tok->type != TOKEN_LBRACE)
		goto fail_early;

	region = &ret->data.region;
	region->name = strndup(identifier->data.identifier.start, identifier->data.identifier.length);
	if (!region->name) {
		err = -ENOMEM;
		goto fail_early;
	}

	region->num_members = 0;
	while (!match(p, TOKEN_RBRACE)) {
		err = parse_type(p, &node);
		if (err)
			goto fail;
		region->members = realloc(region->members, ++region->num_members * sizeof(struct ast_node *));
		region->members[region->num_members - 1] = node;
	}

	if (!consume(p, TOKEN_RBRACE, "expected '}'") || !consume(p, TOKEN_SEMICOLON, "expected ';'")) {
		err = -EINVAL;
		goto fail;
	}

	ret->type = NODE_REGION;
	*node_ret = ret;
	return 0;

fail:
	for (i = 0; i < region->num_members; i++)
		free(region->members[i]);
	free((void *)region->name);
	free(region->members);
fail_early:
	free(ret);
	return err;
}

static int parse_program(struct parser *p, struct ast_node **node_ret)
{
	struct ast_program *prog;
	struct ast_node *reg;
	struct ast_node *ret;
	void *new_ptr;
	int err;
	int i;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return -ENOMEM;
	ret->type = NODE_PROGRAM;

	prog = &ret->data.program;
	prog->num_members = 0;
	prog->members = NULL;
	while (!match(p, TOKEN_EOF)) {
		err = parse_region(p, &reg);
		if (err)
			goto fail;

		new_ptr = realloc(prog->members, ++prog->num_members * sizeof(struct ast_node *));
		if (!new_ptr) {
			err = -ENOMEM;
			goto fail;
		}
		prog->members = new_ptr;
		prog->members[prog->num_members - 1] = reg;
	}

	*node_ret = ret;
	return 0;

fail:
	for (i = 0; i < prog->num_members; i++)
		free(prog->members[i]);
	free(prog->members);
	free(ret);
	return err;
}

size_t node_alignment(struct ast_node *node)
{
	int max_alignment = 1;

	switch (node->type) {
	case NODE_PROGRAM:
		for (int i = 0; i < node->data.program.num_members; i++)
			max_alignment = MAX(max_alignment, node_alignment(node->data.program.members[i]));
		return max_alignment;
	case NODE_REGION:
		for (int i = 0; i < node->data.region.num_members; i++)
			max_alignment = MAX(max_alignment, node_alignment(node->data.region.members[i]));
		return max_alignment;
	case NODE_ARRAY:
		return node->data.array.elem_size;
	case NODE_PRIMITIVE:
		/* Primitives are aligned to their size. */
		return node->data.primitive.byte_width;
	case NODE_POINTER:
		return sizeof(uintptr_t);
	}

	/* Anything should be at least 1-byte-aligned. */
	return 1;
}

size_t node_size(struct ast_node *node)
{
	size_t total = 0;

	switch (node->type) {
	case NODE_PROGRAM:
		for (int i = 0; i < node->data.program.num_members; i++)
			total += node_size(node->data.program.members[i]);
		return total;
	case NODE_REGION:
		for (int i = 0; i < node->data.region.num_members; i++)
			total += node_size(node->data.region.members[i]);
		return total;
	case NODE_ARRAY:
		return node->data.array.elem_size * node->data.array.num_elems;
	case NODE_PRIMITIVE:
		return node->data.primitive.byte_width;
	case NODE_POINTER:
		return sizeof(uintptr_t);
	}
	return 0;
}

int parse(struct token **tokens, size_t token_count, struct ast_node **node_ret)
{
	struct parser p = { .tokens = tokens, .token_count = token_count, .curr_token = 0 };
	return parse_program(&p, node_ret);
}
