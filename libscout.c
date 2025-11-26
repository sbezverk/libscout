#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "avl/avl_tree.h"
#include "elflib.h"
#include "iter.h"
#include "libscout.h"

bool buffer_is_empty(mpsc_buffer_t *b) { return b->head == b->tail; }

bool buffer_is_full(mpsc_buffer_t *b) {
  return ((b->tail + 1) % MAX_LIB_ENTRY) == b->head;
}

bool is_buffer_producer_done(mpsc_buffer_t *buffer) {
  return atomic_load(&buffer->producer_done);
}

bool is_buffer_producer_shutdown_requested(mpsc_buffer_t *buffer) {
  return atomic_load_explicit(&buffer->shutdown_request, memory_order_acquire);
}

int sym_name_compare(struct avl2_node_type_ *n1, struct avl2_node_type_ *n2) {
  if (!n1 || !n2) {
    return 0;
  }
  return strcmp(((sym_node_t *)n1)->sym_name, ((sym_node_t *)n2)->sym_name);
}

int lib_name_compare(struct avl2_node_type_ *n1, struct avl2_node_type_ *n2) {
  if (!n1 || !n2) {
    return 0;
  }
  return strcmp(((lib_node_t *)n1)->lib_name, ((lib_node_t *)n2)->lib_name);
}

int init_tree(avl_tree_type **tree, avl2_compare_type avl_compare,
              avl_option_type avl_option) {
  int rc = EXIT_SUCCESS;
  avl_tree_type *t = NULL;
  if (!tree) {
    return EINVAL;
  }
  t = calloc(1, sizeof(avl_tree_type));
  if (!t) {
    return ENOMEM;
  }
  if (avl2_init(t, avl_compare, AVL_OPTION_DEFAULT) == NULL) {
    rc = errno;
  }
  if (rc != EXIT_SUCCESS) {
    free(t);
    t = NULL;
  } else {
    *tree = t;
  }

  return rc;
}

void destroy_lib_node(lib_node_t *node) {
  if (node) {
    if (node->undefined) {
      avl2_destroy(node->undefined, destroy_sym_node);
      free(node->undefined);
      node->undefined = NULL;
    }
    if (node->defined) {
      avl2_destroy(node->defined, destroy_sym_node);
      free(node->defined);
      node->defined = NULL;
    }
    free(node);
    node = NULL;
  }
}

void send_sym_to_buffer(mpsc_buffer_t *buffer, char *sym) {
  pthread_mutex_lock(&buffer->write_mutex);
  while (buffer_is_full(buffer)) {
    pthread_cond_wait(&buffer->not_full, &buffer->write_mutex);
  }

  buffer->buffer[buffer->tail] = calloc(1, strlen(sym) + 1);
  memcpy(buffer->buffer[buffer->tail], sym, strlen(sym));
  if (buffer->tail + 1 < MAX_LIB_ENTRY) {
    buffer->tail++;
  } else {
    buffer->tail = 0;
  }
  pthread_cond_signal(&buffer->not_empty);
  pthread_mutex_unlock(&buffer->write_mutex);
}

