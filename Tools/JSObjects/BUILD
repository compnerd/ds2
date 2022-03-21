load("@rules_flex//flex:flex.bzl", "flex")
load("@rules_bison//bison:bison.bzl", "bison")

flex(
    name = "tokenizer",
    src = "Sources/libjson/tokenizer.l",
    flex_options = [
        "--nounistd",
        "--batch",
        "--never-interactive",
    ],
)

bison(
    name = "parser",
    src = "Sources/libjson/parser.y",
    bison_options = [
        "--no-lines",
    ],
)

cc_library(
    name = "jsobjects",
    srcs = [
        "Headers/JSObjects/JSObjects.h",
        "Sources/JSObjects.cpp",
        "Sources/libjson/json.c",
        "Sources/libjson/json_internal.h",
        "Sources/libjson/jsoncb.h",
        "Sources/libjson/qstring.c",
        "Sources/libjson/qstring.h",
        ":parser",
        ":tokenizer",
    ],
    includes = [
        ".",
        "Headers",
        "JSObjects",
        "Sources/libjson",
    ],
    visibility = ["//:__subpackages__"],
)
