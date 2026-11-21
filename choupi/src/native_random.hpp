#pragma once

#include "javacard-rom-metadata.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/package.hpp"
#include "jc_types/jc_array.hpp"

namespace jcvm {

constexpr uint16_t kRandomNativeFieldCount = 2;

inline jclass_index_t nativeRandomClass() {
  return HTONS(Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
                   .getExportedClassOffset(
                       ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_RANDOMDATA));
}

jref_t javacard_security_RandomData_getInstance(jbyte_t algorithm);
jbyte_t javacard_security_RandomData_getAlgorithm(jref_t receiver);
jshort_t javacard_security_RandomData_nextBytes(
    jref_t receiver, std::shared_ptr<JC_Array> output, jshort_t offset,
    jshort_t length);
void javacard_security_RandomData_setSeed(
    jref_t receiver, std::shared_ptr<JC_Array> input, jshort_t offset,
    jshort_t length);

} // namespace jcvm
