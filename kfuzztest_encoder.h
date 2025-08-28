#ifndef KFUZZTEST_ENCODER_H
#define KFUZZTEST_ENCODER_H

#include "rand_source.h"
#include "kfuzztest_input_parser.h"

char *encode(struct ast_node *top_level, struct rand_stream *r, size_t *num_bytes);

#endif /* KFUZZTEST_ENCODER_H */
