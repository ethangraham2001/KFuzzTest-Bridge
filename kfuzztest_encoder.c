#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kfuzztest_input_parser.h"
#include "rand_source.h"

struct byte_buffer {
	char *buffer;
	size_t num_bytes;
	size_t alloc_size;
};

struct byte_buffer *new_byte_buffer(size_t initial_size)
{
	struct byte_buffer *ret;
	size_t alloc_size = initial_size >= 8 ? initial_size : 8;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return NULL;

	ret->alloc_size = alloc_size;
	ret->buffer = malloc(alloc_size);
	if (!ret->buffer) {
		free(ret);
		return NULL;
	}
	ret->num_bytes = 0;
	return ret;
}

static void destroy_byte_buffer(struct byte_buffer *buf)
{
	free(buf->buffer);
	free(buf);
}

int append_bytes(struct byte_buffer *buf, const char *bytes, size_t num_bytes)
{
	size_t req_size = buf->alloc_size + num_bytes;
	size_t new_size = buf->alloc_size;
	while (req_size > buf->alloc_size)
		new_size *= 2;
	if (new_size != buf->alloc_size) {
		buf->alloc_size = new_size;
		buf->buffer = realloc(buf->buffer, buf->alloc_size);
		if (!buf->buffer)
			return -1;
	}
	memcpy(buf->buffer + num_bytes, bytes, num_bytes);
	buf->num_bytes += num_bytes;
	return 0;
}

int append_byte(struct byte_buffer *buf, char c)
{
	return append_bytes(buf, &c, 1);
}

int pad(struct byte_buffer *buf, size_t num_padding)
{
	int ret;
	size_t i;
	for (i = 0; i < num_padding; i++)
		if ((ret = append_byte(buf, 0)))
			return ret;
	return 0;
}

/**
 * Sequential lookup of region ID from region name.
 */
struct region_lookup {
	const char **entries;
	size_t num_entries;
};

int lookup(struct region_lookup *l, const char *name)
{
	int i;
	for (int i = 0; i < l->num_entries; i++)
		if (strcmp(name, l->entries[i]) == 0)
			return i;
	return -1;
}

int insert(struct region_lookup *l, const char *name)
{
	l->entries = realloc(l->entries, ++l->num_entries * sizeof(const char *));
	if (!l->entries)
		return -1;
	l->entries[l->num_entries - 1] = name;
	return 0;
}

/**
 * Encodes a node as little-endian.
 */
static int encode_node_le(struct byte_buffer *buf, struct rand_source *r, struct ast_node *node)
{
	char rand_char;
	int ret;
	int i;
	int j;

	switch (node->type) {
	case NODE_PROGRAM:
		for (i = 0; i < node->data.program.num_members; i++)
			if ((ret = encode_node_le(buf, r, node->data.program.members[i])))
				return ret;
		break;
	case NODE_REGION:
		for (i = 0; i < node->data.region.num_members; i++)
			if ((ret = encode_node_le(buf, r, node->data.region.members[i])))
				return ret;
		break;
	case NODE_ARRAY:
		for (i = 0; i < node->data.array.num_elems; i++) {
			for (int j = 0; j < node->data.array.elem_size; j++) {
				if ((ret = append_byte(buf, next(r))))
					return ret;
			}
		}
		break;
	case NODE_PRIMITIVE:
		for (i = 0; i < node->data.primitive.byte_width; i++)
			if ((ret = append_byte(buf, next(r))))
				return ret;
		break;
	case NODE_POINTER:
		/* Placeholder pointer value, 0xFF...FF. */
		for (i = 0; i < sizeof(uintptr_t); i++)
			if ((ret = append_byte(buf, 0xFF)))
				return ret;
		break;
	}
	return 0;
}

char *encode(struct ast_node *top_level, struct rand_source *r)
{
	struct byte_buffer *payload = new_byte_buffer(32);
}
