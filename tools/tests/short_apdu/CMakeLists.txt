cmake_minimum_required(VERSION 3.20)
project(short_apdu_test LANGUAGES CXX)

add_executable(short_apdu_test short_apdu_test.cpp)
target_compile_features(short_apdu_test PRIVATE cxx_std_17)
target_compile_options(short_apdu_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(short_apdu_test PRIVATE "${CMAKE_SOURCE_DIR}/../../../src")

set(CHOUPI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../../choupi/src")
add_executable(
  apdu_context_test
  apdu_context_test.cpp
  apdu_context_stubs.cpp
  "${CHOUPI_SOURCE_DIR}/context.cpp"
  "${CHOUPI_SOURCE_DIR}/heap.cpp"
  "${CHOUPI_SOURCE_DIR}/stack.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jc_array.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jc_object.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jref_t.cpp")
target_compile_features(apdu_context_test PRIVATE cxx_std_17)
target_compile_options(apdu_context_test PRIVATE
  -Wall -Wextra -ffunction-sections -fdata-sections)
target_include_directories(apdu_context_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CHOUPI_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/../../../src")
target_link_options(apdu_context_test PRIVATE -Wl,--gc-sections)
