cmake_minimum_required(VERSION 3.20)
project(persistent_transaction_test LANGUAGES CXX)

set(CHOUPI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../../choupi/src")
add_executable(
  persistent_transaction_test
  persistent_transaction_test.cpp
  filesystem_stub.cpp
  "${CHOUPI_SOURCE_DIR}/jc_handlers/flashmemory.cpp"
  "${CHOUPI_SOURCE_DIR}/persistent_inventory.cpp"
  "${CHOUPI_SOURCE_DIR}/heap.cpp"
  "${CHOUPI_SOURCE_DIR}/context.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jc_array.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jc_object.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jc_instance.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jref_t.cpp"
  "${CHOUPI_SOURCE_DIR}/stack.cpp")
target_compile_features(persistent_transaction_test PRIVATE cxx_std_17)
target_compile_definitions(persistent_transaction_test PRIVATE PC_VERSION=1)
target_compile_options(persistent_transaction_test PRIVATE
  -Wall -Wextra -ffunction-sections -fdata-sections
  -Wno-ignored-qualifiers -Wno-address-of-packed-member)
target_include_directories(persistent_transaction_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}" "${CHOUPI_SOURCE_DIR}")
target_link_options(persistent_transaction_test PRIVATE -Wl,--gc-sections)
