#ifndef RAND_SOURCE_H
#define RAND_SOURCE_H 1

#include <stdlib.h>
#include <stdio.h>

struct rand_stream {
	FILE *source;
	char *buffer;
	size_t buffer_size;
	size_t buffer_pos;
};

struct rand_stream *new_rand_stream(const char *path_to_file, size_t cache_size);

int next_byte(struct rand_stream *rs, char *ret);

#endif /* RAND_SOURCE_H */
