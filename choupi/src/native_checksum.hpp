#pragma once

#include "javacard-rom-metadata.h"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/package.hpp"

namespace jcvm {

constexpr uint16_t kChecksumNativeFieldCount = 3;

inline jclass_index_t nativeChecksumClass() {
  return HTONS(Export_Handler(Package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY))
                   .getExportedClassOffset(
                       ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_CHECKSUM));
}

} // namespace jcvm
