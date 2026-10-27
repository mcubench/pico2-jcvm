cmake_minimum_required(VERSION 3.20)
project(apdu_parser_fuzz LANGUAGES CXX)

add_executable(apdu_parser_fuzz apdu_parser_fuzz.cpp)
target_compile_features(apdu_parser_fuzz PRIVATE cxx_std_17)
target_compile_options(apdu_parser_fuzz PRIVATE
  -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(apdu_parser_fuzz PRIVATE -fsanitize=address,undefined)
target_include_directories(apdu_parser_fuzz PRIVATE "${CMAKE_SOURCE_DIR}/../../../src")
