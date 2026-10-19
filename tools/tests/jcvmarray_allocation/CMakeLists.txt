cmake_minimum_required(VERSION 3.16)
project(jcvmarray_allocation_test LANGUAGES CXX)

add_executable(jcvmarray_allocation_test jcvmarray_allocation_test.cpp)
target_compile_features(jcvmarray_allocation_test PRIVATE cxx_std_17)
target_compile_options(jcvmarray_allocation_test PRIVATE
  -Wall -Wextra -Wno-ignored-qualifiers -Wno-address-of-packed-member
)
