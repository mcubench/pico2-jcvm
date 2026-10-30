cmake_minimum_required(VERSION 3.20)
project(persistent_inventory_test LANGUAGES CXX)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(
  persistent_inventory_test
  persistent_inventory_test.cpp
  "${REPO_ROOT}/choupi/src/persistent_inventory.cpp")
target_compile_features(persistent_inventory_test PRIVATE cxx_std_17)
target_compile_options(persistent_inventory_test PRIVATE -Wall -Wextra)
target_include_directories(persistent_inventory_test PRIVATE
  "${REPO_ROOT}/choupi/src"
  "${REPO_ROOT}/choupi/os/src")
