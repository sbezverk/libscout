#include <stdlib.h>

#include "iter.h"
#include "libscout.h"

iterator_t *iterator_init(void *base_address, size_t element_size,
                          size_t total_number) {
  iterator_t *iter = NULL;
  if (!base_address || element_size <= 0 || total_number <= 0) {
    return iter;
  }
  iter = malloc(sizeof(iterator_t));
  if (!iter) {
    return iter;
  }
  iter->base_address = base_address;
  iter->current_index = 0;
  iter->element_size = element_size;
  iter->total_number = total_number;

  return iter;
}

void *iterator_next(iterator_t *iter) {
  if (iter->current_index >= iter->total_number) {
    return NULL;
  }
  void *return_data =
      iter->base_address + (iter->element_size * iter->current_index);
  iter->current_index++;

  return return_data;
}

void iterator_free(iterator_t *iter) {
  if (iter) {
    free(iter);
    iter = NULL;
  }
}