cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED PICO_BOARD)
  set(PICO_BOARD pico2 CACHE STRING "Pico board")
endif()
if(NOT DEFINED PICO_FLASH_SIZE_BYTES)
  set(PICO_FLASH_SIZE_BYTES 2097152 CACHE STRING "Reported Pico 2 flash size")
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico2_jcvm C CXX ASM)
set(JAVACARD_SUPPORT_GENERATED_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/build/javacard-support")
include("${JAVACARD_SUPPORT_GENERATED_DIR}/javacard-support.cmake")
set(PICO_CXX_ENABLE_EXCEPTIONS 1 CACHE BOOL "Enable C++ exceptions for CHOUPI")
option(CHOUPI_DEBUG_HEAP_GC "Emit periodic CHOUPI heap GC summaries" OFF)
option(CHOUPI_DEBUG_PERSISTENT_GC
  "Exercise experimental persistent tag reclamation after each applet APDU" OFF)
option(CHOUPI_DEBUG_HEAP_GC_TRACE "Emit per-object CHOUPI heap GC tracing" OFF)
option(CHOUPI_DEBUG_MALLOC "Emit Pico SDK allocator tracing" OFF)
add_compile_options(-Wall -Wextra -Werror)
if(LOCAL_ARM_NEWLIB_INCLUDE)
  include_directories(SYSTEM "${LOCAL_ARM_NEWLIB_INCLUDE}")
  include_directories(SYSTEM "${LOCAL_ARM_NEWLIB_INCLUDE}/c++/14.2.1")
  include_directories(SYSTEM "${LOCAL_ARM_NEWLIB_INCLUDE}/c++/14.2.1/arm-none-eabi")
endif()
if(LOCAL_ARM_NEWLIB_LIBRARY_DIR)
  link_directories("${LOCAL_ARM_NEWLIB_LIBRARY_DIR}")
endif()
pico_sdk_init()
if(TARGET pico_mbedtls_headers)
  # This SDK checkout stores mbedtls_config.h inside include/mbedtls while its
  # Pico wrapper includes the header without the mbedtls/ prefix.
  target_include_directories(pico_mbedtls_headers SYSTEM INTERFACE
    "${PICO_SDK_PATH}/lib/mbedtls/include/mbedtls")
endif()

add_executable(pico2_jcvm_smoke src/board_smoke.c)
target_link_libraries(pico2_jcvm_smoke PRIVATE pico_stdlib pico_rand)
pico_enable_stdio_usb(pico2_jcvm_smoke 1)
target_sources(pico2_jcvm_smoke PRIVATE
  "${PICO_SDK_PATH}/src/rp2_common/pico_stdio_usb/stdio_usb_descriptors.c")
pico_enable_stdio_uart(pico2_jcvm_smoke 0)
pico_add_extra_outputs(pico2_jcvm_smoke)

# Compile CHOUPI's platform-independent C++ VM against each RP2350 toolchain.
# Platform startup and the Rust OS are integrated in a later bring-up step.
file(GLOB_RECURSE CHOUPI_VM_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/*.cpp")
list(FILTER CHOUPI_VM_SOURCES EXCLUDE REGEX "/main(_arm|_pc)?\\.cpp$")
list(FILTER CHOUPI_VM_SOURCES EXCLUDE REGEX
  "/native_(random|cipher|aes_cbc|aes_ctr|des_cbc|sha256|mac|mbedtls_digest|rsa_cipher|rsa_key|rsa_signature|ecdsa_signature|key)\\.cpp$")
if(JC_SUPPORT_SECURE_RANDOM)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_random.cpp")
endif()
list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_cipher.cpp")
if(JC_SUPPORT_CIPHER_AES_CBC_NOPAD OR JC_SUPPORT_CIPHER_GETINSTANCE_AES_CBC_NOPAD OR
   JC_SUPPORT_CIPHER_AES_ECB_NOPAD OR JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M1 OR
   JC_SUPPORT_CIPHER_AES_CBC_ISO9797_M2 OR JC_SUPPORT_CIPHER_AES_CBC_PKCS5 OR
   JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M1 OR JC_SUPPORT_CIPHER_AES_ECB_ISO9797_M2 OR
   JC_SUPPORT_CIPHER_AES_ECB_PKCS5 OR JC_SUPPORT_SIGNATURE_AES_MAC_128_NOPAD OR
   JC_SUPPORT_SIGNATURE_AES_CMAC_128)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_aes_cbc.cpp")
endif()
if(JC_SUPPORT_CIPHER_AES_CTR OR JC_SUPPORT_CIPHER_GETINSTANCE_AES_CTR)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_aes_ctr.cpp")
endif()
if(JC_SUPPORT_CIPHER_DES_CBC_NOPAD OR JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD OR
   JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M1 OR JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M2 OR
   JC_SUPPORT_CIPHER_DES_CBC_PKCS5 OR JC_SUPPORT_CIPHER_DES_ECB_NOPAD OR
   JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M1 OR JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M2 OR
   JC_SUPPORT_CIPHER_DES_ECB_PKCS5 OR JC_SUPPORT_SIGNATURE_DES_MAC8_NOPAD OR
   JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M1 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M1 OR
   JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M2 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M2 OR
   JC_SUPPORT_SIGNATURE_DES_MAC4_PKCS5 OR JC_SUPPORT_SIGNATURE_DES_MAC8_PKCS5 OR
   JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M2_ALG3 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M2_ALG3 OR
   JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M1_ALG3 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M1_ALG3)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_des_cbc.cpp")
endif()
if(JC_SUPPORT_SIGNATURE_DES_MAC8_NOPAD OR JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M1 OR
   JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M1 OR JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M2 OR
   JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M2 OR JC_SUPPORT_SIGNATURE_DES_MAC4_PKCS5 OR
   JC_SUPPORT_SIGNATURE_DES_MAC8_PKCS5 OR JC_SUPPORT_SIGNATURE_AES_MAC_128_NOPAD OR
   JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M2_ALG3 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M2_ALG3 OR
   JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M1_ALG3 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M1_ALG3 OR
   JC_SUPPORT_SIGNATURE_AES_CMAC_128)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_mac.cpp")
endif()
if(JC_SUPPORT_SHA256)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_sha256.cpp")
endif()
if(JC_SUPPORT_SHA1 OR JC_SUPPORT_SHA224 OR JC_SUPPORT_SHA384 OR JC_SUPPORT_SHA512)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_mbedtls_digest.cpp")
endif()
if(JC_SUPPORT_CIPHER_RSA_PKCS1 OR JC_SUPPORT_CIPHER_RSA_NOPAD OR JC_SUPPORT_CIPHER_RSA_OAEP)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_rsa_cipher.cpp")
endif()
if(JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_rsa_signature.cpp")
endif()
if(JC_SUPPORT_SIGNATURE_ECDSA_SHA OR JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 OR
   JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 OR JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 OR
   JC_SUPPORT_SIGNATURE_ECDSA_SHA_512)
  list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_ecdsa_signature.cpp")
endif()
set(JC_SUPPORT_RSA_KEYS_ENABLED OFF)
foreach(rsa_bits 512 736 768 896 1024 1280 1536 1984 2048 3072 4096)
  foreach(rsa_kind PUBLIC PRIVATE CRT_PRIVATE)
    if(JC_SUPPORT_KEY_RSA_${rsa_kind}_${rsa_bits})
      set(JC_SUPPORT_RSA_KEYS_ENABLED ON)
    endif()
  endforeach()
endforeach()
if(JC_SUPPORT_KEY_RSA_PRIVATE_TRANSIENT_RESET OR
   JC_SUPPORT_KEY_RSA_PRIVATE_TRANSIENT_DESELECT OR
   JC_SUPPORT_KEY_RSA_CRT_PRIVATE_TRANSIENT_RESET OR
   JC_SUPPORT_KEY_RSA_CRT_PRIVATE_TRANSIENT_DESELECT)
  set(JC_SUPPORT_RSA_KEYS_ENABLED ON)
endif()
list(APPEND CHOUPI_VM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src/native_key.cpp")
if(CHOUPI_VM_SOURCES)
  add_library(choupi_vm_core OBJECT ${CHOUPI_VM_SOURCES})
  target_link_libraries(choupi_vm_core PRIVATE pico_mbedtls_headers)
  if(JC_SUPPORT_SECURE_RANDOM)
    target_link_libraries(choupi_vm_core PRIVATE pico_rand_headers)
  endif()
  if(JC_SUPPORT_SHA256)
    target_link_libraries(choupi_vm_core PRIVATE pico_sha256_headers)
  endif()
  target_include_directories(choupi_vm_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/choupi/os/src"
    "${CMAKE_CURRENT_BINARY_DIR}/choupi-generated"
    "${JAVACARD_SUPPORT_GENERATED_DIR}")
  # The upstream CAP structs are packed by format design, and the source has
  # harmless const-by-value and debug-only parameter diagnostics. Keep the
  # root smoke target's strict warning policy while excluding these legacy
  # diagnostics from this compatibility compile.
  target_compile_options(choupi_vm_core PRIVATE
    -Wno-ignored-qualifiers
    -Wno-address-of-packed-member
    -Wno-unused-parameter
    -Wno-array-bounds
    -Wno-format)
  set_target_properties(choupi_vm_core PROPERTIES CXX_STANDARD 17)
endif()

if(PICO_PLATFORM STREQUAL "rp2350-arm-s" AND CHOUPI_VM_SOURCES)
  if(TARGET pico_malloc AND CHOUPI_DEBUG_MALLOC)
    target_compile_definitions(pico_malloc INTERFACE
      PICO_DEBUG_MALLOC=1
      PICO_DEBUG_MALLOC_LOW_WATER=0x20070000)
  endif()
  set(CHOUPI_ROMMASK_IMAGE
    "${CMAKE_CURRENT_SOURCE_DIR}/build/choupi-pc/flash")
  if(NOT EXISTS "${CHOUPI_ROMMASK_IMAGE}")
    message(FATAL_ERROR "Missing CHOUPI ROM-mask image: ${CHOUPI_ROMMASK_IMAGE}")
  endif()
  configure_file(src/choupi_rommask.S.in
    "${CMAKE_CURRENT_BINARY_DIR}/choupi_rommask.S" @ONLY)
  set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/choupi_rommask.S"
    PROPERTIES OBJECT_DEPENDS "${CHOUPI_ROMMASK_IMAGE}")
  add_executable(pico2_choupi
    choupi/src/main.cpp
    src/choupi_pico2.cpp
    src/pico_filesystem.cpp
    src/ccid_device.cpp
    src/reader_led.cpp
    src/cap_load.cpp
    src/cap_store.cpp
    src/scp03.cpp
    "${PICO_SDK_PATH}/lib/mbedtls/library/aes.c"
    "${PICO_SDK_PATH}/lib/mbedtls/library/platform_util.c"
    "${CMAKE_CURRENT_BINARY_DIR}/choupi_rommask.S"
    $<TARGET_OBJECTS:choupi_vm_core>)
  # SCP03 provides the Pico zeroize implementation. Mbed TLS bignum's optional
  # millisecond clock hook is supplied by native_rsa_cipher.cpp when RSA is
  # selected. Keep both alternatives selected in every profile so Pico's
  # platform_util.c never tries to compile unavailable POSIX/time routines.
  target_compile_definitions(pico2_choupi PRIVATE
    MBEDTLS_PLATFORM_MS_TIME_ALT
    MBEDTLS_PLATFORM_ZEROIZE_ALT)
  if(JC_SUPPORT_CIPHER_DES_CBC_NOPAD OR JC_SUPPORT_CIPHER_GETINSTANCE_DES_CBC_NOPAD OR
     JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M1 OR JC_SUPPORT_CIPHER_DES_CBC_ISO9797_M2 OR
     JC_SUPPORT_CIPHER_DES_CBC_PKCS5 OR JC_SUPPORT_CIPHER_DES_ECB_NOPAD OR
     JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M1 OR JC_SUPPORT_CIPHER_DES_ECB_ISO9797_M2 OR
     JC_SUPPORT_CIPHER_DES_ECB_PKCS5 OR JC_SUPPORT_SIGNATURE_DES_MAC8_NOPAD OR
     JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M1 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M1 OR
     JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M2 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M2 OR
     JC_SUPPORT_SIGNATURE_DES_MAC4_PKCS5 OR JC_SUPPORT_SIGNATURE_DES_MAC8_PKCS5 OR
     JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M2_ALG3 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M2_ALG3 OR
     JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M1_ALG3 OR JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M1_ALG3)
    target_sources(pico2_choupi PRIVATE
      "${PICO_SDK_PATH}/lib/mbedtls/library/des.c")
  endif()
  target_include_directories(pico2_choupi PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/choupi/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/choupi/os/src"
    "${CMAKE_CURRENT_BINARY_DIR}/choupi-generated"
    "${JAVACARD_SUPPORT_GENERATED_DIR}")
  target_compile_options(pico2_choupi PRIVATE
    -Wno-ignored-qualifiers
    -Wno-address-of-packed-member
    -Wno-unused-parameter
    -Wno-array-bounds
    -Wno-format)
  # pico_rand also supplies SCP03 host challenge randomness, so it remains a
  # mandatory card-management dependency even when Java Card RandomData is off.
  target_link_libraries(pico2_choupi PRIVATE pico_stdlib pico_rand pico_flash
    hardware_pwm
    pico_mbedtls_headers)
  if(JC_SUPPORT_SHA256)
    target_link_libraries(pico2_choupi PRIVATE pico_sha256)
  endif()
  if(JC_SUPPORT_SHA1 OR JC_SUPPORT_SHA224 OR JC_SUPPORT_SHA256 OR
     JC_SUPPORT_SHA384 OR JC_SUPPORT_SHA512)
    target_sources(pico2_choupi PRIVATE
      "${PICO_SDK_PATH}/lib/mbedtls/library/sha1.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/sha256.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/sha512.c")
  endif()
  if(JC_SUPPORT_CIPHER_RSA_PKCS1 OR JC_SUPPORT_CIPHER_RSA_NOPAD OR JC_SUPPORT_CIPHER_RSA_OAEP OR JC_SUPPORT_SIGNATURE_RSA_SHA_PKCS1_ANY OR
     JC_SUPPORT_SIGNATURE_ECDSA_SHA OR JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 OR
     JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 OR JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 OR
     JC_SUPPORT_SIGNATURE_ECDSA_SHA_512 OR JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH OR
     JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC OR
     JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN OR
     JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC_PLAIN OR
     JC_SUPPORT_KEY_AGREEMENT_EC_PACE_GM OR
     JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN_XY OR
     JC_SUPPORT_KEYPAIR_EC_FP OR JC_SUPPORT_KEYPAIR_RSA_ANY)
    target_sources(pico2_choupi PRIVATE
      "${PICO_SDK_PATH}/lib/mbedtls/library/bignum.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/bignum_core.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/constant_time.c")
  endif()
  if(JC_SUPPORT_SIGNATURE_ECDSA_SHA OR JC_SUPPORT_SIGNATURE_ECDSA_SHA_224 OR
     JC_SUPPORT_SIGNATURE_ECDSA_SHA_256 OR JC_SUPPORT_SIGNATURE_ECDSA_SHA_384 OR
     JC_SUPPORT_SIGNATURE_ECDSA_SHA_512 OR JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH OR
     JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC OR
     JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN OR
     JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DHC_PLAIN OR
     JC_SUPPORT_KEY_AGREEMENT_EC_PACE_GM OR
     JC_SUPPORT_KEY_AGREEMENT_EC_SVDP_DH_PLAIN_XY OR
     JC_SUPPORT_KEYPAIR_EC_FP OR JC_SUPPORT_KEYPAIR_RSA_ANY)
    target_sources(pico2_choupi PRIVATE
      "${PICO_SDK_PATH}/lib/mbedtls/library/ecp.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/ecp_curves.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/ecdsa.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/md.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/md5.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/hmac_drbg.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/asn1parse.c"
      "${PICO_SDK_PATH}/lib/mbedtls/library/asn1write.c")
  endif()
  if(CHOUPI_DEBUG_HEAP_GC)
    target_compile_definitions(choupi_vm_core PRIVATE CHOUPI_DEBUG_HEAP_GC=1)
    target_compile_definitions(pico2_choupi PRIVATE CHOUPI_DEBUG_HEAP_GC=1)
  endif()
  if(CHOUPI_DEBUG_PERSISTENT_GC)
    target_compile_definitions(pico2_choupi PRIVATE
      CHOUPI_DEBUG_PERSISTENT_GC=1)
  endif()
  if(CHOUPI_DEBUG_HEAP_GC_TRACE)
    target_compile_definitions(choupi_vm_core PRIVATE
      CHOUPI_DEBUG_HEAP_GC_TRACE=1)
    target_compile_definitions(pico2_choupi PRIVATE
      CHOUPI_DEBUG_HEAP_GC_TRACE=1)
  endif()
  pico_enable_stdio_usb(pico2_choupi 1)
  # This image owns TinyUSB's polling and CCID driver. Keep tud_task() and
  # endpoint operations in the main loop instead of racing the SDK IRQ worker.
  target_compile_definitions(pico2_choupi PRIVATE
    PICO_STDIO_USB_ENABLE_IRQ_BACKGROUND_TASK=0)
  # Supply a composite CDC + CCID configuration descriptor from the app. Keep
  # the SDK's CDC stdio implementation but omit its CDC-only descriptor object.
  get_target_property(PICO_STDIO_USB_SOURCES pico_stdio_usb INTERFACE_SOURCES)
  list(REMOVE_ITEM PICO_STDIO_USB_SOURCES
    "${PICO_SDK_PATH}/src/rp2_common/pico_stdio_usb/stdio_usb_descriptors.c")
  set_target_properties(pico_stdio_usb PROPERTIES
    INTERFACE_SOURCES "${PICO_STDIO_USB_SOURCES}")
  pico_enable_stdio_uart(pico2_choupi 0)
  pico_add_extra_outputs(pico2_choupi)
endif()
