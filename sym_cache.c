#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "avl/avl_tree.h"
#include "elflib.h"
#include "iter.h"
#include "libscout.h"

void destroy_sym_node(avl2_node_type *node, avl_tree_type *lib_node) {
  if (((sym_node_t *)node)->sym_name) {
#ifdef DEBUG
    printf("><SB> %s(): removing sym %s\n", __func__,
           ((sym_node_t *)node)->sym_name);
#endif
    free(((sym_node_t *)node)->sym_name);
    ((sym_node_t *)node)->sym_name = NULL;
  }
  free(((sym_node_t *)node));
  node = NULL;
}

void destroy_cache_node(avl2_node_type *node, avl_tree_type *lib_node) {
  if (((lib_node_t *)node)->defined) {
#ifdef DEBUG
    printf("><SB> %s(): removing cached library %s defined symbols\n", __func__,
           ((lib_node_t *)node)->lib_name);
#endif
    avl2_destroy(((lib_node_t *)node)->defined, destroy_sym_node);
    free(((lib_node_t *)node)->defined);
    ((lib_node_t *)node)->defined = NULL;
  }
  if (((lib_node_t *)node)->undefined) {
#ifdef DEBUG
    printf("><SB> %s(): removing cached library %s undefined symbols\n",
           __func__, ((lib_node_t *)node)->lib_name);
#endif
    avl2_destroy(((lib_node_t *)node)->undefined, destroy_sym_node);
    free(((lib_node_t *)node)->undefined);
    ((lib_node_t *)node)->undefined = NULL;
  }
  free(((lib_node_t *)node));
  node = NULL;
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
        if (sym->sym_name) {
          free(sym->sym_name);
          sym->sym_name = NULL;
        }
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
#ifdef DEBUG
          printf(
              "><SB> %s() symbol \"%s\" stored in a new lib name: %s treewith "
              "rc: %s\n ",
              __func__, name, lib_name, strerror(rc));
#endif
        }
      }
    } else {
      // Lib is already in the tree, then just storing sym
      rc = store_sym(debug_node, name);
#ifdef DEBUG
      printf("><SB> %s() symbol \"%s\" stored in an existing lib name: %s tree "
             " with rc: %s\n ",
             __func__, name, lib_name, strerror(rc));
#endif

      // Free lib object since the one found in the tree was used.
      if (lib) {
        free(lib);
        lib = NULL;
      }
    }
  }

  return rc;
}

int get_sym(char *fn, sym_cache_t *cache) {
  int rc = EXIT_SUCCESS;

  elf_file_descr_t *elf_file_descr = NULL;
  char *name = NULL;
  rc = process_elf_file(fn, &elf_file_descr);
  if (rc == EXIT_SUCCESS) {
#ifdef DEBUG
    printf("><SB> %s: Caching symbols found in library: %s\n", __func__, fn);
#endif
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
            if (!elf_sym_is_local(elf_file_descr, entry) &&
                elf_sym_is_defined(elf_file_descr, entry)) {
              // Interested only in Global and Defined symbols
              store_sym_in_tree(cache->cache, fn, name);
            }
            // Freeing name
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

  return rc;
}

int populate_cache(sym_cache_t *cache, char *file_name) {
  int rc = EXIT_SUCCESS;

  if (!cache || !file_name) {
    if (!cache) {
      fprintf(stderr, "><SB> %s(): cache is NULL\n", __func__);
    }
    if (!file_name) {
      fprintf(stderr, "><SB> %s(): file name is NULL\n", __func__);
    }
    return EINVAL;
  }
#ifdef DEBUG
  printf("><SB> %s(): caching symbols for %s\n", __func__, file_name);
#endif
  rc = get_sym(file_name, cache);

  return rc;
}

char *check_sym_cache(sym_cache_t *cache, char *sym) {
  lib_node_t *lib_node = NULL;
  if (!cache || !cache->cache) {
    printf("><SB> %s(): Cache is not initialized.\n", __func__);
    return NULL;
  }
  lib_node = (lib_node_t *)avl2_get_first(cache->cache);
  while (lib_node) {
#ifdef DEBUG
    printf("><SB> %s() found lib %s in the tree.\n", __func__,
           lib_node->lib_name);
#endif
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
    lib_node = (lib_node_t *)avl2_get_next(cache->cache, &lib_node->node);
  }
  // Symbol is not found in any library, returning NULL
  return NULL;
}