void *get_undefined_sym(void *arg) {
  int rc = EXIT_SUCCESS;
  elf_file_descr_t *elf_file_descr = NULL;
  char *name = NULL;
  producer_thread_ctx_t *ctx = (producer_thread_ctx_t *)arg;
  thread_user_data_t *data = (thread_user_data_t *)ctx->user_data;

  printf("><SB> %s() producer thread for file: %s ctx at: %p started\n",
         __func__, data->file_name, (void *)ctx);
  rc = process_elf_file(data->file_name, &elf_file_descr);
  if (rc == EXIT_SUCCESS) {
    for (int i = 0; i < elf_file_descr->elf_sym_tbl_num && rc == EXIT_SUCCESS;
         i++) {
      if (!elf_file_descr->elf_sym_tbls[i].is_dynamic) {
        // Only interested in .dynsym table.
        continue;
      }
      size_t entry_size = 0;
      if (elf_file_descr->is_64bit) {
        entry_size = sizeof(Elf64_Sym);
      } else {
        entry_size = sizeof(Elf32_Sym);
      }
      iterator_t *iter = iterator_init(
          (void *)elf_file_descr->elf_sym_tbls[i].elf_sym_table, entry_size,
          elf_file_descr->elf_sym_tbls[i].elf_sym_tbl_num_entry);
      void *entry = NULL;
      entry = iterator_next(iter);
      while (entry && rc == EXIT_SUCCESS) {
        if (elf_sym_get_string(elf_file_descr,
                               elf_file_descr->elf_sym_tbls[i].is_dynamic,
                               elf_sym_get_st_name(elf_file_descr, entry),
                               &name) == EXIT_SUCCESS) {
          if (strlen(name) == 0) {
            // If name length is 0 ignoring entry, free it
            if (name) {
              free(name);
              name = NULL;
            }
          } else {
            if (elf_sym_is_global(elf_file_descr, entry)) {
              // Interested only in Global symbols
              if (!elf_sym_is_defined(elf_file_descr, entry)) {
                // Interested only in Undefined symbols
                send_sym_to_buffer(ctx->buffer, name);
              }
            }
            // Sym
            if (name) {
              free(name);
              name = NULL;
            }
          }
        }
        entry = iterator_next(iter);
      }
      iterator_free(iter);
    }
    if (elf_file_descr) {
      free_elf_file_descr(elf_file_descr);
      elf_file_descr = NULL;
    }
  }

  printf("><SB> %s() producer thread for file: %s ctx at: %p finished\n",
         __func__, data->file_name, (void *)ctx);

  pthread_mutex_lock(&ctx->buffer->write_mutex);
  ctx->buffer->producer_count--;
  if (ctx->buffer->producer_count <= 0) {
    printf("><SB> %s() Last producer, sending termination signal.\n", __func__);
    // Sending broadcast to the consumer only if no more producers left,
    // otherwise just decrement the producer counter and return.
    atomic_store_explicit(&ctx->buffer->producer_done, true,
                          memory_order_release);
    pthread_cond_broadcast(&ctx->buffer->not_empty);
  }
  pthread_mutex_unlock(&ctx->buffer->write_mutex);

  // Producer thread has finished, releasing thread specific allocations

  free(data->file_name);
  data->file_name = NULL;
  free(data->lib_path);
  data->lib_path = NULL;
  free(data);
  data = NULL;
  free(ctx);

  THREAD_RETURN(rc);
}

int create_producer_thread_ctx(producer_thread_ctx_t **ctx) {
  int rc = EXIT_SUCCESS;
  mpsc_buffer_t *buffer = NULL;
  producer_thread_ctx_t *ctx_n = NULL;

  ctx_n = calloc(1, sizeof(producer_thread_ctx_t));
  if (!ctx_n) {
    rc = ENOMEM;
  }
  if (rc == EXIT_SUCCESS) {
    buffer = calloc(1, sizeof(mpsc_buffer_t));
    if (!buffer) {
      rc = ENOMEM;
    }
  }
  if (rc == EXIT_SUCCESS) {
    buffer->head = 0;
    buffer->tail = 0;
    pthread_cond_init(&buffer->not_empty, NULL);
    pthread_cond_init(&buffer->not_full, NULL);
    pthread_mutex_init(&buffer->write_mutex, NULL);
    atomic_store_explicit(&buffer->producer_done, false, memory_order_release);
    atomic_store_explicit(&buffer->shutdown_request, false,
                          memory_order_release);
  }
  if (rc == EXIT_SUCCESS) {
    ctx_n->buffer = buffer;
    *ctx = ctx_n;
  } else {
    if (ctx_n) {
      ctx_n->buffer = NULL;
      free(ctx_n);
      ctx_n = NULL;
    }
    if (buffer) {
      pthread_cond_destroy(&buffer->not_empty);
      pthread_cond_destroy(&buffer->not_full);
      pthread_mutex_destroy(&buffer->write_mutex);
      free(buffer);
      buffer = NULL;
    }
  }
  return rc;
}

void destroy_producer_thread_ctx(producer_thread_ctx_t *ctx) {
  if (ctx) {
    if (ctx->buffer) {
      pthread_cond_destroy(&ctx->buffer->not_empty);
      pthread_cond_destroy(&ctx->buffer->not_full);
      pthread_mutex_destroy(&ctx->buffer->write_mutex);
      for (int i = 0; i < MAX_LIB_ENTRY; i++) {
        if (ctx->buffer->buffer[i]) {
          free(ctx->buffer->buffer[i]);
          ctx->buffer->buffer[i] = NULL;
        }
      }
      free(ctx->buffer);
      ctx->buffer = NULL;
    }
    ctx->buffer = NULL;
    if (ctx->user_data) {
      free(ctx->user_data);
      ctx->user_data = NULL;
    }
    free(ctx);
    ctx = NULL;
  }
}

