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

int search_for_lib(mpsc_buffer_t *buffer, char *current_path) {
  DIR *dir_stream = NULL;
  struct dirent *dir_entry = NULL;
  int rc = EXIT_SUCCESS;
  char lib_suffix[] = {".so"};

  // printf("><SB> %s() started, current directory: %s\n", __func__,
  //     ctx->buffert_path);
  if (is_buffer_producer_shutdown_requested(buffer)) {
    return rc;
  }
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
        while (buffer_is_full(buffer) &&
               !is_buffer_producer_shutdown_requested(buffer)) {
          pthread_cond_wait(&buffer->not_full, &buffer->write_mutex);
        }
        if (is_buffer_producer_shutdown_requested(buffer)) {
          printf("><SB> Consumer requested to shutdown, exiting...\n");
          // Producer needs to terminate
          pthread_mutex_unlock(&buffer->write_mutex);
          goto cleanup;
        }
        buffer->buffer[buffer->tail] = calloc(PATH_MAX, 1);

        snprintf(buffer->buffer[buffer->tail], PATH_MAX, "%s/%s", current_path,
                 dir_entry->d_name);
        //        printf("><SB> %s() storing library %s in the buffer.\n",
        //        __func__,
        //               (char *)buffer->buffer[buffer->tail]);
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
cleanup:
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

  THREAD_RETURN(rc);
}
