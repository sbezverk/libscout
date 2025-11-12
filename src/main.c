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

#include "elflib/elflib.h"
#include "libscout.h"

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
  fprintf(stdout, "><SB> executable file: %s\n", exec_file);
  int exec_fd = -1;
  exec_fd = open(exec_file, O_RDONLY);
  if (exec_fd < 0) {
    rc = errno;
    fprintf(stderr, "failed tp open executable file %s with error: %s\n",
            exec_file, strerror(rc));
  }
  if (lib_path[0] == 0x0) {
    fprintf(stderr, "\"-l libraries path\" or set LIB_PATH variable is "
                    "required parameter\n");
    return EINVAL;
  }
  lib_node_t *exec_file_node = NULL;
  if (rc == EXIT_SUCCESS) {
    rc = create_lib_node(exec_file, &exec_file_node);
    if (rc != EXIT_SUCCESS) {
      fprintf(stderr,
              "failed to create node for executable file %s with error: %s\n",
              exec_file, strerror(rc));
    }
  }
  if (rc == EXIT_SUCCESS) {
    rc = populate_node_symtable(exec_fd, exec_file_node, false, true);
    if (rc != EXIT_SUCCESS) {
      fprintf(stderr,
              "failed to populate exec file %s sym table  with error: %s\n",
              exec_file_node->lib_name, strerror(rc));
    }
  }
  int undef = avl2_get_size(exec_file_node->undefined);
  printf("><SB> exec file %s has %d undefined global symbols...\n",
         exec_file_node->lib_name, avl2_get_size(exec_file_node->undefined));
  if (undef > 0) {
    dependency_tree_t *deps = NULL;
    rc = find_dependency(exec_file_node, &deps, lib_path);
    if (rc != EXIT_SUCCESS) {
      fprintf(
          stderr,
          "failed to find dependencies for file %s sym table  with error: %s\n",
          exec_file_node->lib_name, strerror(rc));
    }
  }

  close(exec_fd);
  destroy_lib_node(exec_file_node);

  return rc;
}