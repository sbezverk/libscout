#pragma once

#include "avl/avl_tree.h"
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_LIB_ENTRY 2

typedef struct sym_node_ {
  avl2_node_type node;
  char *sym_name;
} sym_node_t;

typedef struct lib_node_ {
  avl2_node_type node;
  char lib_name[PATH_MAX];
  avl_tree_type *defined;
  avl_tree_type *undefined;
} lib_node_t;

typedef struct sym_cache_ {
  avl2_node_type node;
  avl_tree_type *cache;
} sym_cache_t;

typedef struct dependency_tree_ {
  avl2_node_type node;
  avl_tree_type *dependecy_tree;
} dependency_tree_t;

int create_lib_node(char *lib_name, lib_node_t **lib_node);

void destroy_lib_node(lib_node_t *lib_node);

int populate_node_symtable(int fd, lib_node_t *lib_node, bool populate_defined,
                           bool populate_undefined);

int find_dependency(lib_node_t *lib_node, dependency_tree_t **deps,
                    char *lib_path);

typedef struct mpsc_buffer_ {
  char buffer[MAX_LIB_ENTRY][PATH_MAX];
  size_t head;
  size_t tail;
  pthread_mutex_t write_mutex;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
  bool producer_done;
} mpsc_buffer_t;

typedef struct search_thread_ctx_ {
  char current_path[PATH_MAX];
  mpsc_buffer_t *data;
  pthread_t search_t;
} search_thread_ctx_t;