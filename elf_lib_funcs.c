#include <ctype.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "elflib.h"
#include "iter.h"

void free_elf_file_descr(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr) {
    if (elf_file_descr->elf_str_tbls) {
      for (int i = 0; i < elf_file_descr->elf_str_tbl_num; i++) {
        if ((elf_file_descr->elf_str_tbls + i)) {
          if (elf_file_descr->elf_str_tbls[i].elf_str_table) {
            free(elf_file_descr->elf_str_tbls[i].elf_str_table);
            elf_file_descr->elf_str_tbls[i].elf_str_table = NULL;
          }
          if (elf_file_descr->elf_str_tbls[i].elf_str_tbl_name) {
            free(elf_file_descr->elf_str_tbls[i].elf_str_tbl_name);
            elf_file_descr->elf_str_tbls[i].elf_str_tbl_name = NULL;
          }
        }
      }
      free(elf_file_descr->elf_str_tbls);
      elf_file_descr->elf_str_tbls = NULL;
    }
    if (elf_file_descr->elf_sym_tbls) {
      for (int i = 0; i < elf_file_descr->elf_sym_tbl_num; i++) {
        if ((elf_file_descr->elf_sym_tbls + i)) {
          if (elf_file_descr->elf_sym_tbls[i].elf_sym_table) {
            free(elf_file_descr->elf_sym_tbls[i].elf_sym_table);
            elf_file_descr->elf_sym_tbls[i].elf_sym_table = NULL;
          }
          if (elf_file_descr->elf_sym_tbls[i].elf_sym_tbl_name) {
            free(elf_file_descr->elf_sym_tbls[i].elf_sym_tbl_name);
            elf_file_descr->elf_sym_tbls[i].elf_sym_tbl_name = NULL;
          }
        }
      }
      free(elf_file_descr->elf_sym_tbls);
      elf_file_descr->elf_sym_tbls = NULL;
    }
    if (elf_file_descr->elf_pg_hdr) {
      free(elf_file_descr->elf_pg_hdr);
      elf_file_descr->elf_pg_hdr = NULL;
    }
    if (elf_file_descr->elf_sec_hdr) {
      free(elf_file_descr->elf_sec_hdr);
      elf_file_descr->elf_sec_hdr = NULL;
    }
    if (elf_file_descr->elf_sym_tbls) {
      free(elf_file_descr->elf_sym_tbls);
      elf_file_descr->elf_sym_tbls = NULL;
    }
    if (elf_file_descr->elf_hdr) {
      free(elf_file_descr->elf_hdr);
      elf_file_descr->elf_hdr = NULL;
    }
    free(elf_file_descr);
    elf_file_descr = NULL;
  }
}

int get_elf_hdr_phoff(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr) {
    if (elf_file_descr->elf_hdr) {
      return (elf_file_descr->is_64bit
                  ? elf_file_descr->elf_hdr->elf_64_hdr.e_phoff
                  : elf_file_descr->elf_hdr->elf_32_hdr.e_phoff);
    }
  }

  return 0;
}

int get_elf_hdr_phnum(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr) {
    if (elf_file_descr->elf_hdr) {
      return (elf_file_descr->is_64bit
                  ? elf_file_descr->elf_hdr->elf_64_hdr.e_phnum
                  : elf_file_descr->elf_hdr->elf_32_hdr.e_phnum);
    }
  }

  return 0;
}

int get_elf_hdr_phentsize(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr) {
    if (elf_file_descr->elf_hdr) {
      return (elf_file_descr->is_64bit
                  ? elf_file_descr->elf_hdr->elf_64_hdr.e_phentsize
                  : elf_file_descr->elf_hdr->elf_32_hdr.e_phentsize);
    }
  }

  return 0;
}

int get_elf_pg_hdr_type(elf_file_descr_t *elf_file_descr, int indx) {
  if (elf_file_descr) {
    if ((elf_file_descr->elf_hdr) && (elf_file_descr->elf_pg_hdr)) {
      if (indx < get_elf_hdr_phnum(elf_file_descr)) {
        return (elf_file_descr->is_64bit
                    ? elf_file_descr->elf_pg_hdr[indx].elf_64_pg_hdr.p_type
                    : elf_file_descr->elf_pg_hdr[indx].elf_32_pg_hdr.p_type);
      }
    }
  }

  return -1;
}

int get_elf_pg_hdr_vaddr(elf_file_descr_t *elf_file_descr, int indx) {
  if (elf_file_descr) {
    if ((elf_file_descr->elf_hdr) && (elf_file_descr->elf_pg_hdr)) {
      if (indx < get_elf_hdr_phnum(elf_file_descr)) {
        return (elf_file_descr->is_64bit
                    ? elf_file_descr->elf_pg_hdr[indx].elf_64_pg_hdr.p_vaddr
                    : elf_file_descr->elf_pg_hdr[indx].elf_32_pg_hdr.p_vaddr);
      }
    }
  }

  return -1;
}

int get_elf_hdr_shoff(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr) {
    if (elf_file_descr->elf_hdr) {
      return (elf_file_descr->is_64bit
                  ? elf_file_descr->elf_hdr->elf_64_hdr.e_shoff
                  : elf_file_descr->elf_hdr->elf_32_hdr.e_shoff);
    }
  }

  return 0;
}

