cmake_minimum_required(VERSION 3.20)
project(functionality_review_probes LANGUAGES CXX)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(VM "${REPO_ROOT}/choupi/src")
add_executable(review_probe review_probe.cpp
  "${VM}/context.cpp" "${VM}/frame.cpp" "${VM}/heap.cpp"
  "${VM}/stack.cpp" "${VM}/jc_types/jc_array.cpp"
  "${VM}/jc_types/jc_object.cpp" "${VM}/jc_types/jref_t.cpp"
  "${VM}/jc_bytecodes/bc_arithmetic.cpp"
  "${REPO_ROOT}/tools/tests/jcarray/filesystem_stub.cpp")
target_compile_features(review_probe PRIVATE cxx_std_17)
target_compile_definitions(review_probe PRIVATE PC_VERSION=1)
target_compile_options(review_probe PRIVATE -Wall -Wextra -ffunction-sections
  -fdata-sections -Wno-ignored-qualifiers -Wno-address-of-packed-member)
target_include_directories(review_probe PRIVATE "${VM}" "${REPO_ROOT}/choupi/os/src")
target_link_options(review_probe PRIVATE -Wl,--gc-sections)
option(REVIEW_UBSAN "Instrument current runtime source for undefined behavior" OFF)
if(REVIEW_UBSAN)
  target_compile_options(review_probe PRIVATE -fsanitize=undefined)
  target_link_options(review_probe PRIVATE -fsanitize=undefined)
endif()
add_executable(persistence_probe persistence_probe.cpp
  "${VM}/jc_handlers/flashmemory.cpp" "${VM}/persistent_inventory.cpp"
  "${VM}/heap.cpp" "${VM}/jc_types/jc_array.cpp"
  "${VM}/jc_types/jc_object.cpp" "${VM}/jc_types/jc_instance.cpp"
  "${VM}/jc_types/jref_t.cpp" "${VM}/jc_handlers/jc_static.cpp"
  "${REPO_ROOT}/tools/tests/persistent_transaction/filesystem_stub.cpp")
target_compile_features(persistence_probe PRIVATE cxx_std_17)
target_compile_definitions(persistence_probe PRIVATE PC_VERSION=1)
target_compile_options(persistence_probe PRIVATE -Wall -Wextra -ffunction-sections
  -fdata-sections -Wno-ignored-qualifiers -Wno-address-of-packed-member)
target_include_directories(persistence_probe PRIVATE
  "${REPO_ROOT}/tools/tests/persistent_transaction" "${VM}")
target_link_options(persistence_probe PRIVATE -Wl,--gc-sections)
add_executable(session_probe session_probe.cpp "${VM}/vpcd_session.cpp")
target_compile_features(session_probe PRIVATE cxx_std_17)
target_compile_definitions(session_probe PRIVATE PC_VERSION=1)
target_include_directories(session_probe PRIVATE "${VM}")
