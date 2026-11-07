cmake_minimum_required(VERSION 3.20)
project(rmi_cap_metadata_test LANGUAGES CXX)

set(CHOUPI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../../choupi/src")
add_executable(rmi_cap_metadata_test rmi_cap_metadata_test.cpp)
target_compile_features(rmi_cap_metadata_test PRIVATE cxx_std_17)
target_compile_definitions(rmi_cap_metadata_test PRIVATE PC_VERSION=1)
target_compile_options(rmi_cap_metadata_test PRIVATE
  -Wall -Wextra -UNDEBUG -Wno-ignored-qualifiers -Wno-address-of-packed-member)
target_include_directories(rmi_cap_metadata_test PRIVATE "${CHOUPI_SOURCE_DIR}")
