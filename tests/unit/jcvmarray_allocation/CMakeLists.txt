cmake_minimum_required(VERSION 3.16)
project(jcvmarray_allocation_test LANGUAGES CXX)

add_executable(
  jcvmarray_allocation_test
  jcvmarray_allocation_test.cpp
  flashmemory_rollback_generation_stub.cpp
  ../../../choupi/src/heap.cpp
  ../../../choupi/src/jc_types/jc_array.cpp
  ../../../choupi/src/jc_types/jref_t.cpp
)
target_compile_features(jcvmarray_allocation_test PRIVATE cxx_std_17)
target_include_directories(jcvmarray_allocation_test PRIVATE
  ../../../choupi/os/src
)
target_compile_options(jcvmarray_allocation_test PRIVATE
  -Wall -Wextra -ffunction-sections -fdata-sections
  -Wno-ignored-qualifiers -Wno-address-of-packed-member
  -Wno-mismatched-new-delete
)
target_link_options(jcvmarray_allocation_test PRIVATE -Wl,--gc-sections)
