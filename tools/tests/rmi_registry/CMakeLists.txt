cmake_minimum_required(VERSION 3.20)
project(rmi_registry_test LANGUAGES CXX)

set(CHOUPI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../../choupi/src")
add_executable(
  rmi_registry_test
  rmi_registry_test.cpp
  "${CHOUPI_SOURCE_DIR}/context.cpp"
  "${CHOUPI_SOURCE_DIR}/frame.cpp"
  "${CHOUPI_SOURCE_DIR}/heap.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jref_t.cpp"
  "${CHOUPI_SOURCE_DIR}/stack.cpp")
target_compile_features(rmi_registry_test PRIVATE cxx_std_17)
target_compile_definitions(rmi_registry_test PRIVATE PC_VERSION=1)
target_compile_options(rmi_registry_test PRIVATE
  -Wall -Wextra -ffunction-sections -fdata-sections -UNDEBUG
  -Wno-ignored-qualifiers -Wno-address-of-packed-member)
target_include_directories(rmi_registry_test PRIVATE
  "${CHOUPI_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/../../../choupi/os/src")
target_link_options(rmi_registry_test PRIVATE -Wl,--gc-sections)
