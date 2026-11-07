#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_class.hpp"
#include "jc_handlers/jc_cp.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/jc_import.hpp"
#include "jc_types/jc_instance.hpp"

#include <cstdint>

namespace jcarray_test_fixture {
bool fail_persistent_reference_read = false;
}

extern "C" int fs_length(const uint8_t *, uint8_t, uint32_t *) { return 0; }
extern "C" uint8_t fs_exists(const uint8_t *, uint8_t) { return 0; }

extern "C" uint8_t choupi_vm_is_selecting_applet(void) { return 0; }

extern "C" uint8_t fs_read_1b_at(const uint8_t *, uint8_t, uint32_t,
                                 uint8_t *) {
  return 1;
}

namespace jcvm {
Package::Package(jpackage_ID_t packageID) noexcept : ID(packageID) {}

JC_Cap Package::getCap() const { return JC_Cap(0, nullptr, false); }

JC_Cap::JC_Cap(uint16_t, const uint8_t *, bool) {}

const jc_cap_import_component *JC_Cap::getImport() const noexcept {
  return nullptr;
}

const jc_cap_package_info *Import_Handler::getPackageAID(const uint8_t) {
  return nullptr;
}

const jpackage_ID_t
Import_Handler::getPackageIndex(const jc_cap_package_info *) {
  return 0;
}

uint8_t remap_imported_class_token(const jc_cap_package_info &,
                                   const uint8_t token) noexcept {
  return token;
}

const jclass_index_t
Export_Handler::getExportedClassOffset(const uint16_t) const {
  return 0;
}

uint32_t FlashMemory_Handler::persistentWriteRollbackGeneration() noexcept {
  return 0;
}

JC_Instance::JC_Instance(Heap &owner, const jpackage_ID_t packageID,
                         const jclass_index_t claz_index)
    : JC_Object(owner, false), packageID(packageID), claz(claz_index),
      fields(new JCVMArray<jc_field_t>(claz_index)) {}

JC_Instance::~JC_Instance() noexcept { delete this->fields; }

bool JC_Instance::hasPersistentTag() const noexcept { return false; }
fs::Tag JC_Instance::getPersistentTag() const noexcept { return {}; }
auto JC_Instance::getNumberOfFields() const noexcept -> decltype(fields->size()) {
  return fields->size();
}
jpackage_ID_t JC_Instance::getPackageID() const noexcept { return 0; }
jclass_index_t JC_Instance::getClassIndex() const noexcept { return 0; }

auto JC_Instance::getFields() const -> decltype(fields) { return fields; }

jbyte_t JC_Instance::getNativeSessionField_Byte(
    const uint16_t index) const noexcept {
  if (!this->native_session_fields ||
      index >= this->native_session_fields->size()) return 0;
  return static_cast<jbyte_t>((*this->native_session_fields)[index].value);
}

jshort_t JC_Instance::getNativeSessionField_Short(
    const uint16_t index) const noexcept {
  if (!this->native_session_fields ||
      index >= this->native_session_fields->size()) return 0;
  return static_cast<jshort_t>((*this->native_session_fields)[index].value);
}

void JC_Instance::setNativeSessionField_Byte(const uint16_t index,
                                              const jbyte_t value) {
  if (!this->native_session_fields) {
    this->native_session_fields =
        std::make_unique<std::vector<jc_field_t>>(this->fields->size());
  }
  jc_field_t &field = this->native_session_fields->at(index);
  field.type = FIELD_TYPE_BYTE;
  field.value = static_cast<jword_t>(BYTE_TO_WORD(value));
}

void JC_Instance::setNativeSessionField_Short(const uint16_t index,
                                               const jshort_t value) {
  if (!this->native_session_fields) {
    this->native_session_fields =
        std::make_unique<std::vector<jc_field_t>>(this->fields->size());
  }
  jc_field_t &field = this->native_session_fields->at(index);
  field.type = FIELD_TYPE_SHORT;
  field.value = static_cast<jword_t>(value);
}

void JC_Instance::clearNativeSessionFields() noexcept {
  this->native_session_fields.reset();
}

bool JC_Instance::hasLoadedFields() const noexcept {
  return !this->isPersistent();
}

void JC_Instance::setField_Reference(const uint16_t index, const jref_t ref) {
  this->fields->at(index).type = ref.isArray() ? FIELD_TYPE_ARRAY_OBJECT
                                                : FIELD_TYPE_OBJECT;
  this->fields->at(index).value = ref.compact();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
fs::Tag FlashMemory_Handler::computeTag(const fs::Tag &tag,
                                        const uint16_t index) {
  fs::Tag result = tag;
  result.value[result.len++] = static_cast<uint8_t>(index >> 8);
  result.value[result.len++] = static_cast<uint8_t>(index);
  return result;
}

FieldType FlashMemory_Handler::getPersistentField_Type(const fs::Tag &) {
  return FieldType::FIELD_TYPE_UNINITIALIZED;
}

jref_t FlashMemory_Handler::getPersistentField_ReferenceStrict(
    const fs::Tag &, Heap &) {
  if (jcarray_test_fixture::fail_persistent_reference_read) {
    throw Exceptions::SecurityException;
  }
  return jref_t{};
}

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

#ifdef JCVM_INT_SUPPORTED
const jint_t FlashMemory_Handler::getPersistentField_Array_Int(
    const fs::Tag &, const uint16_t, Heap &) {
  return 0;
}

void FlashMemory_Handler::setPersistentField_Array_Int(
    const fs::Tag &, const uint16_t, const jint_t, Heap &) {}
#endif

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
