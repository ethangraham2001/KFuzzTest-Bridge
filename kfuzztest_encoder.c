#include <asm-generic/errno-base.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kfuzztest_encoder.h"
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
	size_t req_size = buf->num_bytes + num_bytes;
	size_t new_size = buf->alloc_size;
	while (req_size > new_size)
		new_size *= 2;
	if (new_size != buf->alloc_size) {
		buf->alloc_size = new_size;
		buf->buffer = realloc(buf->buffer, buf->alloc_size);
		if (!buf->buffer)
			return -1;
	}
	memcpy(buf->buffer + buf->num_bytes, bytes, num_bytes);
	buf->num_bytes += num_bytes;
	return 0;
}

int append_byte(struct byte_buffer *buf, char c)
{
	return append_bytes(buf, &c, 1);
}

static int encode_le(struct byte_buffer *buf, uint64_t value, size_t byte_width)
{
	int ret;
	int i;
	for (i = 0; i < byte_width; ++i) {
		if ((ret = append_byte(buf, (uint8_t)((value >> (i * 8)) & 0xFF)))) {
			return ret;
		}
	}
	return 0;
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

int round_up_to_multiple(int x, int n)
{
	if (n == 0) {
		return x;
	}
	return ((x + n - 1) / n) * n;
}

struct region_info {
	const char *name;
	uint32_t offset;
	uint32_t size;
};

struct reloc_info {
	uint32_t src_reg;
	uint32_t offset;
	uint32_t dst_reg;
};

struct encoder_ctx {
	struct byte_buffer *payload;
	struct rand_stream *rand;

	struct region_info *regions;
	size_t num_regions;

	struct reloc_info *relocations;
	size_t num_relocations;

	size_t reg_offset;
	int curr_reg;
};

int pad_payload(struct encoder_ctx *ctx, size_t amount)
{
	int ret;

	if ((ret = pad(ctx->payload, amount)))
		return ret;
	ctx->reg_offset += amount;
	return 0;
}

int align_payload(struct encoder_ctx *ctx, size_t alignment)
{
	size_t pad_amount = round_up_to_multiple(ctx->payload->num_bytes, alignment) - ctx->payload->num_bytes;
	return pad_payload(ctx, pad_amount);
}

static int lookup_reg(struct encoder_ctx *ctx, const char *name)
{
	int i;

	for (i = 0; i < ctx->num_regions; i++) {
		if (strcmp(ctx->regions[i].name, name) == 0)
			return i;
	}
	return -1;
}

static void add_reloc(struct encoder_ctx *ctx, struct reloc_info reloc)
{
	ctx->relocations = realloc(ctx->relocations, ++ctx->num_relocations * sizeof(struct reloc_info));
	ctx->relocations[ctx->num_relocations - 1] = reloc;
	printf("added relocation: %zu now\n", ctx->num_relocations);
}

static int first_pass(struct encoder_ctx *ctx, struct ast_node *top_level)
{
	struct ast_program *prog;
	struct ast_node *reg;
	int i;

	prog = &top_level->data.program;
	ctx->regions = malloc(prog->num_members * sizeof(struct region_info));
	if (!ctx->regions)
		return -ENOMEM;
	ctx->num_regions = prog->num_members;

	for (i = 0; i < prog->num_members; i++) {
		reg = prog->members[i];
		/* Offset can only be determined after the second pass. */
		ctx->regions[i] = (struct region_info){
			.name = reg->data.region.name,
			.size = node_size(reg),
		};
	}
	return 0;
}
/**
 * Encodes a value node as little-endian. A value node is one that can be
 * directly written, i.e. a primitive, a pointer, or an array.
 */
static int encode_value_le(struct encoder_ctx *ctx, struct ast_node *node)
{
	char rand_char;
	size_t offset;
	int dst_reg;
	int ret;
	int i;
	int j;

	switch (node->type) {
	case NODE_ARRAY:
		for (i = 0; i < node->data.array.num_elems; i++) {
			for (int j = 0; j < node->data.array.elem_size; j++) {
				if ((ret = next_byte(ctx->rand, &rand_char)))
					return ret;
				if ((ret = append_byte(ctx->payload, rand_char)))
					return ret;
			}
		}
		ctx->reg_offset += node->data.array.num_elems * node->data.array.elem_size;
		break;
	case NODE_PRIMITIVE:
		for (i = 0; i < node->data.primitive.byte_width; i++) {
			if ((ret = next_byte(ctx->rand, &rand_char)))
				return ret;
			if ((ret = append_byte(ctx->payload, rand_char)))
				return ret;
		}
		ctx->reg_offset += node->data.primitive.byte_width;
		break;
	case NODE_POINTER:
		dst_reg = lookup_reg(ctx, node->data.pointer.points_to);
		if (dst_reg < 0)
			return -1;
		add_reloc(ctx, (struct reloc_info){
				       .src_reg = ctx->curr_reg, .offset = ctx->reg_offset, .dst_reg = dst_reg });
		/* Placeholder pointer value, as pointers are patched by KFuzzTest anyways. */
		encode_le(ctx->payload, UINTPTR_MAX, sizeof(uintptr_t));
		ctx->reg_offset += sizeof(uintptr_t);
		break;
	case NODE_PROGRAM:
	case NODE_REGION:
	default:
		return -1;
	}
	return 0;
}

static int encode_region(struct encoder_ctx *ctx, struct ast_region *reg)
{
	struct ast_node *child;
	int alignment;
	int ret;
	int i;

	ctx->reg_offset = 0;
	for (i = 0; i < reg->num_members; i++) {
		child = reg->members[i];
		align_payload(ctx, node_alignment(child));
		if ((ret = encode_value_le(ctx, child)))
			return ret;
	}
	return 0;
}

static int second_pass(struct encoder_ctx *ctx, struct ast_node *top_level)
{
	size_t size_before, size_after;
	struct ast_node *reg;
	int ret;
	int i;

	for (i = 0; i < ctx->num_regions; i++) {
		reg = top_level->data.program.members[i];
		align_payload(ctx, node_alignment(reg));

		ctx->curr_reg = i;
		size_before = ctx->payload->num_bytes;
		ctx->regions[i].offset = ctx->payload->num_bytes;
		if ((ret = encode_region(ctx, &reg->data.region)))
			return ret;
		size_after = ctx->payload->num_bytes;
		ctx->regions[i].size = size_after - size_before;
		pad_payload(ctx, 8); /* Poison with a KASAN granule. */
	}
	return 0;
}

/**
 * Encodes a region array into a byte buffer and returns it.
 */
static struct byte_buffer *encode_region_array(struct encoder_ctx *ctx)
{
	struct byte_buffer *reg_array;
	struct region_info info;
	int ret;
	int i;

	reg_array = new_byte_buffer(32);
	if (!reg_array)
		return NULL;

	if (encode_le(reg_array, ctx->num_regions, sizeof(uint32_t)))
		goto fail;

	for (i = 0; i < ctx->num_regions; i++) {
		info = ctx->regions[i];
		if (encode_le(reg_array, info.offset, sizeof(uint32_t)))
			goto fail;
		if (encode_le(reg_array, info.size, sizeof(uint32_t)))
			goto fail;
	}
	return reg_array;

fail:
	destroy_byte_buffer(reg_array);
	return NULL;
}

static struct byte_buffer *encode_reloc_table(struct encoder_ctx *ctx, size_t padding_amount)
{
	struct byte_buffer *reloc_table;
	struct reloc_info info;
	int i;

	reloc_table = new_byte_buffer(32);
	if (!reloc_table)
		return NULL;

	if (encode_le(reloc_table, ctx->num_relocations, sizeof(uint32_t)))
		goto fail;
	if (encode_le(reloc_table, padding_amount, sizeof(uint32_t)))
		goto fail;

	for (i = 0; i < ctx->num_relocations; i++) {
		info = ctx->relocations[i];
		if (encode_le(reloc_table, info.src_reg, sizeof(uint32_t)))
			goto fail;
		if (encode_le(reloc_table, info.offset, sizeof(uint32_t)))
			goto fail;
		if (encode_le(reloc_table, info.dst_reg, sizeof(uint32_t)))
			goto fail;
	}
	pad(reloc_table, padding_amount);
	return reloc_table;

fail:
	destroy_byte_buffer(reloc_table);
	return NULL;
}

static size_t reloc_table_size(struct encoder_ctx *ctx)
{
	return 2 * sizeof(uint32_t) + 3 * ctx->num_relocations * sizeof(uint32_t);
}

char *encode(struct ast_node *top_level, struct rand_stream *r, size_t *num_bytes)
{
	char prefix[8] = { 0xCE, 0xFA, 0x0B, 0X00, 0x00, 0x00, 0x00, 0x00 };
	struct byte_buffer *region_array;
	struct byte_buffer *reloc_table;
	struct byte_buffer *out;
	size_t header_size;
	int alignment;
	int i;

	/* Construct a map of regions. */
	struct encoder_ctx ctx = { 0 };
	if (first_pass(&ctx, top_level))
		return NULL;

	ctx.rand = r;
	ctx.payload = new_byte_buffer(32);
	if (!ctx.payload)
		return NULL;
	second_pass(&ctx, top_level);

	region_array = encode_region_array(&ctx);
	header_size = sizeof(prefix) + region_array->num_bytes + reloc_table_size(&ctx);
	alignment = node_alignment(top_level);
	reloc_table = encode_reloc_table(&ctx, round_up_to_multiple(header_size + 8, alignment) - header_size);

	out = new_byte_buffer(128);
	if (!out)
		return NULL;
	append_bytes(out, prefix, 8);
	append_bytes(out, region_array->buffer, region_array->num_bytes);
	append_bytes(out, reloc_table->buffer, reloc_table->num_bytes);
	append_bytes(out, ctx.payload->buffer, ctx.payload->num_bytes);
	*num_bytes = out->num_bytes;
	return out->buffer;
}
