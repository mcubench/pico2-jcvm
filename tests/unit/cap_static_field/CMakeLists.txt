cmake_minimum_required(VERSION 3.20)
project(cap_static_field_test LANGUAGES CXX)

add_executable(
  cap_static_field_test
  "${CMAKE_SOURCE_DIR}/../cap_static_field_test.cpp")
target_compile_features(cap_static_field_test PRIVATE cxx_std_17)
target_compile_definitions(cap_static_field_test PRIVATE PC_VERSION=1)
target_include_directories(cap_static_field_test PRIVATE
  "${CMAKE_SOURCE_DIR}/../../../choupi/src")
target_compile_options(cap_static_field_test PRIVATE
  -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer
  -Wno-address-of-packed-member)
target_link_options(cap_static_field_test PRIVATE
  -fsanitize=address,undefined)
