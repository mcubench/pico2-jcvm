#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_class.hpp"
#include "jc_handlers/jc_cp.hpp"
#include "jc_types/jc_instance.hpp"

#include <cstdint>

extern "C" int fs_length(const uint8_t *, uint8_t, uint32_t *) { return 0; }

extern "C" uint8_t choupi_vm_is_selecting_applet(void) { return 0; }

extern "C" uint8_t fs_read_1b_at(const uint8_t *, uint8_t, uint32_t,
                                 uint8_t *) {
  return 1;
}

namespace jcvm {
Package::Package(jpackage_ID_t packageID) noexcept : ID(packageID) {}

bool JC_Instance::hasPersistentTag() const noexcept { return false; }
jpackage_ID_t JC_Instance::getPackageID() const noexcept { return 0; }
jclass_index_t JC_Instance::getClassIndex() const noexcept { return 0; }

auto JC_Instance::getFields() const -> decltype(fields) { return fields; }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
const jbyte_t FlashMemory_Handler::getPersistentField_Array_Byte(
    const fs::Tag &, const uint16_t, Heap &) {
  return 0;
}

void FlashMemory_Handler::setPersistentField_Array_Byte(
    const fs::Tag &, const uint16_t, const jbyte_t, Heap &) {}

const jshort_t FlashMemory_Handler::getPersistentField_Array_Short(
    const fs::Tag &, const uint16_t, Heap &) {
  return 0;
}

void FlashMemory_Handler::setPersistentField_Array_Short(
    const fs::Tag &, const uint16_t, const jshort_t, Heap &) {}

const jint_t FlashMemory_Handler::getPersistentField_Array_Int(
    const fs::Tag &, const uint16_t, Heap &) {
  return 0;
}

void FlashMemory_Handler::setPersistentField_Array_Int(
    const fs::Tag &, const uint16_t, const jint_t, Heap &) {}

const jref_t FlashMemory_Handler::getPersistentField_Array_Reference(
    const fs::Tag &, const uint16_t, Heap &) {
  return jref_t{};
}

void FlashMemory_Handler::setPersistentField_Array_Reference(
    const fs::Tag &, const uint16_t, const jref_t, Heap &) {}

std::pair<Package, const uint8_t *>
ConstantPool_Handler::resolveClassref(const jc_cap_class_ref, const bool) {
  return {Package(0), nullptr};
}

const jc_cap_class_ref
ConstantPool_Handler::getClassRef(const jc_cp_offset_t) {
  return {};
}

const jc_cap_class_info *
ConstantPool_Handler::getClassFromClassIndex(const jclass_index_t) {
  return nullptr;
}

std::pair<Package, const jc_cap_class_info *>
ConstantPool_Handler::classref2class(const jc_cap_class_ref) {
  return {Package(0), nullptr};
}

jbool_t Class_Handler::docheckcast(
    const std::pair<Package, const uint8_t *>,
    const std::pair<Package, const uint8_t *>) {
  return TRUE;
}
#pragma GCC diagnostic pop
} // namespace jcvm
