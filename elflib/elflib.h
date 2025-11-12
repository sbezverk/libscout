#pragma once

#include <elf.h>
#include <stdbool.h>

typedef union elf_hdr_ {
  Elf64_Ehdr elf_64_hdr;
  Elf32_Ehdr elf_32_hdr;
  char elf_hdr_raw[sizeof(Elf64_Ehdr)];
} elf_hdr_u_t;

typedef union elf_pg_hdr_ {
  Elf64_Phdr elf_64_pg_hdr;
  Elf32_Phdr elf_32_pg_hdr;
  char elf_pg_hdr_raw[sizeof(Elf64_Phdr)];
} elf_pg_hdr_u_t;

typedef union elf_sec_hdr_ {
  Elf64_Shdr elf_64_sec_hdr;
  Elf32_Shdr elf_32_sec_hdr;
  char elf_sec_hdr_raw[sizeof(Elf64_Shdr)];
} elf_sec_hdr_u_t;

typedef union elf_sym_tbl_entry_ {
  Elf64_Sym elf_64_sym_entry;
  Elf32_Sym elf_32_sym_entry;
  char elf_sym_entry_raw[sizeof(Elf64_Sym)];
} elf_sym_tbl_entry_u_t;

typedef struct elf_sym_tbl_ {
  int sec_indx;
  elf_sym_tbl_entry_u_t *elf_sym_table;
  int elf_sym_tbl_size;
  char *elf_sym_tbl_name;
  int elf_sym_tbl_num_entry;
  bool is_dynamic;
} elf_sym_tbl_t;

typedef struct elf_str_tbl_ {
  int sec_indx;
  char *elf_str_tbl_name;
  int elf_str_tbl_size;
  char *elf_str_table;
} elf_str_tbl_t;

typedef struct elf_file_ {
  int fd;
  bool is_64bit;
  uint32_t file_type;
  elf_hdr_u_t *elf_hdr;
  elf_pg_hdr_u_t *elf_pg_hdr;
  elf_sec_hdr_u_t *elf_sec_hdr;
  int elf_sym_tbl_num;
  elf_sym_tbl_t *elf_sym_tbls;
  int elf_str_tbl_num;
  elf_str_tbl_t *elf_str_tbls;
  Elf64_Addr at_base;
} elf_file_descr_t;

int prepare_open_core_file(char *org_core_file, int *core_file_hd);

int process_elf_file(int elf_file_dscr, elf_file_descr_t **elf_file_p);

void free_elf_file_descr(elf_file_descr_t *elf_file_descr);

int get_elf_hdr_phoff(elf_file_descr_t *elf_file_descr);

int get_elf_hdr_phnum(elf_file_descr_t *elf_file_descr);

int get_elf_hdr_phentsize(elf_file_descr_t *elf_file_descr);

int get_elf_hdr_shoff(elf_file_descr_t *elf_file_descr);

int get_elf_hdr_shnum(elf_file_descr_t *elf_file_descr);

int get_elf_hdr_shentsize(elf_file_descr_t *elf_file_descr);

int get_elf_hdr_shstrndx(elf_file_descr_t *elf_file_descr);

int get_elf_pg_hdr_type(elf_file_descr_t *elf_file_descr, int indx);

int get_elf_pg_hdr_vaddr(elf_file_descr_t *elf_file_descr, int indx);

int get_elf_sec_hdr_type(elf_file_descr_t *elf_file_descr, int indx);

int get_elf_sec_hdr_addr(elf_file_descr_t *elf_file_descr, int indx);

int get_elf_sec_hdr_strndx(elf_file_descr_t *elf_file_descr);

int get_elf_all_sym_symtab(elf_file_descr_t *elf_file_descr, char ***symbols,
                           int *sym_count);

int get_elf_sym_symtab(elf_file_descr_t *elf_file_descr, char *symbol,
                       void **symbol_data);

int get_els_exec_link_time_base_addr(elf_file_descr_t *elf_file_descr,
                                     Elf64_Addr *eltba);

int elf_sym_get_string(elf_file_descr_t *elf_file_descr, bool is_dynamic,
                       int st_name, char **name);

bool elf_sym_is_defined(elf_file_descr_t *elf_file_descr,
                        elf_sym_tbl_entry_u_t elf_sym_table);

bool elf_sym_is_weak(elf_file_descr_t *elf_file_descr,
                     elf_sym_tbl_entry_u_t elf_sym_table);

bool elf_sym_is_local(elf_file_descr_t *elf_file_descr,
                      elf_sym_tbl_entry_u_t elf_sym_table);

bool elf_sym_is_global(elf_file_descr_t *elf_file_descr,
                       elf_sym_tbl_entry_u_t elf_sym_table);

int elf_sym_get_st_name(elf_file_descr_t *elf_file_descr,
                        elf_sym_tbl_entry_u_t elf_sym_table);
