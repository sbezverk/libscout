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

int main(int argc, char *argv[]) {
  int rc = EXIT_SUCCESS;
  int fd = -1;

  if (argc < 2) {
    fprintf(stderr, "full path and library name is required, exiting...\n");
    exit(rc);
  }
  fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "failed to open library %s with error: %s\n", argv[1],
            strerror(rc));
    exit(rc);
  }
  elf_file_descr_t *file_p = NULL;

  rc = process_elf_file(fd, &file_p);

  if (rc != EXIT_SUCCESS) {
    fprintf(stderr, "failed to process library %s with error: %s\n", argv[1],
            strerror(rc));
    exit(rc);
  }

  free_elf_file_descr(file_p);
  return rc;
}