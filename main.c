#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <memory.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "elflib.h"
#include "libscout.h"

#define REGEX_PATTERN "^lib[a-zA-Z0-9_-]+\\.so(\\.[0-9]+)*$"

int main_thread(char *fn, char *lib_path) {
  int rc = EXIT_SUCCESS;
  sym_cache_t *cache = NULL;
  thread_user_data_t *thread_user_data = NULL;
  producer_thread_ctx_t *sym_producer_ctx = NULL;
  pthread_t consumer_thread_id;
  void *consumer_thread_rc;
  regex_t lib_regex;

  // Initialization of the sym cache
  cache = malloc(sizeof(sym_cache_t));
  if (!cache) {
    return ENOMEM;
  }
  rc = init_tree(&cache->cache, lib_name_compare, AVL_OPTION_DEFAULT);
  if (rc == EXIT_SUCCESS) {
    // Compiling regex for library file names
    rc = regcomp(&lib_regex, REGEX_PATTERN, REG_EXTENDED | REG_NOSUB);
    if (rc != 0) {
      char err_buf[256];
      regerror(rc, &lib_regex, err_buf, sizeof(err_buf));
      printf("><SB> %s(): regcomp() failed: %s\n", __func__, err_buf);
    }
  }
  if (rc == EXIT_SUCCESS) {
    // Starting searching for libs and populating sym cache
    rc = search_for_lib(lib_path, cache, &lib_regex);
    regfree(&lib_regex);
  }
  if (rc == EXIT_SUCCESS) {
    // Libraries' sym cache is ready, can start Undefined Symbols resolving
    // thread.
    thread_user_data = calloc(sizeof(thread_user_data_t), 1);
    if (!thread_user_data) {
      rc = ENOMEM;
    }
  }
  if (rc == EXIT_SUCCESS) {
    thread_user_data->file_name = calloc(strlen(fn) + 1, 1);
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
    strncpy(thread_user_data->file_name, fn, strlen(fn));
    strncpy(thread_user_data->lib_path, lib_path, strlen(lib_path));
    thread_user_data->cache = cache;
    rc = create_producer_thread_ctx(&sym_producer_ctx);
  }
  if (rc == EXIT_SUCCESS) {
    sym_producer_ctx->user_data = (void *)thread_user_data;

    pthread_create(&consumer_thread_id, NULL, resolve_undefined_sym,
                   sym_producer_ctx);

    pthread_join(consumer_thread_id, &consumer_thread_rc);
    rc = (int)(long)consumer_thread_rc;
    printf("><SB> _%s() resolve_undefined_sym finished with rc: %s\n", __func__,
           strerror(rc));
  }
  if (thread_user_data) {
    if (thread_user_data->lib_path) {
      free(thread_user_data->lib_path);
      thread_user_data->lib_path = NULL;
    }
    if (thread_user_data->file_name) {
      free(thread_user_data->file_name);
      thread_user_data->file_name = NULL;
    }
  }

  destroy_producer_thread_ctx(sym_producer_ctx);

  if (cache) {
    if (cache->cache) {
      avl2_destroy(cache->cache, destroy_cache_node);
      free(cache->cache);
      cache->cache = NULL;
    }
    free(cache);
    cache = NULL;
  }

  return rc;
}

int main(int argc, char *argv[]) {
  int rc = EXIT_SUCCESS;
  int opt = 0;
  char exec_file[PATH_MAX] = {0};
  char lib_path[PATH_MAX] = {0};

  while ((opt = getopt(argc, argv, "l:e:h")) != -1) {
    switch (opt) {
    case 'e':
      memcpy(exec_file, optarg, strlen(optarg));
      break;
    case 'l':
      memcpy(lib_path, optarg, strlen(optarg));
      break;
    case 'h':
      // TODO
      break;
    case '?':
      fprintf(stderr, "Unrecognized option \"-%c\"\n", opt);
      return EINVAL;
    default:
      fprintf(stderr, "An unexpcted error ossured.\n");
      return EINVAL;
    }
  }
  if (exec_file[0] == 0x0) {
    fprintf(stderr,
            "\"-e full_path_to_executable_file\" is required parameter\n");
    return EINVAL;
  }
  if (lib_path[0] == 0x0) {
    fprintf(stderr, "\"-l libraries path\" or set LIB_PATH variable is "
                    "required parameter\n");
    return EINVAL;
  }
  fprintf(stdout, "><SB> executable file: %s\n", exec_file);
  fprintf(stdout, "><SB> lib path: %s\n", lib_path);

  rc = main_thread(exec_file, lib_path);
  printf("><SB> %s(): main thread finished with error code: %s\n", __func__,
         strerror(rc));

  return rc;
}