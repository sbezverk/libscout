#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "avl/avl_tree.h"
#include "elflib/elflib.h"
#include "libscout.h"

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

int create_lib_node(char *lib_name, lib_node_t **lib_node) {
  int rc = EXIT_SUCCESS;
  lib_node_t *node = NULL;

  if (!lib_node || !lib_name) {
    fprintf(stderr, "parameters check failure\n");
    rc = EINVAL;
  }
  if (rc == EXIT_SUCCESS) {
    node = calloc(1, sizeof(lib_node_t));
    if (!node) {
      fprintf(stderr, "lib_node_t mem allocation failure\n");
      rc = ENOMEM;
    }
  }
  if (rc == EXIT_SUCCESS) {
    rc = init_tree(&node->undefined, sym_name_compare, AVL_OPTION_DEFAULT);
    if (rc != EXIT_SUCCESS) {
      fprintf(stderr, "lib_node_t undefined tree init failure, error: %s\n",
              strerror(rc));
    }
  }
  if (rc == EXIT_SUCCESS) {
    rc = init_tree(&node->defined, sym_name_compare, AVL_OPTION_DEFAULT);
    if (rc != EXIT_SUCCESS) {
      fprintf(stderr, "lib_node_t dependecy tree init failure, error: %s\n",
              strerror(rc));
    }
  }
  strncpy(node->lib_name, lib_name, PATH_MAX);
  if (rc != EXIT_SUCCESS) {
    // Cleanup resources in case of the function failure
    destroy_lib_node(node);
  } else {
    *lib_node = node;
  }

  return rc;
}

void destroy_sym_node(avl2_node_type *node, avl_tree_type *lib_node) {
  if (((sym_node_t *)node)->sym_name) {
    free(((sym_node_t *)node)->sym_name);
    ((sym_node_t *)node)->sym_name = NULL;
  }
  free(((sym_node_t *)node));
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

static int store_sym_in_tree(avl_tree_type *tree, char *name) {
  int rc = EXIT_SUCCESS;

  sym_node_t *sym = NULL;
  sym = calloc(1, sizeof(sym_node_t));
  if (!sym) {
    rc = ENOMEM;
  }
  if (rc == EXIT_SUCCESS) {
    sym->sym_name = name;
    // Check if the sym is already in the tree
    if (!avl2_search(tree, &sym->node)) {
      if (!avl2_insert(tree, &sym->node)) {
        // Cleanup failed entry
        if (sym) {
          free(sym);
          sym = NULL;
          free(name);
          name = NULL;
        }
        rc = EFAULT;
      }
    } else {
      // CLeanup duplicate entry
      if (sym) {
        free(sym);
        sym = NULL;
        free(name);
        name = NULL;
      }
    }
  }

  return rc;
}

int populate_node_symtable(int fd, lib_node_t *lib_node, bool populate_defined,
                           bool populate_undefined) {
  int rc = EXIT_SUCCESS;
  elf_file_descr_t *elf_file_descr = NULL;
  char *name = NULL;

  rc = process_elf_file(fd, &elf_file_descr);
  if (rc == EXIT_SUCCESS) {
    for (int i = 0; i < elf_file_descr->elf_sym_tbl_num && rc == EXIT_SUCCESS;
         i++) {
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
            if (populate_undefined &&
                !elf_sym_is_defined(
                    elf_file_descr,
                    elf_file_descr->elf_sym_tbls[i].elf_sym_table[y])) {
              rc = store_sym_in_tree(lib_node->undefined, name);
              continue;
            }
            if (populate_defined &&
                elf_sym_is_defined(
                    elf_file_descr,
                    elf_file_descr->elf_sym_tbls[i].elf_sym_table[y])) {
              rc = store_sym_in_tree(lib_node->defined, name);
              continue;
            }
            // It is Global sym but it is not being collected, hence free it
            if (name) {
              free(name);
              name = NULL;
            }
          } else {
            // Symbol is not Global, free it
            if (name) {
              free(name);
              name = NULL;
            }
          }
        }
      }
    }
    if (elf_file_descr) {
      free_elf_file_descr(elf_file_descr);
      elf_file_descr = NULL;
    }
    if (rc != EXIT_SUCCESS) {
    }
  }

  return rc;
}

static bool buffer_is_empty(mpsc_buffer_t *b) { return b->head == b->tail; }

static bool buffer_is_full(mpsc_buffer_t *b) { return b->tail + 1 == b->head; }