int get_elf_hdr_shnum(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr) {
    if (elf_file_descr->elf_hdr) {
      return (elf_file_descr->is_64bit
                  ? elf_file_descr->elf_hdr->elf_64_hdr.e_shnum
                  : elf_file_descr->elf_hdr->elf_32_hdr.e_shnum);
    }
  }

  return 0;
}

int get_elf_hdr_shentsize(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr) {
    if (elf_file_descr->elf_hdr) {
      return (elf_file_descr->is_64bit
                  ? elf_file_descr->elf_hdr->elf_64_hdr.e_shentsize
                  : elf_file_descr->elf_hdr->elf_32_hdr.e_shentsize);
    }
  }

  return 0;
}

int get_elf_hdr_shstrndx(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr) {
    if (elf_file_descr->elf_hdr) {
      return (elf_file_descr->is_64bit
                  ? elf_file_descr->elf_hdr->elf_64_hdr.e_shstrndx
                  : elf_file_descr->elf_hdr->elf_32_hdr.e_shstrndx);
    }
  }

  return 0;
}

int get_elf_sec_hdr_type(elf_file_descr_t *elf_file_descr, int indx) {
  if (elf_file_descr) {
    if ((elf_file_descr->elf_hdr) && (elf_file_descr->elf_sec_hdr)) {
      if (indx < get_elf_hdr_shnum(elf_file_descr)) {
        return (elf_file_descr->is_64bit
                    ? elf_file_descr->elf_sec_hdr[indx].elf_64_sec_hdr.sh_type
                    : elf_file_descr->elf_sec_hdr[indx].elf_32_sec_hdr.sh_type);
      }
    }
  }

  return -1;
}

int get_elf_sec_hdr_addr(elf_file_descr_t *elf_file_descr, int indx) {
  if (elf_file_descr) {
    if ((elf_file_descr->elf_hdr) && (elf_file_descr->elf_sec_hdr)) {
      if (indx < get_elf_hdr_shnum(elf_file_descr)) {
        return (elf_file_descr->is_64bit
                    ? elf_file_descr->elf_sec_hdr[indx].elf_64_sec_hdr.sh_addr
                    : elf_file_descr->elf_sec_hdr[indx].elf_32_sec_hdr.sh_addr);
      }
    }
  }

  return -1;
}

int get_elf_sec_hdr_strndx(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr) {
    if (elf_file_descr->elf_hdr) {
      return (elf_file_descr->is_64bit
                  ? elf_file_descr->elf_hdr->elf_64_hdr.e_shstrndx
                  : elf_file_descr->elf_hdr->elf_32_hdr.e_shstrndx);
    }
  }

  return -1;
}

int get_elf_sec_size(elf_file_descr_t *elf_file_descr, int indx) {
  if (elf_file_descr) {
    if ((elf_file_descr->elf_hdr) && (elf_file_descr->elf_sec_hdr)) {
      if (indx < get_elf_hdr_shnum(elf_file_descr)) {
        return (elf_file_descr->is_64bit
                    ? elf_file_descr->elf_sec_hdr[indx].elf_64_sec_hdr.sh_size
                    : elf_file_descr->elf_sec_hdr[indx].elf_32_sec_hdr.sh_size);
      }
    }
  }

  return -1;
}

int get_elf_sec_offset(elf_file_descr_t *elf_file_descr, int indx) {
  if (elf_file_descr) {
    if ((elf_file_descr->elf_hdr) && (elf_file_descr->elf_sec_hdr)) {
      if (indx < get_elf_hdr_shnum(elf_file_descr)) {
        return (
            elf_file_descr->is_64bit
                ? elf_file_descr->elf_sec_hdr[indx].elf_64_sec_hdr.sh_offset
                : elf_file_descr->elf_sec_hdr[indx].elf_32_sec_hdr.sh_offset);
      }
    }
  }

  return -1;
}

int get_elf_pg_offset(elf_file_descr_t *elf_file_descr, int indx) {
  if (elf_file_descr) {
    if ((elf_file_descr->elf_hdr) && (elf_file_descr->elf_pg_hdr)) {
      if (indx < get_elf_hdr_phnum(elf_file_descr)) {
        return (elf_file_descr->is_64bit
                    ? elf_file_descr->elf_pg_hdr[indx].elf_64_pg_hdr.p_offset
                    : elf_file_descr->elf_pg_hdr[indx].elf_32_pg_hdr.p_offset);
      }
    }
  }

  return -1;
}

int get_elf_pg_size(elf_file_descr_t *elf_file_descr, int indx) {
  if (elf_file_descr) {
    if ((elf_file_descr->elf_hdr) && (elf_file_descr->elf_pg_hdr)) {
      if (indx < get_elf_hdr_phnum(elf_file_descr)) {
        return (elf_file_descr->is_64bit
                    ? elf_file_descr->elf_pg_hdr[indx].elf_64_pg_hdr.p_filesz
                    : elf_file_descr->elf_pg_hdr[indx].elf_32_pg_hdr.p_filesz);
      }
    }
  }

  return -1;
}

