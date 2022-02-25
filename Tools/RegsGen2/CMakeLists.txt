##
## Copyright (c) 2014-present, Facebook, Inc.
## All rights reserved.
##
## This source code is licensed under the University of Illinois/NCSA Open
## Source License found in the LICENSE file in the root directory of this
## source tree. An additional grant of patent rights can be found in the
## PATENTS file in the same directory.
##

cmake_minimum_required(VERSION 3.13)

project(RegsGen2
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

add_subdirectory(../JSObjects "${CMAKE_CURRENT_BINARY_DIR}/JSObjects")

add_library(getopt OBJECT
  getopt.c)

add_executable(regsgen2
  FlagSet.cpp
  GDBDefinitions.cpp
  GDBVectorSet.cpp
  LLDBDefinitions.cpp
  ParseConstants.cpp
  RegisterSet.cpp
  RegisterTemplate.cpp
  main.cpp)
target_link_libraries(regsgen2 PRIVATE
  getopt jsobjects)
