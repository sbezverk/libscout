package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "libscout",
    srcs = [
        "elf_lib_funcs.c",
        "elflib.h",
        "iter.h",
        "iterator.c",
        "libscout.c",
        "libscout.h",
        "main.c",
        "search_for_lib.c",
        "sym_cache.c",
    ],
    copts = [
        "--std=c23",
        "-g",
        "-O0",
    ],
    defines = [
        "_POSIX_C_SOURCE=200809",
    ],
    deps = [
        "//avl",
    ],
)

cc_binary(
    name = "process_lib_test",
    srcs = [
        "elf_lib_funcs.c",
        "elflib.h",
        "process_lib_test.c",
    ],
    copts = [
        "--std=c23",
        "-g",
        "-O0",
    ],
    defines = [
        "_POSIX_C_SOURCE=200809",
    ],
    deps = [
    ],
)
