package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "libscout",
    srcs = [
        "elflib/elf_lib_funcs.c",
        "elflib/elflib.h",
        "include/libscout.h",
        "src/libscout.c",
        "src/main.c",
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