int get_elf_sec_name_indx(elf_file_descr_t *elf_file_descr, int indx) {
  if (elf_file_descr) {
    if ((elf_file_descr->elf_hdr) && (elf_file_descr->elf_sec_hdr)) {
      if (indx < get_elf_hdr_shnum(elf_file_descr)) {
        return (elf_file_descr->is_64bit
                    ? elf_file_descr->elf_sec_hdr[indx].elf_64_sec_hdr.sh_name
                    : elf_file_descr->elf_sec_hdr[indx].elf_32_sec_hdr.sh_name);
      }
    }
  }

  return -1;
}

static int get_elf_sym_tab_entry_size(elf_file_descr_t *elf_file_descr) {
  if (elf_file_descr->is_64bit) {
    return sizeof(Elf64_Sym);
  }
  return sizeof(Elf32_Sym);
}

bool elf_sym_is_defined(elf_file_descr_t *elf_file_descr,
                        void *elf_sym_table_entry) {
  if (!elf_file_descr) {
    return false;
  }
  if (elf_file_descr->is_64bit) {
    return ((Elf64_Sym *)(elf_sym_table_entry))->st_shndx != SHN_UNDEF;
  } else {
    return ((Elf32_Sym *)(elf_sym_table_entry))->st_shndx != SHN_UNDEF;
  }
}
bool elf_sym_is_weak(elf_file_descr_t *elf_file_descr,
                     void *elf_sym_table_entry) {
  if (!elf_file_descr) {
    return false;
  }
  return (elf_sym_get_st_info(elf_file_descr, elf_sym_table_entry) >> 4) ==
         STB_WEAK;
}
bool elf_sym_is_local(elf_file_descr_t *elf_file_descr,
                      void *elf_sym_table_entry) {
  if (!elf_file_descr) {
    return false;
  }
  return (elf_sym_get_st_info(elf_file_descr, elf_sym_table_entry) >> 4) ==
         STB_LOCAL;
}
bool elf_sym_is_global(elf_file_descr_t *elf_file_descr,
                       void *elf_sym_table_entry) {
  if (!elf_file_descr) {
    return false;
  }
  return (elf_sym_get_st_info(elf_file_descr, elf_sym_table_entry) >> 4) ==
         STB_GLOBAL;
}

int elf_sym_get_st_name(elf_file_descr_t *elf_file_descr,
                        void *elf_sym_table_entry) {
  if (!elf_file_descr) {
    return -1;
  }
  if (elf_file_descr->is_64bit) {
    return ((Elf64_Sym *)(elf_sym_table_entry))->st_name;
  } else {
    return ((Elf32_Sym *)(elf_sym_table_entry))->st_name;
  }
}

int elf_sym_get_st_info(elf_file_descr_t *elf_file_descr,
                        void *elf_sym_table_entry) {
  if (!elf_file_descr) {
    return -1;
  }
  if (elf_file_descr->is_64bit) {
    return ((Elf64_Sym *)(elf_sym_table_entry))->st_info;
  } else {
    return ((Elf32_Sym *)(elf_sym_table_entry))->st_info;
  }
}

static int get_elf_sec_data(int fd, elf_file_descr_t *elf_file_descr, int indx,
                            void *data) {
  int rc = EXIT_SUCCESS;
  int offset = 0;
  int size = 0;
  if ((data == NULL) || (indx > get_elf_hdr_shnum(elf_file_descr))) {
    rc = EINVAL;
  }
  if (rc == EXIT_SUCCESS) {
    offset = get_elf_sec_offset(elf_file_descr, indx);
    size = get_elf_sec_size(elf_file_descr, indx);
    if (offset <= 0 || size <= 0) {
      // No data, there is nothing to do.
      return EXIT_SUCCESS;
    }
  }
  if (rc == EXIT_SUCCESS) {
    if ((lseek(fd, offset, SEEK_SET)) == -1) {
      rc = errno;
    }
  }
  if (rc == EXIT_SUCCESS) {
    int n = 0;
    if ((n = read(fd, data, size)) == -1) {
      rc = errno;
    }
  }

  return rc;
}

static int get_elf_pg_data(int fd, elf_file_descr_t *elf_file_descr, int size,
                           int offset, void *data) {
  int rc = EXIT_SUCCESS;
  if (!data) {
    rc = EINVAL;
  }
  if (rc == EXIT_SUCCESS) {
    if ((lseek(fd, offset, SEEK_SET)) == -1) {
      rc = errno;
    }
  }
  if (rc == EXIT_SUCCESS) {
    int n = 0;
    if ((n = read(fd, data, size)) == -1) {
      rc = errno;
    }
  }

  return rc;
}

static int get_elf_str_from_table(char *table, int table_size, int indx,
                                  char **name) {
  int rc = EXIT_SUCCESS;
  int i = 0;

  while ((table[i + indx] != 0x0) && (i < table_size)) {
    i++;
  }
  if (i == table_size) {
    rc = ENOENT;
  }
  if (rc == EXIT_SUCCESS) {
    *name = calloc(i + 1, 1);
    if (*name == NULL) {
      rc = ENOMEM;
    }
  }
  if (rc == EXIT_SUCCESS) {
    memcpy(*name, (char *)(table + indx), i);
  }

  return rc;
}

