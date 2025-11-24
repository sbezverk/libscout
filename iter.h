#pragma once

#include <stddef.h>

typedef struct iterator_ {
  char *base_address;
  size_t element_size;
  size_t total_number;
  size_t current_index;
} iterator_t;

iterator_t *iterator_init(void *base_address, size_t element_size,
                          size_t total_number);

void iterator_free(iterator_t *iter);

void *iterator_next(iterator_t *iter);