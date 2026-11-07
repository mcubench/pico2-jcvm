cmake_minimum_required(VERSION 3.20)
project(rmi_array_cap_inspect LANGUAGES CXX)

set(CHOUPI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../../../choupi/src")
add_executable(rmi_array_cap_inspect inspect_remote_methods.cpp)
target_compile_features(rmi_array_cap_inspect PRIVATE cxx_std_17)
target_compile_definitions(rmi_array_cap_inspect PRIVATE PC_VERSION=1)
target_compile_options(rmi_array_cap_inspect PRIVATE
  -Wall -Wextra -Wno-ignored-qualifiers -Wno-address-of-packed-member)
target_include_directories(rmi_array_cap_inspect PRIVATE "${CHOUPI_SOURCE_DIR}")