static int get_elf_sec_name(elf_file_descr_t *file_p, int indx, char **name) {
  int rc = EXIT_SUCCESS;
  int sec_name_indx = -1;

  if (!name) {
    rc = EINVAL;
  }
  if (rc == EXIT_SUCCESS) {
    sec_name_indx = get_elf_sec_name_indx(file_p, indx);
    if (sec_name_indx == -1) {
      rc = EINVAL;
    }
  }
  if (rc == EXIT_SUCCESS) {
    if (sec_name_indx >= file_p->elf_str_tbls[0].elf_str_tbl_size) {
      rc = EINVAL;
    }
  }
  if (rc == EXIT_SUCCESS) {
    rc = get_elf_str_from_table(file_p->elf_str_tbls[0].elf_str_table,
                                file_p->elf_str_tbls[0].elf_str_tbl_size,
                                sec_name_indx, name);
  }

  return rc;
}

static int elf_populate_str_table(int fd, elf_file_descr_t *file_p,
                                  int sec_indx, int tbl_indx) {
  int rc = EXIT_SUCCESS;

  file_p->elf_str_tbls[tbl_indx].elf_str_table =
      malloc(get_elf_sec_size(file_p, sec_indx));
  if (!file_p->elf_str_tbls[tbl_indx].elf_str_table) {
    rc = ENOMEM;
  }
  if (rc == EXIT_SUCCESS) {
    rc = get_elf_sec_data(fd, file_p, sec_indx,
                          (void *)file_p->elf_str_tbls[tbl_indx].elf_str_table);
  }
  if (rc == EXIT_SUCCESS) {
    file_p->elf_str_tbls[tbl_indx].elf_str_tbl_size =
        get_elf_sec_size(file_p, sec_indx);
    file_p->elf_str_tbls[tbl_indx].sec_indx = sec_indx;
    char *name = NULL;
    if (get_elf_sec_name(file_p, file_p->elf_str_tbls[tbl_indx].sec_indx,
                         &name) == -1) {
      rc = EINVAL;
    } else {
      file_p->elf_str_tbls[tbl_indx].elf_str_tbl_name = name;
#ifdef DEBUG
      printf("><SB> string table index: %d name:%s\n",
             file_p->elf_str_tbls[tbl_indx].sec_indx,
             file_p->elf_str_tbls[tbl_indx].elf_str_tbl_name);
#endif
    }
  }

  return rc;
}

static int elf_populate_sym_table(int fd, elf_file_descr_t *file_p,
                                  int sec_indx, int tbl_indx) {
  int rc = EXIT_SUCCESS;

  int sec_size = get_elf_sec_size(file_p, sec_indx);
  file_p->elf_sym_tbls[tbl_indx].elf_sym_table = malloc(sec_size);
  if (!file_p->elf_sym_tbls[tbl_indx].elf_sym_table) {
    rc = ENOMEM;
  }
#ifdef DEBUG
  printf("><SB> >>>>>>> Table: %d Section: %d size: %d allocated at: %p\n",
         tbl_indx, sec_indx, sec_size,
         (void *)file_p->elf_sym_tbls[tbl_indx].elf_sym_table);
#endif
  if (rc == EXIT_SUCCESS) {
    rc = get_elf_sec_data(fd, file_p, sec_indx,
                          (void *)file_p->elf_sym_tbls[tbl_indx].elf_sym_table);
  }
  if (rc == EXIT_SUCCESS) {
    file_p->elf_sym_tbls[tbl_indx].elf_sym_tbl_size =
        get_elf_sec_size(file_p, sec_indx);
    file_p->elf_sym_tbls[tbl_indx].sec_indx = sec_indx;
    file_p->elf_sym_tbls[tbl_indx].elf_sym_tbl_num_entry =
        file_p->elf_sym_tbls[tbl_indx].elf_sym_tbl_size /
        get_elf_sym_tab_entry_size(file_p);
    char *name = NULL;
    if (get_elf_sec_name(file_p, file_p->elf_sym_tbls[tbl_indx].sec_indx,
                         &name) == -1) {
      rc = EINVAL;
    } else {
      file_p->elf_sym_tbls[tbl_indx].elf_sym_tbl_name = name;
      int r =
          strcmp(file_p->elf_sym_tbls[tbl_indx].elf_sym_tbl_name, ".dynsym");
      if (r == 0) {
        file_p->elf_sym_tbls[tbl_indx].is_dynamic = true;
      } else {
        file_p->elf_sym_tbls[tbl_indx].is_dynamic = false;
      }
#ifdef DEBUG
      printf("><SB> sym table index: %d name: %s\n",
             file_p->elf_sym_tbls[tbl_indx].sec_indx,
             file_p->elf_sym_tbls[tbl_indx].elf_sym_tbl_name);
#endif
    }
  }

  return rc;
}

static int get_elf_strtab_indx(elf_file_descr_t *elf_file_descr, char *tbl_name,
                               int *indx) {
  for (int i = 0; i < elf_file_descr->elf_str_tbl_num; i++) {
    if (strcmp(elf_file_descr->elf_str_tbls[i].elf_str_tbl_name, tbl_name) ==
        0) {
      *indx = i;
      return EXIT_SUCCESS;
    }
  }

  return ENOENT;
}

