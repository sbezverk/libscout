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
#include "libscout.h"

static bool buffer_is_empty(mpsc_buffer_t *b) { return b->head == b->tail; }

static bool buffer_is_full(mpsc_buffer_t *b) {
  return ((b->tail + 1) % MAX_LIB_ENTRY) == b->head;
}

static bool is_buffer_producer_done(mpsc_buffer_t *buffer) {
  return atomic_load(&buffer->producer_done);
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

static int init_tree(avl_tree_type **tree, avl2_compare_type avl_compare,
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

// int create_lib_node(char *lib_name, lib_node_t **lib_node) {
//   int rc = EXIT_SUCCESS;
//   lib_node_t *node = NULL;

//   if (!lib_node || !lib_name) {
//     fprintf(stderr, "parameters check failure\n");
//     rc = EINVAL;
//   }
//   if (rc == EXIT_SUCCESS) {
//     node = calloc(1, sizeof(lib_node_t));
//     if (!node) {
//       fprintf(stderr, "lib_node_t mem allocation failure\n");
//       rc = ENOMEM;
//     }
//   }
//   if (rc == EXIT_SUCCESS) {
//     rc = init_tree(&node->undefined, sym_name_compare, AVL_OPTION_DEFAULT);
//     if (rc != EXIT_SUCCESS) {
//       fprintf(stderr, "lib_node_t undefined tree init failure, error: %s\n",
//               strerror(rc));
//     }
//   }
//   if (rc == EXIT_SUCCESS) {
//     rc = init_tree(&node->defined, sym_name_compare, AVL_OPTION_DEFAULT);
//     if (rc != EXIT_SUCCESS) {
//       fprintf(stderr, "lib_node_t dependecy tree init failure, error: %s\n",
//               strerror(rc));
//     }
//   }
//   strncpy(node->lib_name, lib_name, PATH_MAX);
//   if (rc != EXIT_SUCCESS) {
//     // Cleanup resources in case of the function failure
//     destroy_lib_node(node);
//   } else {
//     *lib_node = node;
//   }

//   return rc;
// }

void destroy_sym_node(avl2_node_type *node, avl_tree_type *lib_node) {
  if (((sym_node_t *)node)->sym_name) {
    free(((sym_node_t *)node)->sym_name);
    ((sym_node_t *)node)->sym_name = NULL;
  }
  free(((sym_node_t *)node));
  node = NULL;
}
void destroy_cache_node(avl2_node_type *node, avl_tree_type *lib_node) {
  if (((lib_node_t *)node)->defined) {
    avl2_destroy(((lib_node_t *)node)->defined, destroy_sym_node);
    free(((lib_node_t *)node)->defined);
    ((lib_node_t *)node)->defined = NULL;
  }
  if (((lib_node_t *)node)->undefined) {
    avl2_destroy(((lib_node_t *)node)->undefined, destroy_sym_node);
    free(((lib_node_t *)node)->undefined);
    ((lib_node_t *)node)->undefined = NULL;
  }
  free(((lib_node_t *)node));
  node = NULL;
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
  printf("><SB> %s() producer thread ctx at: %p\n", __func__, (void *)ctx);
  rc = process_elf_file(data->fd, &elf_file_descr);
  if (rc == EXIT_SUCCESS) {
    for (int i = 0; i < elf_file_descr->elf_sym_tbl_num && rc == EXIT_SUCCESS;
         i++) {
      if (!elf_file_descr->elf_sym_tbls[i].is_dynamic) {
        // Only interested in .dynsym table.
        continue;
      }
      for (int y = 0;
           y < elf_file_descr->elf_sym_tbls[i].elf_sym_tbl_num_entry &&
           rc == EXIT_SUCCESS;
           y++) {
        if (elf_sym_get_string(
                elf_file_descr, elf_file_descr->elf_sym_tbls[i].is_dynamic,
                elf_sym_get_st_name(
                    elf_file_descr,
                    elf_file_descr->elf_sym_tbls[i].elf_sym_table[y]),
                &name) == EXIT_SUCCESS) {
          if (strlen(name) == 0) {
            // If name length is 0 ignoring entry, free it
            if (name) {
              free(name);
              name = NULL;
            }
            continue;
          }
          if (elf_sym_is_global(
                  elf_file_descr,
                  elf_file_descr->elf_sym_tbls[i].elf_sym_table[y])) {
            // Interested only in Global symbols
            if (!elf_sym_is_defined(
                    elf_file_descr,
                    elf_file_descr->elf_sym_tbls[i].elf_sym_table[y])) {
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
    }
    if (elf_file_descr) {
      free_elf_file_descr(elf_file_descr);
      elf_file_descr = NULL;
    }
  }

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

  printf("><SB> %s() has finished, exiting...\n", __func__);

  THREAD_RETURN(rc);
}

int search_for_lib(mpsc_buffer_t *buffer, char *current_path) {
  DIR *dir_stream = NULL;
  struct dirent *dir_entry = NULL;
  int rc = EXIT_SUCCESS;
  char lib_suffix[] = {".so"};

  // printf("><SB> %s() started, current directory: %s\n", __func__,
  //     ctx->buffert_path);
  if ((dir_stream = opendir(current_path)) == NULL) {
    rc = errno;
  }
  if (rc == EXIT_SUCCESS) {
    while ((dir_entry = readdir(dir_stream)) != NULL) {
      if (dir_entry->d_type == 0x04) {
        if ((strcmp(dir_entry->d_name, ".") == 0) &&
            (strlen(dir_entry->d_name) == 1)) {
          continue;
        } else if ((strcmp(dir_entry->d_name, "..") == 0) &&
                   (strlen(dir_entry->d_name) == 2)) {
          continue;
        } else {
          // printf("><SB> found directory: %s, entering...\n",
          // dir_entry->d_name);
          char *new_current_path = calloc(PATH_MAX, 1);
          if (!new_current_path) {
            rc = ENOMEM;
            return rc;
          }
          snprintf((char *)new_current_path, PATH_MAX, "%s/%s", current_path,
                   dir_entry->d_name);
          if ((rc = search_for_lib(buffer, new_current_path)) != EXIT_SUCCESS) {
            return rc;
          }
          if (new_current_path) {
            free(new_current_path);
            new_current_path = NULL;
          }
        }
      } else {
        if (strlen(dir_entry->d_name) < 4) {
          // library name cannot be shorter than 4 bytes "a.so"
          continue;
        }
        if (strcmp(dir_entry->d_name + strlen(dir_entry->d_name) -
                       strlen(lib_suffix),
                   lib_suffix) != 0) {
          continue;
        }
        pthread_mutex_lock(&buffer->write_mutex);
        while (buffer_is_full(buffer)) {
          pthread_cond_wait(&buffer->not_full, &buffer->write_mutex);
        }

        buffer->buffer[buffer->tail] = calloc(PATH_MAX, 1);

        snprintf(buffer->buffer[buffer->tail], PATH_MAX, "%s/%s", current_path,
                 dir_entry->d_name);
        if (buffer->tail + 1 < MAX_LIB_ENTRY) {
          buffer->tail++;
        } else {
          buffer->tail = 0;
        }
        pthread_cond_signal(&buffer->not_empty);
        pthread_mutex_unlock(&buffer->write_mutex);
      }
    }
  }
  // printf("Complete processing path: %s\n", current_path);
  if (dir_stream) {
    closedir(dir_stream);
    dir_stream = NULL;
  }
  // printf("><SB> %s() finished, with error code: %d\n", __func__, rc);

  return rc;
}

void *search_for_lib_thread(void *arg) {
  int rc = EXIT_SUCCESS;
  producer_thread_ctx_t *ctx = (producer_thread_ctx_t *)arg;
  thread_user_data_t *data = (thread_user_data_t *)ctx->user_data;
  // printf("><SB> %s() thread started, current directory: %s\n", __func__,
  //     ctx->buffert_path);

  printf("><SB> %s() current path: %s\n", __func__, data->lib_path);
  rc = search_for_lib(ctx->buffer, data->lib_path);

  // Informing that the search for libs has been completed
  printf("><SB> %s() has been completed\n", __func__);
  pthread_mutex_lock(&ctx->buffer->write_mutex);
  atomic_store_explicit(&ctx->buffer->producer_done, true,
                        memory_order_release);
  pthread_cond_broadcast(&ctx->buffer->not_empty);
  pthread_mutex_unlock(&ctx->buffer->write_mutex);
  printf("><SB> %s() has signaled completion, with error code: %d\n", __func__,
         rc);
  if (rc == EXIT_SUCCESS) {
    rc = EOF;
  }

  return (void *)(long)rc;
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
      free(ctx->buffer);
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

int fetch_library_name(mpsc_buffer_t *buffer, pthread_t lib_search_thread_id,
                       char **lib_name) {
  int rc = EXIT_SUCCESS;
  void *search_t_rc = EXIT_SUCCESS;

  if (is_buffer_producer_done(buffer)) {
    return EOF;
  }
  pthread_mutex_lock(&buffer->write_mutex);
  while (buffer_is_empty(buffer) && !buffer->producer_done) {
    pthread_cond_wait(&buffer->not_empty, &buffer->write_mutex);
  }

  if (!buffer_is_empty(buffer)) {
    // printf("><SB> signalling a library to process...");
    char *n = NULL;
    n = malloc(strlen(buffer->buffer[buffer->head]) + 1);
    if (!n) {
      return ENOMEM;
    }
    memcpy(n, buffer->buffer[buffer->head],
           strlen(buffer->buffer[buffer->head]) + 1);
    free(buffer->buffer[buffer->head]);
    if (buffer->head + 1 < MAX_LIB_ENTRY) {
      buffer->head++;
    } else {
      buffer->head = 0;
    }
    pthread_cond_broadcast(&buffer->not_full);
    pthread_mutex_unlock(&buffer->write_mutex);
    *lib_name = n;
  } else if (buffer->producer_done) {
    pthread_mutex_unlock(&buffer->write_mutex);
    pthread_join(lib_search_thread_id, &search_t_rc);
    rc = (int)(long)search_t_rc;
    if (rc != EXIT_SUCCESS && rc != EOF) {
      printf("><SB> lib search thread failed with error code: %s\n",
             strerror(rc));
    }
  } else {
    // Impossible situation
    assert(0);
  }

  return rc;
}

static int store_sym(lib_node_t *lib_node, char *name) {
  int rc = EXIT_SUCCESS;
  sym_node_t *sym = NULL;
  sym_node_t *debug_node = NULL;
  sym = calloc(1, sizeof(sym_node_t));
  if (!sym) {
    rc = ENOMEM;
  }
  if (rc == EXIT_SUCCESS) {
    sym->sym_name = calloc(strlen(name) + 1, 1);
    memcpy(sym->sym_name, name, strlen(name));
    // Check if the sym is already in the tree
    debug_node = (sym_node_t *)avl2_search(lib_node->defined, &sym->node);
    if (debug_node == NULL) {
      debug_node = (sym_node_t *)avl2_insert(lib_node->defined, &sym->node);
      if (debug_node == NULL) {
        // Cleanup failed entry
        if (sym) {
          free(sym);
          sym = NULL;
        }
        rc = EFAULT;
      }
    } else {
      // CLeanup duplicate entry
      if (sym) {
        free(sym);
        sym = NULL;
      }
    }
  }

  return rc;
}

static int store_sym_in_tree(avl_tree_type *tree, char *lib_name, char *name) {
  int rc = EXIT_SUCCESS;

  lib_node_t *lib = NULL;
  lib_node_t *debug_node = NULL;
  lib = calloc(1, sizeof(lib_node_t));
  if (!lib) {
    rc = ENOMEM;
  }
  if (rc == EXIT_SUCCESS) {
    memcpy(&lib->lib_name[0], lib_name, strlen(lib_name));
    // Check if the lib is already in the tree
    debug_node = (lib_node_t *)avl2_search(tree, &lib->node);
    if (debug_node == NULL) {
      // New lib entry, need to initialize it
      rc = init_tree(&lib->defined, sym_name_compare, AVL_OPTION_DEFAULT);
      if (rc == EXIT_SUCCESS) {
        debug_node = (lib_node_t *)avl2_insert(tree, &lib->node);
        if (debug_node == NULL) {
          // Cleanup failed entry
          if (lib) {
            free(lib);
            lib = NULL;
          }
          rc = EFAULT;
        } else {
          // New lib has been inserted, storing sym
          rc = store_sym(lib, name);
          // printf(
          //     "><SB> %s() symbol \"%s\" stored in a new lib name: %s tree
          //     with " "rc: %s\n",
          //     __func__, name, lib_name, strerror(rc));
        }
      }
    } else {
      // Lib is already in the tree, then just storing sym
      rc = store_sym(debug_node, name);
      // printf("><SB> %s() symbol \"%s\" stored in an existing lib name: %s
      // tree "
      //        "with "
      //        "rc: %s\n",
      //        __func__, name, lib_name, strerror(rc));
      // Free lib object since the one found in the tree was used.
      if (lib) {
        free(lib);
        lib = NULL;
      }
    }
  }

  return rc;
}

int get_sym(int fd, sym_cache_t *cache, char *lib_name) {
  int rc = EXIT_SUCCESS;

  elf_file_descr_t *elf_file_descr = NULL;
  char *name = NULL;
  rc = process_elf_file(fd, &elf_file_descr);
  if (rc == EXIT_SUCCESS) {
    for (int i = 0; i < elf_file_descr->elf_sym_tbl_num && rc == EXIT_SUCCESS;
         i++) {
      if (!elf_file_descr->elf_sym_tbls[i].is_dynamic) {
        // Only interested in .dynsym table.
        continue;
      }
      for (int y = 0;
           y < elf_file_descr->elf_sym_tbls[i].elf_sym_tbl_num_entry &&
           rc == EXIT_SUCCESS;
           y++) {
        if (ELF_ST_TYPE(elf_sym_get_st_info(
                elf_file_descr,
                elf_file_descr->elf_sym_tbls[i].elf_sym_table[y])) !=
            STT_OBJECT) {
          continue;
        }
        if (elf_sym_get_string(
                elf_file_descr, elf_file_descr->elf_sym_tbls[i].is_dynamic,
                elf_sym_get_st_name(
                    elf_file_descr,
                    elf_file_descr->elf_sym_tbls[i].elf_sym_table[y]),
                &name) == EXIT_SUCCESS) {
          if (strlen(name) == 0) {
            // If name length is 0 ignoring entry, free it
            if (name) {
              free(name);
              name = NULL;
            }
            continue;
          }
          if (elf_sym_is_global(
                  elf_file_descr,
                  elf_file_descr->elf_sym_tbls[i].elf_sym_table[y]) &&
              elf_sym_is_defined(
                  elf_file_descr,
                  elf_file_descr->elf_sym_tbls[i].elf_sym_table[y])) {
            // Interested only in Global and Defined symbols
            store_sym_in_tree(cache->cache, lib_name, name);
          }
          // Freeing name
          if (name) {
            free(name);
            name = NULL;
          }
        }
      }
    }
    if (elf_file_descr) {
      free_elf_file_descr(elf_file_descr);
      elf_file_descr = NULL;
    }
  }

  return rc;
}

static int populate_cache(sym_cache_t *cache, char *lib_name) {
  int rc = EXIT_SUCCESS;
  int fd = -1;

  if (!cache || !lib_name) {
    return EINVAL;
  }
  fd = open(lib_name, O_RDONLY);
  if (fd == -1) {
    rc = errno;
  }
  if (rc == EXIT_SUCCESS) {
    rc = get_sym(fd, cache, lib_name);
  }
  if (fd > 0) {
    close(fd);
  }

  return rc;
}

char *check_sym_cache(sym_cache_t *cache, char *sym) {
  lib_node_t *lib_node = NULL;
  lib_node = (lib_node_t *)avl2_get_first(cache->cache);
  while (lib_node) {
    //    printf("><SB> %s() found lib %s in the tree.\n", __func__,
    //           lib_node->lib_name);
    lib_node = (lib_node_t *)avl2_get_next(cache->cache, &lib_node->node);
    sym_node_t sym_node = {
        .sym_name = sym,
    };
    if (lib_node) {
      // Make sure that lib's sym tree is initialized
      if (lib_node->defined) {
        if (avl2_search(lib_node->defined, &sym_node.node)) {
          // Symbol found in the library, returning its name
          return lib_node->lib_name;
        }
      }
    }
  }
  // Symbol is not found in any library, returning NULL
  return NULL;
}

void *resolve_undefined_sym(void *arg) {
  int rc = EXIT_SUCCESS;
  producer_thread_ctx_t *ctx = (producer_thread_ctx_t *)arg;
  thread_user_data_t *data = (thread_user_data_t *)ctx->user_data;
  sym_cache_t *cache = NULL;

  // Initialization of the sym cache
  cache = malloc(sizeof(sym_cache_t));
  if (!cache) {
    rc = ENOMEM;
    THREAD_RETURN(rc);
  }
  rc = init_tree(&cache->cache, lib_name_compare, AVL_OPTION_DEFAULT);
  if (rc != EXIT_SUCCESS) {
    THREAD_RETURN(rc);
  }

  // Starting lib search thread to feed this function with found
  // libraries
  thread_user_data_t *lib_search_thread_user_data =
      malloc(sizeof(thread_user_data_t));
  if (!lib_search_thread_user_data) {
    rc = ENOMEM;
    THREAD_RETURN(rc);
  }
  lib_search_thread_user_data->lib_path = calloc(strlen(data->lib_path) + 1, 1);
  if (!lib_search_thread_user_data->lib_path) {
    rc = ENOMEM;
    THREAD_RETURN(rc);
  }
  producer_thread_ctx_t *lib_search_producer_ctx = NULL;
  strncpy(lib_search_thread_user_data->lib_path, data->lib_path,
          strlen(data->lib_path));
  rc = create_producer_thread_ctx(&lib_search_producer_ctx);
  if (rc != EXIT_SUCCESS) {
    THREAD_RETURN(rc);
  }
  lib_search_producer_ctx->user_data = (void *)lib_search_thread_user_data;

  pthread_t lib_search_thread_id;
  void *lib_search_thread_rc;
  pthread_create(&lib_search_thread_id, NULL, search_for_lib_thread,
                 lib_search_producer_ctx);

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
      if (ctx->buffer->head + 1 < MAX_LIB_ENTRY) {
        ctx->buffer->head++;
      } else {
        ctx->buffer->head = 0;
      }
      pthread_cond_broadcast(&ctx->buffer->not_full);
      pthread_mutex_unlock(&ctx->buffer->write_mutex);

      // printf("><SB> %s() Undefind sym: %s\n", __func__, sym);

      // Got Undefined symbol, need to check the cache if it has already been
      // found, if it has, record the library name and add this library's
      // undefined symbols to the Undefined Symbol Buffer, for further
      // resolution.
      char *dep_lib_name = NULL;
      if ((dep_lib_name = check_sym_cache(cache, sym)) != NULL) {
        // Symbol is found in the cache
        printf("><SB> %s() >>>>>>> Found dependency lib: %s\n", __func__,
               dep_lib_name);
      } else {
        // Symbol is not found in the cache
        char *lib_name = NULL;
        rc = fetch_library_name(lib_search_producer_ctx->buffer,
                                lib_search_thread_id, &lib_name);
        if (rc == EXIT_SUCCESS) {
          printf("><SB> %s() found library: %s\n", __func__, lib_name);
          rc = populate_cache(cache, lib_name);
          free(lib_name);
          lib_name = NULL;
        } else if (rc == EOF) {
          rc = EXIT_SUCCESS;
        }
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
  if (lib_search_thread_user_data) {
    if (lib_search_thread_user_data->lib_path) {
      free(lib_search_thread_user_data->lib_path);
      lib_search_thread_user_data->lib_path = NULL;
    }
  }

  // TODO: (sbezverk) Add cache clean up
  if (cache) {
    if (cache->cache) {
      avl2_destroy(cache->cache, destroy_cache_node);
      free(cache->cache);
      cache->cache = NULL;
    }
    free(cache);
    cache = NULL;
  }
  pthread_join(lib_search_thread_id, &lib_search_thread_rc);
  destroy_producer_thread_ctx(lib_search_producer_ctx);

  THREAD_RETURN(rc);
}