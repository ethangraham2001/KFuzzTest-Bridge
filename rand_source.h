#ifndef RAND_SOURCE_H
#define RAND_SOURCE_H 1

#include <stdlib.h>

/**
 * Iterates through some random blob.
 */
struct rand_source {
	const char *data;
	size_t data_len;
	size_t cursor;
};

static char next(struct rand_source *r)
{
	char ret = r->data[r->cursor];
	r->cursor = (r->cursor + 1) % r->data_len;
	return ret;
}

#endif /* RAND_SOURCE_H */