int elf_sym_get_string(elf_file_descr_t *file_p, bool is_dynamic, int st_name,
                       char **name) {
  int rc = ENOENT;
  if (name) {
    int ndx = -1;
    if (is_dynamic) {
      rc = get_elf_strtab_indx(file_p, ".dynstr", &ndx);
    } else {
      rc = get_elf_strtab_indx(file_p, ".strtab", &ndx);
    }
    if (rc == EXIT_SUCCESS) {
      rc = get_elf_str_from_table(file_p->elf_str_tbls[ndx].elf_str_table,
                                  file_p->elf_str_tbls[ndx].elf_str_tbl_size,
                                  st_name, name);
    }
  }
  return rc;
}

int get_els_exec_link_time_base_addr(elf_file_descr_t *elf_file_descr,
                                     Elf64_Addr *eltba) {

  for (int i = 0; i < get_elf_hdr_phnum(elf_file_descr); i++) {
    if ((get_elf_pg_hdr_type(elf_file_descr, i)) == PT_LOAD) {
      Elf64_Addr vaddr = get_elf_pg_hdr_vaddr(elf_file_descr, i);
      if (vaddr == -1) {
        return ENOENT;
      }
      *eltba = vaddr;
      return EXIT_SUCCESS;
    }
  }

  return ENOENT;
}

static int process_nt_auxv(bool is_64bit, char *data, int size,
                           Elf64_Addr *at_base) {
  int rc = EXIT_SUCCESS;
  int field_sz = 0;
  if (is_64bit) {
    field_sz = sizeof(Elf64_Addr);
  } else {
    field_sz = sizeof(Elf32_Addr);
  }
  int i = 0;

  while (i < size) {
    Elf64_Addr key = 0, val = 0;
    memcpy(&key, (void *)(data + i), field_sz);
    memcpy(&val, (void *)(data + i + field_sz), field_sz);
    if (key == AT_NULL) {
      break;
    }
    if (key == AT_BASE) {
      *at_base = val;
    }
    i += (field_sz * 2);
  }

  return rc;
}

int printf_file_name(char *data, int data_size) {
  int i = 0;
  while (data[i] != 0 && i < data_size) {
    i++;
  }
  if (i < data_size) {
    printf("%s\n", data);
  }
  return i + 1;
}

static int process_nt_file(bool is_64bit, char *data, int size) {
  int rc = EXIT_SUCCESS;
  int field_sz = (is_64bit) ? sizeof(Elf64_Addr) : sizeof(Elf32_Addr);

  Elf64_Xword num_entry = 0;
  memcpy(&num_entry, data, field_sz);
  printf("NT_FILE contains %lu entries\n", num_entry);

  const char *addr_start_ptr = data + field_sz;
  const char *offset_start_ptr = addr_start_ptr + (num_entry * (field_sz * 2));
  const char *fn_start_ptr = offset_start_ptr + (num_entry * field_sz);

  int indx = 0;
  for (int i = 0; i < num_entry; i++) {
    Elf64_Addr a_1 = 0, a_2 = 0, off_1 = 0;
    memcpy(&a_1, addr_start_ptr + (i * (2 * field_sz)), field_sz);
    memcpy(&a_2, addr_start_ptr + (i * (2 * field_sz) + field_sz), field_sz);
    printf("Addr start: 0x%0lx, Addr end: 0x%0lx\n", a_1, a_2);
    memcpy(&off_1, offset_start_ptr + (i * field_sz), field_sz);
    printf("Offset: 0x%0lx\n", off_1);
    printf("file name: ");
    indx += printf_file_name(((char *)fn_start_ptr) + indx,
                             size - (fn_start_ptr - data) - indx);
  }

  return rc;
}

