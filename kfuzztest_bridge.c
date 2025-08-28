// SPDX-License-Identifier: GPL-2.0
/*
 * KFuzzTest tool for sending inputs into a KFuzzTest harness.
 *
 * Copyright 2025 Google LLC
 */
#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "debug.h"
#include "byte_buffer.h"
#include "kfuzztest_input_lexer.h"
#include "kfuzztest_input_parser.h"
#include "kfuzztest_encoder.h"
#include "rand_stream.h"

const char *usage_str = "usage: "
			"./kfuzztest-bridge <program-description> <fuzz-target-name> <input-file>\n"
			"for more detailed information see <docs>";

static int invoke_one(const char *input_fmt, const char *fuzz_target, const char *input_filepath);

int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("%s\n", usage_str);
		return -1;
	}

	return invoke_one(argv[1], argv[2], argv[3]);
}

static int invoke_kfuzztest_target(const char *target_name, const char *data, size_t data_size)
{
	ssize_t bytes_written;
	char buf[256];
	int ret;
	int fd;

	ret = snprintf(buf, sizeof(buf), "/sys/kernel/debug/kfuzztest/%s/input", target_name);
	if (ret < 0)
		return ret;

	fd = openat(AT_FDCWD, buf, O_WRONLY, 0);
	if (fd < 0)
		return fd;

	bytes_written = write(fd, (void *)data, data_size);
	if (bytes_written < 0) {
		close(fd);
		return bytes_written;
	}

	if (close(fd) != 0)
		return 1;
	return 0;
}

static int invoke_one(const char *input_fmt, const char *fuzz_target, const char *input_filepath)
{
	struct ast_node *ast_prog;
	struct rand_stream *rs;
	struct token **tokens;
	size_t num_tokens;
	size_t num_bytes;
	struct byte_buffer *bytes;
	int ret;

	ret = tokenize(input_fmt, &tokens, &num_tokens);
	if (ret)
		return ret;

	ast_prog = parse(tokens, num_tokens);

	rs = new_rand_stream(input_filepath, 1024);
	bytes = encode(ast_prog, rs, &num_bytes);
	if (!bytes)
		return -EINVAL;
	print_bytes(bytes->buffer, num_bytes);

	ret = invoke_kfuzztest_target(fuzz_target, bytes->buffer, num_bytes);
	destroy_byte_buffer(bytes);
	return ret;
}
