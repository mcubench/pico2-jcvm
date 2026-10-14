cmake_minimum_required(VERSION 3.20)
project(install_status_test LANGUAGES CXX)

add_executable(install_status_test install_status_test.cpp)
target_compile_features(install_status_test PRIVATE cxx_std_17)
target_compile_options(install_status_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(install_status_test PRIVATE
  "${CMAKE_SOURCE_DIR}/../../../src")
