cmake_minimum_required(VERSION 3.20)
project(short_apdu_test LANGUAGES CXX)

add_executable(short_apdu_test short_apdu_test.cpp)
target_compile_features(short_apdu_test PRIVATE cxx_std_17)
target_compile_options(short_apdu_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(short_apdu_test PRIVATE "${CMAKE_SOURCE_DIR}/../../../src")
