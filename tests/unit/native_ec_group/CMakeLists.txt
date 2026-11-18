cmake_minimum_required(VERSION 3.20)
project(native_ec_group_test LANGUAGES C CXX)

if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH is required for the mbedTLS sources")
endif()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(CHOUPI_SOURCE_DIR "${REPO_ROOT}/choupi/src")
set(MBEDTLS_DIR "$ENV{PICO_SDK_PATH}/lib/mbedtls")

add_executable(
  native_ec_group_test
  native_ec_group_test.cpp
  "${MBEDTLS_DIR}/library/bignum.c"
  "${MBEDTLS_DIR}/library/bignum_core.c"
  "${MBEDTLS_DIR}/library/constant_time.c"
  "${MBEDTLS_DIR}/library/ecp.c"
  "${MBEDTLS_DIR}/library/ecp_curves.c"
  "${MBEDTLS_DIR}/library/platform_util.c")

target_compile_features(native_ec_group_test PRIVATE cxx_std_17)
target_compile_options(native_ec_group_test PRIVATE -fsanitize=address)
target_compile_options(native_ec_group_test PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
target_compile_definitions(native_ec_group_test PRIVATE
  PC_VERSION
  MBEDTLS_CONFIG_FILE="mbedtls_config_pc.h")
target_include_directories(native_ec_group_test PRIVATE
  "${CHOUPI_SOURCE_DIR}"
  "${MBEDTLS_DIR}/include"
  "${MBEDTLS_DIR}/library")
target_link_options(native_ec_group_test PRIVATE -fsanitize=address)
