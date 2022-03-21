cc_library(
    name = "getopt",
    srcs = [
        "getopt.c",
    ],
    hdrs = [
        "getopt.h",
    ],
)

cc_binary(
    name = "regsgen2",
    srcs = [
        "Constants.h",
        "Context.h",
        "Definitions.h",
        "FlagSet.cpp",
        "FlagSet.h",
        "GDBDefinitions.cpp",
        "GDBDefinitions.h",
        "GDBVectorSet.cpp",
        "GDBVectorSet.h",
        "KeyValue.h",
        "LLDBDefinitions.cpp",
        "LLDBDefinitions.h",
        "ParseConstants.cpp",
        "ParseConstants.h",
        "RegisterSet.cpp",
        "RegisterSet.h",
        "RegisterTemplate.cpp",
        "RegisterTemplate.h",
        "main.cpp",
    ],
    copts = [
        "-std=c++17",
    ],
    visibility = ["//:__subpackages__"],
    deps = [
        ":getopt",
        "//Tools/JSObjects:jsobjects",
    ],
)
