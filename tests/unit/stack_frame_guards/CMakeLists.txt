cmake_minimum_required(VERSION 3.20)
project(stack_frame_guards_test LANGUAGES CXX)

set(CHOUPI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../../choupi/src")
add_executable(
  stack_frame_guards_test
  stack_frame_guards_test.cpp
  "${CHOUPI_SOURCE_DIR}/context.cpp"
  "${CHOUPI_SOURCE_DIR}/frame.cpp"
  "${CHOUPI_SOURCE_DIR}/heap.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jc_array.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jc_object.cpp"
  "${CHOUPI_SOURCE_DIR}/jc_types/jref_t.cpp"
  "${CHOUPI_SOURCE_DIR}/stack.cpp"
  "${CMAKE_SOURCE_DIR}/../jcarray/filesystem_stub.cpp")
target_compile_features(stack_frame_guards_test PRIVATE cxx_std_17)
target_compile_definitions(stack_frame_guards_test PRIVATE PC_VERSION=1)
target_compile_options(stack_frame_guards_test PRIVATE
  -Wall -Wextra -ffunction-sections -fdata-sections
  -fsanitize=address,undefined -fno-sanitize-recover=all -UNDEBUG
  -Wno-ignored-qualifiers -Wno-address-of-packed-member)
target_include_directories(stack_frame_guards_test PRIVATE
  "${CHOUPI_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/../../../choupi/os/src")
target_link_options(stack_frame_guards_test PRIVATE
  -Wl,--gc-sections -fsanitize=address,undefined)