static int process_elf_pg_hdr_pt_note(int fd, elf_file_descr_t *file_p,
                                      void *pg_header) {
  int rc = EXIT_SUCCESS;
  char *data = NULL;

  if (!file_p || !pg_header) {
    return EINVAL;
  }
  int size = 0;
  int data_offset = 0;
  if (file_p->is_64bit) {
    size = ((Elf64_Phdr *)(pg_header))->p_filesz;
    data_offset = ((Elf64_Phdr *)(pg_header))->p_offset;
  } else {
    size = ((Elf32_Phdr *)(pg_header))->p_filesz;
    data_offset = ((Elf32_Phdr *)(pg_header))->p_offset;
  }
  if (size == 0) {
    // There is no PT_NOTE, nothing to do
    return rc;
  }
  data = malloc(size);
  if (!data) {
    return ENOMEM;
  }
  rc = get_elf_pg_data(fd, file_p, size, data_offset, (void *)data);
  if (rc == EXIT_SUCCESS) {
    Elf32_Word name_sz = 0;
    Elf32_Word descr_sz = 0;
    Elf32_Word type = 0;
    Elf32_Word offset = 0;
    while ((offset + 12) < size && rc == EXIT_SUCCESS) {
      char *name = NULL;
      char *descr = NULL;
      memcpy(&name_sz, (char *)(data + offset), 4);
      offset += 4;
      memcpy(&descr_sz, (char *)(data + offset), 4);
      offset += 4;
      memcpy(&type, (char *)(data + offset), 4);
      offset += 4;
      if (name_sz > 0) {
        name = malloc(name_sz);
        if (!name) {
          rc = ENOMEM;
          continue;
        }
        memcpy(name, (char *)(data + offset), name_sz);
        offset += (name_sz + 3) & ~3;
      }
      if (descr_sz > 0) {
        descr = malloc(descr_sz);
        if (!descr) {
          rc = ENOMEM;
          continue;
        }
        memcpy(descr, (char *)(data + offset), descr_sz);
        offset += (descr_sz + 3) & ~3;
      }
      switch (type) {
      case NT_AUXV:
        Elf64_Addr at_base = 0;
#ifdef DEBUG
        printf("Found NT_AUXV\n");
#endif
        if ((process_nt_auxv(file_p->is_64bit, descr, descr_sz, &at_base)) ==
            EXIT_SUCCESS) {
          file_p->at_base = at_base;
          if (file_p->at_base) {
            printf("Found AT_BASE, base address: 0x%lx\n", file_p->at_base);
          }
        }
        break;
      case NT_FILE:
#ifdef DEBUG
        printf("NT_FILE\n");
#endif
        process_nt_file(file_p->is_64bit, descr, descr_sz);
      }
      if (name) {
        free(name);
      }
      if (descr) {
        free(descr);
      }
    }
  }

  if (data) {
    free(data);
    data = NULL;
  }

  return rc;
}

static int populate_string_table(int fd, elf_file_descr_t *file_p, int indx) {
  int rc = EXIT_SUCCESS;
  elf_str_tbl_t *elf_str_tbl = NULL;
  if (!file_p->elf_str_tbls) {
    file_p->elf_str_tbls = malloc(sizeof(elf_str_tbl_t));
    if (!file_p->elf_str_tbls) {
      rc = ENOMEM;
    }
  } else {
    elf_str_tbl =
        realloc(file_p->elf_str_tbls,
                sizeof(elf_str_tbl_t) * (file_p->elf_str_tbl_num + 1));
    if (!elf_str_tbl) {
      rc = ENOMEM;
    } else {
      file_p->elf_str_tbls = elf_str_tbl;
    }
  }
  if (rc == EXIT_SUCCESS) {
    rc = elf_populate_str_table(fd, file_p, indx, file_p->elf_str_tbl_num);
    if (rc == EXIT_SUCCESS) {
      file_p->elf_str_tbl_num++;
    }
  }

  return rc;
}

static int populate_sym_table(int fd, elf_file_descr_t *file_p, int indx) {
  int rc = EXIT_SUCCESS;
  elf_sym_tbl_t *elf_sym_tbl = NULL;

  if (!file_p->elf_sym_tbls) {
    file_p->elf_sym_tbls = malloc(sizeof(elf_sym_tbl_t));
    if (!file_p->elf_str_tbls) {
      rc = ENOMEM;
    }
  } else {
    elf_sym_tbl =
        realloc(file_p->elf_sym_tbls,
                sizeof(elf_sym_tbl_t) * (file_p->elf_sym_tbl_num + 1));
    if (!elf_sym_tbl) {
      rc = ENOMEM;
    } else {
      file_p->elf_sym_tbls = elf_sym_tbl;
    }
  }
  if (rc == EXIT_SUCCESS) {
    rc = elf_populate_sym_table(fd, file_p, indx, file_p->elf_sym_tbl_num);
    if (rc == EXIT_SUCCESS) {
      file_p->elf_sym_tbl_num++;
    }
  }

  return rc;
}

int print_str_table(elf_file_descr_t *file_p) {
  int rc = EXIT_SUCCESS;
  for (int y = 0; y < file_p->elf_str_tbl_num; y++) {
    printf("><SB> String table name: %s Section index: %d\n",
           file_p->elf_str_tbls[y].elf_str_tbl_name,
           file_p->elf_str_tbls[y].sec_indx);
    int i = 0;
    char *str = NULL;
    do {
      free(str);
      str = NULL;
      get_elf_str_from_table(file_p->elf_str_tbls[y].elf_str_table + 1,
                             file_p->elf_str_tbls[y].elf_str_tbl_size, i, &str);
      i += strlen(str) + 1;
      printf("\t- %s\n", str);
    } while (*str);
  }

  return rc;
}

int print_sym_table(elf_file_descr_t *file_p) {
  int rc = EXIT_SUCCESS;

  for (int y = 0; y < file_p->elf_sym_tbl_num; y++) {
    printf("><SB> Symbol table name: %s Section index: %d\n",
           file_p->elf_sym_tbls[y].elf_sym_tbl_name,
           file_p->elf_sym_tbls[y].sec_indx);
    char *str = NULL;
    size_t entry_size = 0;
    if (file_p->is_64bit) {
      entry_size = sizeof(Elf64_Sym);
    } else {
      entry_size = sizeof(Elf32_Sym);
    }
    iterator_t *iter =
        iterator_init((void *)file_p->elf_sym_tbls[y].elf_sym_table, entry_size,
                      file_p->elf_sym_tbls[y].elf_sym_tbl_num_entry);
    void *entry = NULL;
    entry = iterator_next(iter);
    while (entry) {
      if (ELF_ST_TYPE(elf_sym_get_st_info(file_p, entry)) == STT_OBJECT) {
        int st_name = elf_sym_get_st_name(file_p, entry);
        rc = elf_sym_get_string(file_p, file_p->elf_sym_tbls[y].is_dynamic,
                                st_name, &str);
        if (rc == EXIT_SUCCESS) {
          printf("\t- %s\n", str);
        } else {
          printf("><SB> failed to find st_name\n");
          break;
        }
      }
      // }
      entry = iterator_next(iter);
    }
    iterator_free(iter);
  }

  return rc;
}

