##
## Copyright (c) 2014-present, Facebook, Inc.
## All rights reserved.
##
## This source code is licensed under the University of Illinois/NCSA Open
## Source License found in the LICENSE file in the root directory of this
## source tree. An additional grant of patent rights can be found in the
## PATENTS file in the same directory.
##

cmake_minimum_required(VERSION 3.3)

project(JSObjects
  LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS NO)
set(CMAKE_C_STANDARD_REQUIRED YES)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_POSITION_INDEPENDENT_CODE YES)

find_package(BISON)
find_package(FLEX)

FLEX_TARGET(JSON_SCANNER
            ${JSObjects_SOURCE_DIR}/Sources/libjson/tokenizer.l
            ${JSObjects_BINARY_DIR}/tokenizer.c
            COMPILE_FLAGS "--header-file=${JSObjects_BINARY_DIR}/tokenizer.h \
                           --nounistd --batch --never-interactive")
BISON_TARGET(JSON_PARSER
             ${JSObjects_SOURCE_DIR}/Sources/libjson/parser.y
             ${JSObjects_BINARY_DIR}/parser.c
             COMPILE_FLAGS "--no-lines")
ADD_FLEX_BISON_DEPENDENCY(JSON_SCANNER JSON_PARSER)

add_library(jsobjects STATIC
    ${BISON_JSON_PARSER_OUTPUTS}
    ${FLEX_JSON_SCANNER_OUTPUTS}
    Sources/JSObjects.cpp
    Sources/libjson/json.c
    Sources/libjson/qstring.c)
target_include_directories(jsobjects PUBLIC
    ${JSObjects_SOURCE_DIR}/Headers)
target_include_directories(jsobjects PRIVATE
    ${JSObjects_SOURCE_DIR}/Sources/libjson
    ${JSObjects_BINARY_DIR})

if((CMAKE_C_COMPILER_ID MATCHES "GNU" AND
    CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 4.6) OR
   ((CMAKE_C_COMPILER_ID MATCHES "Clang" AND
     NOT CMAKE_C_SIMULATE_ID MATCHES "MSVC") AND
    CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0))
  # this warning is enabled by -Wall on newer compilers and must be disabled
  # for the generated parser code
  set_source_files_properties(${JSObjects_BINARY_DIR}/parser.c PROPERTIES
      COMPILE_FLAGS -Wno-error=unused-but-set-variable)

  target_compile_definitions(jsobjects PRIVATE
    _POSIX_C_SOURCE=200809L)
  target_compile_options(jsobjects PRIVATE -Wall -Wextra -Werror)
  target_compile_options(jsobjects PRIVATE
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-sign-compare)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(jsobjects PRIVATE /W3 /DSTRICT /wd4996)
  target_compile_options(jsobjects PRIVATE /MP)
endif()
