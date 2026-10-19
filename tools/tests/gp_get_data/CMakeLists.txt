cmake_minimum_required(VERSION 3.20)
project(gp_get_data_test LANGUAGES CXX)

add_executable(gp_get_data_test gp_get_data_test.cpp)
target_compile_features(gp_get_data_test PRIVATE cxx_std_17)
target_compile_options(gp_get_data_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(gp_get_data_test PRIVATE
  "${CMAKE_SOURCE_DIR}/../../../src")
