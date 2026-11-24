cmake_minimum_required(VERSION 3.20)
project(pqc_backend_test LANGUAGES C)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(PQC_DIR "${REPO_ROOT}/choupi/src/pqc")

add_executable(pqc_backend_test
  pqc_backend_test.c
  "${PQC_DIR}/pqc_arena.c"
  "${PQC_DIR}/pqc_mlkem.c"
  "${PQC_DIR}/pqc_mldsa.c")
target_include_directories(pqc_backend_test PRIVATE
  "${PQC_DIR}"
  "${REPO_ROOT}/third_party/mlkem-native"
  "${REPO_ROOT}/third_party/mldsa-native")
target_compile_options(pqc_backend_test PRIVATE
  -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all)
target_link_options(pqc_backend_test PRIVATE -fsanitize=address,undefined)