int process_file_sym(char *file_name, char *lib_path,
                     producer_thread_ctx_t *original_producer_ctx,
                     pthread_t *tid) {
  int rc = EXIT_SUCCESS;
  pthread_t producer_thread_id;
  producer_thread_ctx_t *producer_ctx = NULL;
  thread_user_data_t *thread_user_data = NULL;

  if (!file_name || !lib_path || !original_producer_ctx) {
    return EINVAL;
  }
  producer_ctx = (producer_thread_ctx_t *)malloc(sizeof(producer_thread_ctx_t));
  if (!producer_ctx) {
    rc = ENOMEM;
  }
  producer_ctx->buffer = original_producer_ctx->buffer;
  if (rc == EXIT_SUCCESS) {
    thread_user_data = malloc(sizeof(thread_user_data_t));
    if (!thread_user_data) {
      rc = ENOMEM;
    }
  }
  if (rc == EXIT_SUCCESS) {
    thread_user_data->file_name = calloc(strlen(file_name) + 1, 1);
    if (!thread_user_data->file_name) {
      rc = ENOMEM;
    }
  }
  if (rc == EXIT_SUCCESS) {
    thread_user_data->lib_path = calloc(strlen(lib_path) + 1, 1);
    if (!thread_user_data->lib_path) {
      rc = ENOMEM;
    }
  }
  if (rc == EXIT_SUCCESS) {
    strncpy(thread_user_data->file_name, file_name, strlen(file_name));
    strncpy(thread_user_data->lib_path, lib_path, strlen(lib_path));
    producer_ctx->user_data = (void *)thread_user_data;
    pthread_create(&producer_thread_id, NULL, get_undefined_sym, producer_ctx);
    pthread_mutex_lock(&producer_ctx->buffer->write_mutex);
    producer_ctx->buffer->producer_count++;
    pthread_mutex_unlock(&producer_ctx->buffer->write_mutex);
    *tid = producer_thread_id;
  }

  return rc;
}

typedef struct producer_thread_info_ {
  pthread_t tid;
  void *rc;
} producer_thread_info_t;

