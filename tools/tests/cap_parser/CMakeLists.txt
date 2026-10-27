cmake_minimum_required(VERSION 3.20)
project(cap_parser_test LANGUAGES CXX)

set(CHOUPI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../../choupi/src")
add_executable(
  cap_parser_test
  cap_parser_test.cpp
  "${CHOUPI_SOURCE_DIR}/jc_handlers/jc_cap.cpp")
target_compile_features(cap_parser_test PRIVATE cxx_std_17)
target_compile_options(cap_parser_test PRIVATE
  -Wall -Wextra -Werror -Wno-error=ignored-qualifiers
  -Wno-error=address-of-packed-member -fsanitize=address,undefined
  -fno-omit-frame-pointer)
target_link_options(cap_parser_test PRIVATE -fsanitize=address,undefined)
target_include_directories(cap_parser_test PRIVATE "${CHOUPI_SOURCE_DIR}")
