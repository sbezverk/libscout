#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "elflib.h"
#include "libscout.h"

void *main_thread(void *arg) {
  int rc = EXIT_SUCCESS;
  main_thread_ctx_t *ctx = (main_thread_ctx_t *)arg;
  int exec_fd = -1;

  sym_thread_data_t *sym_thread_data = malloc(sizeof(sym_thread_data_t));
  if (!sym_thread_data) {
    rc = ENOMEM;
    THREAD_RETURN(rc);
  }
  sym_thread_data->lib_path = calloc(strlen(ctx->lib_path) + 1, 1);
  if (!sym_thread_data->lib_path) {
    rc = ENOMEM;
    THREAD_RETURN(rc);
  }
  exec_fd = open(ctx->exe_file_name, O_RDONLY);
  if (exec_fd < 0) {
    rc = errno;
    fprintf(stderr, "failed tp open executable file %s with error: %s\n",
            ctx->exe_file_name, strerror(rc));
    THREAD_RETURN(rc);
  }

  producer_thread_ctx_t *und_sym_producer_ctx = NULL;
  sym_thread_data->fd = exec_fd;
  strncpy(sym_thread_data->lib_path, ctx->lib_path, strlen(ctx->lib_path));

  rc = create_producer_thread_ctx(&und_sym_producer_ctx);
  if (rc != EXIT_SUCCESS) {
    THREAD_RETURN(rc);
  }
  und_sym_producer_ctx->user_data = (void *)sym_thread_data;
  printf("><SB> user data addr: %p user data points to: %p\n",
         &und_sym_producer_ctx->user_data,
         (void *)und_sym_producer_ctx->user_data);
  printf("><SB> sym th data addr: %p sym th  data points to: %p\n",
         &sym_thread_data, (void *)sym_thread_data);

  pthread_t producer_thread_id;
  pthread_t consumer_thread_id;
  void *producer_thread_rc;
  void *consumer_thread_rc;

  pthread_create(&producer_thread_id, NULL, get_undefined_sym,
                 und_sym_producer_ctx);

  pthread_create(&consumer_thread_id, NULL, resolve_undefined_sym,
                 und_sym_producer_ctx);

  pthread_join(producer_thread_id, &producer_thread_rc);
  rc = (int)(long)producer_thread_rc;
  if (rc != EXIT_SUCCESS) {
    printf("><SB> _%s() get_undefined_sym finished with rc: %s\n", __func__,
           strerror(rc));
    close(exec_fd);
    THREAD_RETURN(rc);
  }
  pthread_join(consumer_thread_id, &consumer_thread_rc);
  rc = (int)(long)consumer_thread_rc;
  printf("><SB> _%s() resolve_undefined_sym finished with rc: %s\n", __func__,
         strerror(rc));

  free(sym_thread_data->lib_path);
  sym_thread_data->lib_path = NULL;
  close(exec_fd);
  destroy_producer_thread_ctx(und_sym_producer_ctx);

  THREAD_RETURN(rc);
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

  pthread_t main_thread_id;
  void *main_thread_rc;
  main_thread_ctx_t main_thread_ctx = {
      .exe_file_name = exec_file,
      .lib_path = lib_path,
  };
  pthread_create(&main_thread_id, NULL, main_thread, &main_thread_ctx);
  pthread_join(main_thread_id, &main_thread_rc);
  rc = (int)(long)main_thread_rc;

  return rc;
}