int search_for_lib(search_thread_ctx_t *ctx) {
  DIR *dir_stream = NULL;
  struct dirent *dir_entry = NULL;
  int rc = EXIT_SUCCESS;
  char lib_suffix[] = {".so"};

  // printf("><SB> %s() started, current directory: %s\n", __func__,
  //     ctx->datat_path);
  if ((dir_stream = opendir(ctx->current_path)) == NULL) {
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
          search_thread_ctx_t np = {
              .data = ctx->data,
          };
          snprintf(np.current_path, PATH_MAX, "%s/%s", ctx->current_path,
                   dir_entry->d_name);
          if ((rc = search_for_lib(&np)) != EXIT_SUCCESS) {
            return rc;
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
        // printf("><SB> %s() library: %s\n", __func__, dir_entry->d_name);
        pthread_mutex_lock(&ctx->data->write_mutex);
        while (buffer_is_full(ctx->data)) {
          pthread_cond_wait(&ctx->data->not_full, &ctx->data->write_mutex);
        }
        snprintf(&ctx->data->buffer[ctx->data->tail][0], PATH_MAX, "%s/%s",
                 ctx->current_path, dir_entry->d_name);
        if (ctx->data->tail + 1 < MAX_LIB_ENTRY) {
          ctx->data->tail++;
        } else {
          ctx->data->tail = 0;
        }
        pthread_cond_signal(&ctx->data->not_empty);
        pthread_mutex_unlock(&ctx->data->write_mutex);

        // printf("><SB> found library: %s \n", dir_entry->d_name);
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
  search_thread_ctx_t *ctx = (search_thread_ctx_t *)arg;

  // printf("><SB> %s() thread started, current directory: %s\n", __func__,
  //     ctx->datat_path);
  rc = search_for_lib(ctx);

  // Informing that the search for libs has been completed
  pthread_mutex_lock(&ctx->data->write_mutex);
  ctx->data->producer_done = true;
  pthread_cond_broadcast(&ctx->data->not_empty);
  pthread_mutex_unlock(&ctx->data->write_mutex);
  printf("><SB> %s() thread finished, with error code: %d\n", __func__, rc);
  if (rc == EXIT_SUCCESS) {
    rc = EOF;
  }

  return (void *)(long)rc;
}

int create_search_for_lib_ctx(char *lib_path, search_thread_ctx_t **ctx) {
  int rc = EXIT_SUCCESS;
  mpsc_buffer_t *buffer = NULL;
  search_thread_ctx_t *ctx_n = NULL;

  ctx_n = calloc(1, sizeof(search_thread_ctx_t));
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
    buffer->producer_done = false;
    memcpy(&ctx_n->current_path[0], lib_path, PATH_MAX);
    ctx_n->search_t = 0;
    ctx_n->data = buffer;
    *ctx = ctx_n;
  } else {
    if (ctx_n) {
      ctx_n->data = NULL;
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

void destroy_serach_for_lib_ctx(search_thread_ctx_t *ctx) {
  if (ctx) {
    if (ctx->data) {
      pthread_cond_destroy(&ctx->data->not_empty);
      pthread_cond_destroy(&ctx->data->not_full);
      pthread_mutex_destroy(&ctx->data->write_mutex);
      free(ctx->data);
    }
    ctx->data = NULL;
    free(ctx);
    ctx = NULL;
  }
}

int fetch_library_name(search_thread_ctx_t *ctx, char **lib_name) {
  int rc = EXIT_SUCCESS;
  void *search_t_rc = EXIT_SUCCESS;

  pthread_mutex_lock(&ctx->data->write_mutex);
  while (buffer_is_empty(ctx->data) && !ctx->data->producer_done) {
    pthread_cond_wait(&ctx->data->not_empty, &ctx->data->write_mutex);
  }

  if (!buffer_is_empty(ctx->data)) {
    // printf("><SB> signalling a library to process...");
    char *n = NULL;
    n = malloc(strlen(ctx->data->buffer[ctx->data->head]) + 1);
    if (!n) {
      return ENOMEM;
    }
    memcpy(n, &ctx->data->buffer[ctx->data->head][0],
           strlen(ctx->data->buffer[ctx->data->head]) + 1);
    if (ctx->data->head + 1 < MAX_LIB_ENTRY) {
      ctx->data->head++;
    } else {
      ctx->data->head = 0;
    }
    pthread_cond_broadcast(&ctx->data->not_full);
    pthread_mutex_unlock(&ctx->data->write_mutex);
    *lib_name = n;
  } else if (ctx->data->producer_done) {
    pthread_mutex_unlock(&ctx->data->write_mutex);
    pthread_join(ctx->search_t, &search_t_rc);
    rc = (int)(long)search_t_rc;
    if (rc != EXIT_SUCCESS && rc != EOF) {
      printf("><SB> Search thread failed with error code: %s\n", strerror(rc));
    }
  } else {
    // Impossible situation
    assert(0);
  }

  return rc;
}

int find_dependency(lib_node_t *lib_node, dependency_tree_t **deps,
                    char *lib_path) {
  int rc = EXIT_SUCCESS;
  sym_cache_t *cache = NULL;
  search_thread_ctx_t *ctx = NULL;
  char *lib_name = NULL;

  //  printf("><SB> %s() started, current directory: %s\n", __func__, lib_path);

  rc = create_search_for_lib_ctx(lib_path, &ctx);
  if (rc != EXIT_SUCCESS) {
    return rc;
  }

  // Starting thread to look for libraries
  pthread_create(&ctx->search_t, NULL, search_for_lib_thread, (void *)ctx);

  while ((rc = fetch_library_name(ctx, &lib_name)) == EXIT_SUCCESS) {
    printf("><SB> processing library: %s\n", lib_name);

    // For now freeing memory
    free(lib_name);
    lib_name = NULL;
  }

  if (rc == EOF) {
    // Fetching files completed normally with expected EOF
    rc = EXIT_SUCCESS;
  }
  // printf("><SB> %s() finished with error code: %d\n", __func__, rc);
  destroy_serach_for_lib_ctx(ctx);

  return rc;
}
