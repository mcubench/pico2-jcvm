cmake_minimum_required(VERSION 3.13)
project(execution_budget_test CXX)

add_executable(execution_budget_test
  execution_budget_test.cpp
  "${CMAKE_CURRENT_LIST_DIR}/../../../choupi/src/execution_budget.cpp")
target_include_directories(execution_budget_test PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../../choupi/src")
target_compile_features(execution_budget_test PRIVATE cxx_std_17)
