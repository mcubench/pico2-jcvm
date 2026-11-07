cmake_minimum_required(VERSION 3.20)
project(cap_instruction_boundaries_test LANGUAGES CXX)

add_executable(
  cap_instruction_boundaries_test
  "${CMAKE_SOURCE_DIR}/../cap_instruction_boundaries_test.cpp")
target_compile_features(cap_instruction_boundaries_test PRIVATE cxx_std_17)
target_compile_options(cap_instruction_boundaries_test PRIVATE
  -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined
  -fno-omit-frame-pointer)
target_link_options(cap_instruction_boundaries_test PRIVATE
  -fsanitize=address,undefined)
