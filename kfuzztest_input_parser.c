#include "kfuzztest_input_lexer.h"

#include <string.h>

enum ast_node_type {
	NODE_PROGRAM,
	NODE_REGION,
	NODE_PRIMITIVE,
	NODE_POINTER,
};

struct ast_node; /* Forward declaration. */

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
		struct ast_region region;
		struct ast_pointer pointer;
		struct ast_primitive primitive;
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
	ret->type = NODE_PRIMITIVE;
	ret->data.primitive.byte_width = byte_width;
	return ret;
}

static struct ast_node *parse_ptr(struct parser *p)
{
	struct ast_node *ret;
	struct token *tok;
	if (advance(p)->type != TOKEN_KEYWORD_PTR)
		return NULL;
	if (advance(p)->type != TOKEN_LBRACKET)
		return NULL;

	tok = advance(p);
	if (tok->type != TOKEN_IDENTIFIER)
		return NULL;

	if (advance(p)->type != TOKEN_RBRACKET)
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
}

static struct ast_node *parse_region(struct parser *p)
{
	enum token_type tok_type;
	struct ast_node *node;
	struct ast_node *ret;
	struct token *tok;

	if (!match(p, TOKEN_IDENTIFIER))
		return NULL;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return NULL;

	advance(p); // Identifier;
	tok = advance(p);
	if (tok->type != TOKEN_LBRACE)
		return NULL;

	ret->data.region.name = strndup(tok->data.identifier.start, tok->data.identifier.length);
	while (!match(p, TOKEN_RBRACE)) {
		node = parse_type(p);
		/* TODO: add to members. */
	}
}
