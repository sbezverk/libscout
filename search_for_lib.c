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

int search_for_lib(char *lib_path, sym_cache_t *cache) {
  DIR *dir_stream = NULL;
  struct dirent *dir_entry = NULL;
  int rc = EXIT_SUCCESS;
  char lib_suffix[] = {".so"};

  if ((dir_stream = opendir(lib_path)) == NULL) {
    rc = errno;
  }
  if (rc == EXIT_SUCCESS) {
    char new_lib_path[PATH_MAX] = {0};
    while ((dir_entry = readdir(dir_stream)) != NULL) {
      if (dir_entry->d_type == 0x04) {
        if ((strcmp(dir_entry->d_name, ".") == 0) &&
            (strlen(dir_entry->d_name) == 1)) {
          continue;
        } else if ((strcmp(dir_entry->d_name, "..") == 0) &&
                   (strlen(dir_entry->d_name) == 2)) {
          continue;
        } else {
          snprintf((char *)new_lib_path, PATH_MAX, "%s/%s", lib_path,
                   dir_entry->d_name);
          rc = search_for_lib(new_lib_path, cache);
          if (rc != EXIT_SUCCESS) {
            goto cleanup;
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
        snprintf((char *)new_lib_path, PATH_MAX, "%s/%s", lib_path,
                 dir_entry->d_name);
        // Populating cache with best effort, as some files even though
        // have .so extension are not valid ELF library.
        populate_cache(cache, new_lib_path);
      }
    }
  }
cleanup:
  if (dir_stream) {
    closedir(dir_stream);
    dir_stream = NULL;
  }

  return rc;
}
