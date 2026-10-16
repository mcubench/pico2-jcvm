cmake_minimum_required(VERSION 3.20)
project(jcarray_transient_test LANGUAGES CXX)

if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH is required for the mbedTLS headers")
endif()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(CHOUPI_SOURCE_DIR "${REPO_ROOT}/choupi/src")
set(MBEDTLS_INCLUDE_DIR "$ENV{PICO_SDK_PATH}/lib/mbedtls/include")

add_executable(
  jcarray_transient_test
  "${REPO_ROOT}/choupi/test/jcarray_transient_test.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jc_array.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jc_object.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jref_t.cpp"
  "${CHOUPI_SOURCE_DIR}/heap.cpp"
  "${CHOUPI_SOURCE_DIR}/context.cpp"
  "${CHOUPI_SOURCE_DIR}/stack.cpp"
  "${CHOUPI_SOURCE_DIR}/jni.cpp"
  filesystem_stub.cpp)

target_compile_features(jcarray_transient_test PRIVATE cxx_std_17)
target_compile_options(jcarray_transient_test PRIVATE
  -Wall -Wextra -ffunction-sections -fdata-sections -fsanitize=address)
target_compile_definitions(jcarray_transient_test PRIVATE
  PC_VERSION
  MBEDTLS_CONFIG_FILE="mbedtls_config_pc.h")
target_include_directories(jcarray_transient_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../short_apdu"
  "${CHOUPI_SOURCE_DIR}"
  "${REPO_ROOT}/src"
  "${MBEDTLS_INCLUDE_DIR}"
  "${MBEDTLS_INCLUDE_DIR}/mbedtls")
target_link_options(jcarray_transient_test PRIVATE
  -Wl,--gc-sections -fsanitize=address)
