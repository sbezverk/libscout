#pragma once

#include "avl/avl_tree.h"
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#define THREAD_RETURN(rc) return (void *)(long)rc
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
  // Buffer of pointers to opaque objects
  void *buffer[MAX_LIB_ENTRY];
  size_t head;
  size_t tail;
  pthread_mutex_t write_mutex;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
  int producer_count;
  _Atomic bool producer_done;
} mpsc_buffer_t;

typedef struct producer_thread_ctx_ {
  void *user_data;
  mpsc_buffer_t *buffer;
} producer_thread_ctx_t;

typedef struct main_thread_ctx_ {
  char *exe_file_name;
  char *lib_path;
} main_thread_ctx_t;

typedef struct thread_user_data_ {
  int fd;
  char *lib_path;
} thread_user_data_t;

void *get_undefined_sym(void *arg);
void *resolve_undefined_sym(void *arg);

int create_producer_thread_ctx(producer_thread_ctx_t **ctx);

void destroy_producer_thread_ctx(producer_thread_ctx_t *ctx);