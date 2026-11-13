cmake_minimum_required(VERSION 3.20)
project(rp2350_fs_overlay_test LANGUAGES CXX)

set(REPO_ROOT "${CMAKE_SOURCE_DIR}/../../..")
add_executable(
  rp2350_fs_overlay_test
  rp2350_fs_overlay_test.cpp
  "${REPO_ROOT}/src/cap_store.cpp"
  "${REPO_ROOT}/src/pico_filesystem.cpp"
  "${REPO_ROOT}/src/gp_lifecycle_recovery.cpp"
  "${REPO_ROOT}/choupi/src/jc_handlers/flashmemory.cpp"
  "${REPO_ROOT}/choupi/src/persistent_inventory.cpp"
  "${REPO_ROOT}/choupi/src/heap.cpp"
  "${REPO_ROOT}/choupi/src/context.cpp"
  "${REPO_ROOT}/choupi/src/stack.cpp"
  "${REPO_ROOT}/choupi/src/jc_types/jc_array.cpp"
  "${REPO_ROOT}/choupi/src/jc_types/jc_object.cpp"
  "${REPO_ROOT}/choupi/src/jc_types/jc_instance.cpp"
  "${REPO_ROOT}/choupi/src/jc_types/jref_t.cpp")
target_compile_features(rp2350_fs_overlay_test PRIVATE cxx_std_17)
target_compile_options(rp2350_fs_overlay_test PRIVATE
  -Wall -Wextra -ffunction-sections -fdata-sections
  -Wno-unused-parameter -Wno-format -Wno-ignored-qualifiers
  -Wno-address-of-packed-member)
target_compile_definitions(rp2350_fs_overlay_test PRIVATE
  RP2350_FS_OVERLAY_TEST=1)
target_include_directories(rp2350_fs_overlay_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/fake_include"
  "${REPO_ROOT}/choupi/src"
  "${REPO_ROOT}/choupi/os/src"
  "${REPO_ROOT}/src")
target_link_options(rp2350_fs_overlay_test PRIVATE -Wl,--gc-sections)
