cmake_minimum_required(VERSION 3.20)
project(gp_delete_test LANGUAGES CXX)

add_executable(gp_delete_test gp_delete_test.cpp)
target_compile_features(gp_delete_test PRIVATE cxx_std_17)
target_compile_options(gp_delete_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(gp_delete_test PRIVATE
  "${CMAKE_SOURCE_DIR}/../../../src")