// resolve_undefined_sym receives three parameters from the context,
// buffer to receive undefined symbols, executable file name and already
// populated cache of symbols.
void *resolve_undefined_sym(void *arg) {
  int rc = EXIT_SUCCESS;
  producer_thread_ctx_t *ctx = (producer_thread_ctx_t *)arg;
  thread_user_data_t *data = (thread_user_data_t *)ctx->user_data;
  sym_cache_t *cache = NULL;
  producer_thread_info_t *producer_thread_arr = NULL;
  int number_of_producers = 0;
  int producer_arr_size = 1;
  avl_tree_type *dependency = NULL;

  if (!data) {
    rc = EINVAL;
    THREAD_RETURN(rc);
  }
  if (!data->file_name) {
    rc = EINVAL;
    THREAD_RETURN(rc);
  }
  producer_thread_arr =
      calloc(producer_arr_size, sizeof(producer_thread_info_t));
  if (!producer_thread_arr) {
    rc = ENOMEM;
    THREAD_RETURN(rc);
  }
  cache = data->cache;

  rc = init_tree(&dependency, sym_name_compare, AVL_OPTION_DEFAULT);
  if (rc != EXIT_SUCCESS) {
    goto cleanup;
  }
  // Starting thread processing executable file undefined symbols
  rc = process_file_sym(data->file_name, data->lib_path, ctx,
                        &producer_thread_arr[0].tid);
  if (rc != EXIT_SUCCESS) {
    THREAD_RETURN(rc);
  }
  number_of_producers++;

  while (true && rc == EXIT_SUCCESS) {
    pthread_mutex_lock(&ctx->buffer->write_mutex);
    while (buffer_is_empty(ctx->buffer) && !ctx->buffer->producer_done) {
      // printf("><SB> %s() waiting on a signal\n", __func__);
      pthread_cond_wait(&ctx->buffer->not_empty, &ctx->buffer->write_mutex);
    }

    if (!buffer_is_empty(ctx->buffer)) {
      // printf("><SB> %s() signalling a library to process...\n", __func__);
      char *sym = NULL;
      sym = malloc(strlen(ctx->buffer->buffer[ctx->buffer->head]) + 1);
      if (!sym) {
        rc = ENOMEM;
        THREAD_RETURN(rc);
      }
      memcpy(sym, ctx->buffer->buffer[ctx->buffer->head],
             strlen(ctx->buffer->buffer[ctx->buffer->head]) + 1);
      free(ctx->buffer->buffer[ctx->buffer->head]);
      ctx->buffer->buffer[ctx->buffer->head] = NULL;
      if (ctx->buffer->head + 1 < MAX_LIB_ENTRY) {
        ctx->buffer->head++;
      } else {
        ctx->buffer->head = 0;
      }
      pthread_cond_broadcast(&ctx->buffer->not_full);
      pthread_mutex_unlock(&ctx->buffer->write_mutex);

      //      printf("><SB> %s() Undefind sym: %s\n", __func__, sym);

      // Got Undefined symbol, need to check the cache if it has already been
      // found, if it has, record the library name and add this library's
      // undefined symbols to the Undefined Symbol Buffer, for further
      // resolution.
      char *dep_lib_name = NULL;
      if ((dep_lib_name = check_sym_cache(cache, sym)) != NULL) {
        printf("><SB> %s(): Symbol: %s is resolved in %s\n", __func__, sym,
               dep_lib_name);
        // Symbol is found in the cache
        // printf("><SB> %s() >>>>>>> Found dependency lib: %s\n", __func__,
        //        dep_lib_name);
        // Check if this library has already been added to the dependency tree
        sym_node_t search_lib = {
            .sym_name = dep_lib_name,
        };
        sym_node_t *dependency_lib =
            (sym_node_t *)avl2_search(dependency, &search_lib.node);
        if (!dependency_lib) {
          dependency_lib = malloc(sizeof(sym_node_t));
          if (!dependency_lib) {
            rc = ENOMEM;
          }
          if (rc == EXIT_SUCCESS) {
            dependency_lib->sym_name = dep_lib_name;
            if (avl2_insert(dependency, &dependency_lib->node) == NULL) {
              rc = EBADMSG;
            }
            if (rc == EXIT_SUCCESS) {
              // Adding library's Undefined symbols for further processing
              if (number_of_producers + 1 >= producer_arr_size) {
                // Need to increase size of producer_thread_arr
                producer_thread_info_t *t = realloc(
                    producer_thread_arr, sizeof(producer_thread_info_t) *
                                             (producer_arr_size + 1) * 2);
                if (!t) {
                  rc = ENOMEM;
                } else {
                  producer_arr_size = (producer_arr_size + 1) * 2;
                  producer_thread_arr = t;
                }
              }
              if (rc == EXIT_SUCCESS) {
                rc = process_file_sym(
                    dep_lib_name, dep_lib_name, ctx,
                    &producer_thread_arr[number_of_producers].tid);
                if (rc == EXIT_SUCCESS) {
                  number_of_producers++;
                  // printf("><SB> %s() successfully added producer library %s "
                  //        "Undefined symbols\n ",
                  //        __func__, dep_lib_name);
                }
              }
            }
          }
        }
      } else {
        printf("><SB> %s(): Symbol %s is not resolved.\n", __func__, sym);
      }
      // For now just release it
      free(sym);
      sym = NULL;
    } else if (is_buffer_producer_done(ctx->buffer)) {
      printf("><SB> %s() received producer is done signal, exiting...\n",
             __func__);
      pthread_mutex_unlock(&ctx->buffer->write_mutex);
      break;
    } else {
      // Impossible situation
      assert(0);
    }
  }
  int thread_rc = EXIT_SUCCESS;
  for (int i = 0; i < number_of_producers; i++) {
    pthread_join(producer_thread_arr[i].tid, &producer_thread_arr[i].rc);
    thread_rc = (int)(long)producer_thread_arr[i].rc;
    if (thread_rc != EXIT_SUCCESS) {
      fprintf(stderr,
              "><SB> %s(): producer thread %lu finished with error: %s\n",
              __func__, producer_thread_arr[i].tid, strerror(thread_rc));
    }
  }
cleanup:
  if (dependency) {
    avl2_destroy(dependency, NULL);
    free(dependency);
    dependency = NULL;
  }
  free(producer_thread_arr);
  producer_thread_arr = NULL;

  THREAD_RETURN(rc);
}