cmake_minimum_required(VERSION 3.20)
project(gp_registry_test LANGUAGES CXX)

add_executable(gp_registry_test gp_registry_test.cpp)
target_compile_features(gp_registry_test PRIVATE cxx_std_17)
target_compile_options(gp_registry_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(gp_registry_test PRIVATE
  "${CMAKE_SOURCE_DIR}/../../../src")
