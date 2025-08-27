#include "kfuzztest_input_lexer.h"

#include <string.h>
#include <stdio.h>

enum ast_node_type {
	NODE_PROGRAM,
	NODE_REGION,
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

struct ast_primitive {
	int byte_width;
};

struct ast_node {
	enum ast_node_type type;
	union {
		struct ast_program program;
		struct ast_region region;
		struct ast_primitive primitive;
		struct ast_pointer pointer;
	} data;
};

struct parser {
	struct token **tokens;
	size_t token_count;
	size_t curr_token;
};

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
		printf("%s\n", err_msg);
		return NULL;
	}
	return advance(p);
}
static void retreat(struct parser *p)
{
	p->curr_token--;
}

static bool match(struct parser *p, enum token_type t)
{
	struct token *tok = peek(p);
	return tok->type == t;
}

static struct ast_node *parse_primitive(struct parser *p)
{
	struct ast_node *ret;
	struct token *tok;
	int byte_width;
	tok = advance(p);
	switch (tok->type) {
	case TOKEN_KEYWORD_U8:
		byte_width = 1;
		break;
	case TOKEN_KEYWORD_U16:
		byte_width = 2;
		break;
	case TOKEN_KEYWORD_U32:
		byte_width = 4;
		break;
	case TOKEN_KEYWORD_U64:
		byte_width = 8;
		break;
	default:
		return NULL;
	}

	ret = malloc(sizeof(*ret));
	if (!ret)
		return NULL;
	ret->type = NODE_PRIMITIVE;
	ret->data.primitive.byte_width = byte_width;
	return ret;
}

static struct ast_node *parse_ptr(struct parser *p)
{
	struct ast_node *ret;
	struct token *tok;
	if (!consume(p, TOKEN_KEYWORD_PTR, "expected 'ptr'"))
		return NULL;
	if (!consume(p, TOKEN_LBRACKET, "expected '['"))
		return NULL;

	tok = advance(p);
	if (tok->type != TOKEN_IDENTIFIER)
		return NULL;

	if (!consume(p, TOKEN_RBRACKET, "expected ']'"))
		return NULL;

	ret = malloc(sizeof(*ret));
	ret->type = NODE_POINTER;
	ret->data.pointer.points_to = strndup(tok->data.identifier.start, tok->data.identifier.length);
	return ret;
}

static struct ast_node *parse_type(struct parser *p)
{
	if (is_primitive(peek(p))) {
		return parse_primitive(p);
	}
	if (peek(p)->type == TOKEN_KEYWORD_PTR) {
		return parse_ptr(p);
	}
	return NULL;
}

static struct ast_node *parse_region(struct parser *p)
{
	struct token *tok, *identifier;
	struct ast_region *region;
	enum token_type tok_type;
	struct ast_node *node;
	struct ast_node *ret;
	int i;

	if (!match(p, TOKEN_IDENTIFIER))
		return NULL;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return NULL;

	identifier = advance(p);
	tok = advance(p);
	if (tok->type != TOKEN_LBRACE)
		goto fail_early;

	region = &ret->data.region;
	region->name = strndup(identifier->data.identifier.start, identifier->data.identifier.length);
	if (!region->name)
		goto fail_early;

	region->num_members = 0;
	while (!match(p, TOKEN_RBRACE)) {
		node = parse_type(p);
		if (!node)
			goto fail;
		/* TODO: Handle realloc failure. */
		region->members = realloc(region->members, ++region->num_members * sizeof(struct ast_node *));
		region->members[region->num_members - 1] = node;
	}

	if (!consume(p, TOKEN_RBRACE, "expected '}'"))
		goto fail;

	ret->type = NODE_REGION;
	return ret;

fail:
	for (i = 0; i < region->num_members; i++)
		free(region->members[i]);
	free((void *)region->name);
	free(region->members);
fail_early:
	free(ret);
	return NULL;
}

static struct ast_node *parse_program(struct parser *p)
{
	struct ast_program *prog;
	struct ast_node *reg;
	struct ast_node *ret;
	int i;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return NULL;
	ret->type = NODE_PROGRAM;

	prog = &reg->data.program;
	prog->num_members = 0;
	prog->members = NULL;
	while (!match(p, TOKEN_EOF)) {
		reg = parse_region(p);
		if (!reg)
			goto fail;
		/* TODO: Handle realloc failure. */
		prog->members = realloc(prog->members, ++prog->num_members * sizeof(struct ast_node *));
	}

	return ret;

fail:
	for (i = 0; i < prog->num_members; i++)
		free(prog->members[i]);
	free(prog->members);
	free(ret);
	return NULL;
}

static struct ast_node *parse(struct token **tokens, size_t token_count)
{
	struct parser p = { .tokens = tokens, .token_count = token_count, .curr_token = 0 };
	return parse_program(&p);
}