int process_elf_file(char *fn, elf_file_descr_t **elf_file_descr) {
  int rc = EXIT_SUCCESS;
  elf_file_descr_t *file_p = NULL;
  int fd = -1;

  if ((fn == NULL) || (elf_file_descr == NULL)) {
    return EINVAL;
  }
  if (rc == EXIT_SUCCESS) {
    fd = open(fn, O_RDONLY);
    if (fd < 0) {
      rc = errno;
      fprintf(stderr, "failed tp open executable file %s with error: %s\n", fn,
              strerror(rc));
    }
  }
  if (rc == EXIT_SUCCESS) {
    file_p = calloc(1, sizeof(elf_file_descr_t));
    if (!file_p) {
      rc = ENOMEM;
    }
  }
  if (rc == EXIT_SUCCESS) {
    file_p->elf_hdr = calloc(1, sizeof(elf_hdr_u_t));
    if (!file_p->elf_hdr) {
      rc = ENOMEM;
    }
  }
  if (rc == EXIT_SUCCESS) {
    if (read(fd, file_p->elf_hdr->elf_hdr_raw,
             sizeof(file_p->elf_hdr->elf_hdr_raw)) == -1) {
      rc = errno;
    }
  }
  if (rc == EXIT_SUCCESS) {
    // Check if it is a valid ELF file
    if (memcmp(file_p->elf_hdr->elf_64_hdr.e_ident, ELFMAG, SELFMAG) != 0) {
      fprintf(stderr, "><SB> %s(): file %s is not a valid ELF file\n", __func__,
              fn);
      rc = EINVAL;
    }
  }
  if (rc == EXIT_SUCCESS) {
    if (file_p->elf_hdr->elf_64_hdr.e_ident[EI_DATA] != ELFDATA2LSB &&
        file_p->elf_hdr->elf_64_hdr.e_ident[EI_DATA] != ELFDATA2MSB) {
      fprintf(stderr, "><SB> %s(): file %s is not a valid ELF file\n", __func__,
              fn);
      rc = EINVAL;
    }
  }
  if (rc == EXIT_SUCCESS) {
    if (file_p->elf_hdr->elf_64_hdr.e_ident[EI_CLASS] != ELFCLASS64 &&
        file_p->elf_hdr->elf_32_hdr.e_ident[EI_CLASS] != ELFCLASS32) {
      fprintf(stderr, "><SB> %s(): file %s is not a valid ELF file\n", __func__,
              fn);
      rc = EINVAL;
    }
  }
  if (rc == EXIT_SUCCESS) {
    if (file_p->elf_hdr->elf_64_hdr.e_ident[EI_CLASS] == ELFCLASS64) {
      file_p->is_64bit = true;
    }
// Processing program headers if offset is not 0
#ifdef DEBUG
    printf("><SB> %s(): Start of a program header table: 0x%02x\n", __func__,
           get_elf_hdr_phoff(file_p));
    printf("><SB> %s(): Size of a program header table entry: 0x%02x\n",
           __func__, get_elf_hdr_phentsize(file_p));
#endif
    if (get_elf_hdr_phoff(file_p) > 0) {
      file_p->elf_pg_hdr =
          calloc(get_elf_hdr_phnum(file_p), get_elf_hdr_phentsize(file_p));
      if (!file_p->elf_pg_hdr) {
        rc = ENOMEM;
      }
      if (rc == EXIT_SUCCESS) {
        if ((lseek(fd, get_elf_hdr_phoff(file_p), SEEK_SET)) == -1) {
          rc = errno;
        }
      }
      if (rc == EXIT_SUCCESS) {
        void *entry = NULL;
        iterator_t *iter =
            iterator_init(file_p->elf_pg_hdr, get_elf_hdr_phentsize(file_p),
                          get_elf_hdr_phnum(file_p));
        entry = iterator_next(iter);
        while (entry && rc == EXIT_SUCCESS) {
          if ((read(fd, entry, get_elf_hdr_phentsize(file_p))) == -1) {
            rc = errno;
          }
          entry = iterator_next(iter);
        }
        iterator_free(iter);
      }
      if (rc == EXIT_SUCCESS) {
        void *entry = NULL;
        int type = 0;
        iterator_t *iter =
            iterator_init(file_p->elf_pg_hdr, get_elf_hdr_phentsize(file_p),
                          get_elf_hdr_phnum(file_p));
        entry = iterator_next(iter);
        while (entry && rc == EXIT_SUCCESS) {
          if (file_p->is_64bit) {
            type = ((Elf64_Phdr *)(entry))->p_type;
          } else {
            type = ((Elf32_Phdr *)(entry))->p_type;
          }
          switch (type) {
          case PT_NOTE:
#ifdef DEBUG
            printf("Found PT_NOTE, processing...\n");
#endif
            rc = process_elf_pg_hdr_pt_note(fd, file_p, entry);
            break;
          case PT_LOAD:
#ifdef DEBUG
            printf("Found PT_LOAD, processing...\n");
#endif
            break;
          case PT_DYNAMIC:
#ifdef DEBUG
            printf("Found PT_DYNAMIC, processing...\n");
#endif
            break;
          case PT_INTERP:
#ifdef DEBUG
            printf("Found PT_INTERP, processing...\n");
#endif
            break;
          case PT_SHLIB:
#ifdef DEBUG
            printf("Found PT_SHLIB, processing...\n");
#endif
            break;
          case PT_PHDR:
#ifdef DEBUG
            printf("Found PT_PHDR, processing...\n");
#endif
            break;
          case PT_TLS:
#ifdef DEBUG
            printf("Found PT_TLS, processing...\n");
#endif
            break;
          case PT_GNU_EH_FRAME:
#ifdef DEBUG
            printf("Found PT_GNU_EH_FRAME, processing...\n");
#endif
            break;
          }
          entry = iterator_next(iter);
        }
        iterator_free(iter);
      }
    }
  }
  if ((get_elf_hdr_shnum(file_p) > 0) && (get_elf_hdr_shoff(file_p) > 0) &&
      (rc == EXIT_SUCCESS)) {
    // Processing Section header info
    file_p->elf_sec_hdr =
        calloc(get_elf_hdr_shnum(file_p), sizeof(elf_sec_hdr_u_t));
    if (!file_p->elf_sec_hdr) {
      rc = ENOMEM;
    }
    if (rc == EXIT_SUCCESS) {
      if ((lseek(fd, get_elf_hdr_shoff(file_p), SEEK_SET)) == -1) {
        rc = errno;
      }
    }
    if (rc == EXIT_SUCCESS) {
      for (int i = 0; i < get_elf_hdr_shnum(file_p) && rc == EXIT_SUCCESS;
           i++) {
        if ((read(fd, &file_p->elf_sec_hdr[i].elf_sec_hdr_raw,
                  get_elf_hdr_shentsize(file_p))) == -1) {
          rc = errno;
        }
      }
    }
    if (rc == EXIT_SUCCESS) {
      if (get_elf_sec_hdr_strndx(file_p) != 0) {
        // Allocate memory for Section Header String table
        file_p->elf_str_tbls = calloc(1, sizeof(elf_str_tbl_t));
        if (!file_p->elf_str_tbls) {
          rc = ENOMEM;
        }
      }
      if (rc == EXIT_SUCCESS) {
        rc = elf_populate_str_table(fd, file_p, get_elf_sec_hdr_strndx(file_p),
                                    0);
      }
      if (rc == EXIT_SUCCESS) {
        file_p->elf_str_tbl_num++;
      }
    }
    if (rc == EXIT_SUCCESS) {
      // Processing some Section headers
      if (get_elf_hdr_shnum(file_p) != SHN_UNDEF) {
        for (int i = 0; i < get_elf_hdr_shnum(file_p) && rc == EXIT_SUCCESS;
             i++) {
          switch (get_elf_sec_hdr_type(file_p, i)) {
          case SHT_SYMTAB:
#ifdef DEBUG
            printf("SHT_SYMTAB\n");
#endif
            rc = populate_sym_table(fd, file_p, i);
            break;
          case SHT_DYNSYM:
#ifdef DEBUG
            printf("SHT_DYNSYM\n");
#endif
            rc = populate_sym_table(fd, file_p, i);
            break;
          case SHT_STRTAB:
#ifdef DEBUG
            printf("SHT_STRTAB\n");
#endif
            if (i == get_elf_sec_hdr_strndx(file_p)) {
              // Section Header String table has alreade been processed
              continue;
            }
            rc = populate_string_table(fd, file_p, i);
            break;
          case SHT_RELA:
#ifdef DEBUG
            printf("SHT_RELA\n");
#endif
            break;
          case SHT_DYNAMIC:
#ifdef DEBUG
            printf("SHT_DYNAMIC\n");
#endif
            break;
          case SHT_NOTE:
#ifdef DEBUG
            printf("SHT_NOTE\n");
#endif
            break;
          case SHT_REL:
#ifdef DEBUG
            printf("SHT_REL\n");
#endif
            break;
          case SHT_SHLIB:
#ifdef DEBUG
            printf("SHT_SHLIB\n");
#endif
            break;
          case SHT_SYMTAB_SHNDX:
#ifdef DEBUG
            printf("SHT_SYMTAB_SHNDX\n");
#endif
            break;
          }
        }
      }
    }
  }
  if (fd != -1) {
    close(fd);
    fd = -1;
  }
  if (rc != EXIT_SUCCESS) {
    free_elf_file_descr(file_p);
  } else {
    *elf_file_descr = file_p;
#ifdef DEBUG
    print_str_table(file_p);
    print_sym_table(file_p);
#endif
  }

  return rc;
}
