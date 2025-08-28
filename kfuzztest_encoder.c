#include <asm-generic/errno-base.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kfuzztest_input_parser.h"
#include "rand_stream.h"
#include "byte_buffer.h"

#define KFUZZTEST_MAGIC 0xBFACE
#define KFUZZTEST_PROTO_VERSION 0
#define KFUZZTEST_POISON_SIZE 8

#define BUFSIZE_SMALL 32
#define BUFSIZE_LARGE 128

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

static void cleanup_ctx(struct encoder_ctx *ctx)
{
	if (ctx->regions)
		free(ctx->regions);
	if (ctx->relocations)
		free(ctx->relocations);
	if (ctx->payload)
		destroy_byte_buffer(ctx->payload);
}

int pad_payload(struct encoder_ctx *ctx, size_t amount)
{
	int ret;

	if ((ret = pad(ctx->payload, amount)))
		return ret;
	ctx->reg_offset += amount;
	return 0;
}

static int round_up_to_multiple(int x, int n)
{
	if (n == 0) {
		return x;
	}
	return ((x + n - 1) / n) * n;
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
	return -ENOENT;
}

static int add_reloc(struct encoder_ctx *ctx, struct reloc_info reloc)
{
	void *new_ptr = realloc(ctx->relocations, (ctx->num_relocations + 1) * sizeof(struct reloc_info));
	if (!new_ptr)
		return -ENOMEM;

	ctx->relocations = new_ptr;
	ctx->relocations[ctx->num_relocations] = reloc;
	ctx->num_relocations++;
	return 0;
}

static int build_region_map(struct encoder_ctx *ctx, struct ast_node *top_level)
{
	struct ast_program *prog;
	struct ast_node *reg;
	int i;

	if (top_level->type != NODE_PROGRAM)
		return -EINVAL;

	prog = &top_level->data.program;
	ctx->regions = malloc(prog->num_members * sizeof(struct region_info));
	if (!ctx->regions)
		return -ENOMEM;

	ctx->num_regions = prog->num_members;
	for (i = 0; i < ctx->num_regions; i++) {
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
	size_t array_size;
	char rand_char;
	int dst_reg;
	int ret;
	int i;

	switch (node->type) {
	case NODE_ARRAY:
		array_size = node->data.array.num_elems * node->data.array.elem_size;
		for (i = 0; i < array_size; i++) {
			if ((ret = next_byte(ctx->rand, &rand_char)))
				return ret;
			if ((ret = append_byte(ctx->payload, rand_char)))
				return ret;
		}
		ctx->reg_offset += array_size;
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
			return dst_reg;
		if ((ret = add_reloc(ctx, (struct reloc_info){ .src_reg = ctx->curr_reg,
							       .offset = ctx->reg_offset,
							       .dst_reg = dst_reg })))
			return ret;
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

static int encode_payload(struct encoder_ctx *ctx, struct ast_node *top_level)
{
	struct ast_node *reg;
	int ret;
	int i;

	for (i = 0; i < ctx->num_regions; i++) {
		reg = top_level->data.program.members[i];
		align_payload(ctx, node_alignment(reg));

		ctx->curr_reg = i;
		ctx->regions[i].offset = ctx->payload->num_bytes;
		if ((ret = encode_region(ctx, &reg->data.region)))
			return ret;
		pad_payload(ctx, KFUZZTEST_POISON_SIZE);
	}
	return 0;
}

static struct byte_buffer *encode_region_array(struct encoder_ctx *ctx)
{
	struct byte_buffer *reg_array;
	struct region_info info;
	int i;

	reg_array = new_byte_buffer(BUFSIZE_SMALL);
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

	reloc_table = new_byte_buffer(BUFSIZE_SMALL);
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

int encode(struct ast_node *top_level, struct rand_stream *r, size_t *num_bytes, struct byte_buffer **ret)
{
	struct byte_buffer *region_array = NULL;
	struct byte_buffer *final_buffer = NULL;
	struct byte_buffer *reloc_table = NULL;
	size_t header_size;
	int alignment;
	int retcode;

	struct encoder_ctx ctx = { 0 };
	if (build_region_map(&ctx, top_level))
		goto fail_early;

	ctx.rand = r;
	ctx.payload = new_byte_buffer(32);
	if (!ctx.payload)
		goto fail_early;
	if (encode_payload(&ctx, top_level))
		goto fail_early;

	region_array = encode_region_array(&ctx);
	if (!region_array)
		goto fail_early;

	header_size = sizeof(uint64_t) + region_array->num_bytes + reloc_table_size(&ctx);
	alignment = node_alignment(top_level);
	reloc_table = encode_reloc_table(&ctx, round_up_to_multiple(header_size + KFUZZTEST_POISON_SIZE, alignment) -
						       header_size);
	if (!reloc_table)
		goto fail_early;

	final_buffer = new_byte_buffer(BUFSIZE_LARGE);
	if (!final_buffer)
		goto fail_early;

	if ((retcode = encode_le(final_buffer, KFUZZTEST_MAGIC, sizeof(uint32_t))) ||
	    (retcode = encode_le(final_buffer, KFUZZTEST_PROTO_VERSION, sizeof(uint32_t))) ||
	    (retcode = append_bytes(final_buffer, region_array->buffer, region_array->num_bytes)) ||
	    (retcode = append_bytes(final_buffer, reloc_table->buffer, reloc_table->num_bytes)) ||
	    (retcode = append_bytes(final_buffer, ctx.payload->buffer, ctx.payload->num_bytes))) {
		destroy_byte_buffer(final_buffer);
		goto fail_early;
	}

	*num_bytes = final_buffer->num_bytes;
	*ret = final_buffer;

fail_early:
	if (region_array)
		destroy_byte_buffer(region_array);
	if (reloc_table)
		destroy_byte_buffer(reloc_table);
	cleanup_ctx(&ctx);
	return retcode;
}
