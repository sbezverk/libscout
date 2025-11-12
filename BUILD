package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "libscout",
    srcs = [
        "elf_lib_funcs.c",
        "elflib.h",
        "libscout.c",
        "libscout.h",
        "main.c",
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
