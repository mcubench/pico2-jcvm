/*
** The MIT License (MIT)
**
** Copyright (c) 2020, National Cybersecurity Agency of France (ANSSI)
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
** Author:
**   - Guillaume Bouffard <guillaume.bouffard@ssi.gouv.fr>
*/

#include "jc_cp.hpp"
#include "../debug.hpp"
#include "jc_class.hpp"
#include "jc_export.hpp"
#include "jc_import.hpp"

#ifndef PC_VERSION
extern "C" void choupi_install_stage(uint8_t stage);
extern "C" void choupi_install_context(uint32_t context);
#endif

namespace jcvm {

/**
 * Get a cp_entry
 *
 * @param[offset] index of the cp_entry
 *
 * @return Get associated Constant Pool offset.
 */
const jc_cap_constant_pool_info
ConstantPool_Handler::getCPEntry(const jc_cp_offset_t offset)
#ifndef JCVM_ARRAY_SIZE_CHECK
    noexcept
#endif /* JCVM_ARRAY_SIZE_CHECK */
{
  if (offset >= 64) {
    TRACE_JCVM_DEBUG("CP lookup index=%u count=%u package=%u",
                     static_cast<unsigned>(offset),
                     static_cast<unsigned>(HTONS(
                         this->package.getCap().getConstantPool()->count)),
                     static_cast<unsigned>(this->package.getPackageID()));
  }
  return this->package.getCap().getConstantPool()->constantpool()[offset];
}

/*
 * Get Class Information (Package & Class indexes) from current constant pool
 * offset.
 *
 * @param[offset] Current constant pool offset
 *
 * @return Class Information (Package & Class indexes) from current constant
 * pool offset.
 */
std::pair<jpackage_ID_t, jclass_index_t>
ConstantPool_Handler::getClassInformation(const jc_cp_offset_t offset)
#ifndef JCVM_ARRAY_SIZE_CHECK
    noexcept
#endif /* JCVM_ARRAY_SIZE_CHECK */
{
  jpackage_ID_t package;
  jclass_index_t claz;

  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);
#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_CLASSREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  auto classref = cp_entry.info.class_ref_info.class_ref;

  if (classref.isExternalClassRef()) {
    Import_Handler importHandler(this->package);
    package = importHandler.getPackageIndexFromOffset(
        classref.external_classref.package_token);

    Package importedPackage(package);
    Export_Handler exportHandler(importedPackage);
    claz = exportHandler.getExportedClassOffset(
        classref.external_classref.class_token);
  } else // is internal classref
  {
    package = this->package.getPackageID();
    claz = classref.internal_classref;
  }

  return std::make_pair(package, HTONS(claz));
}

/*
 * Converting constant pool offset to jc_class_ref
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a jc_class_ref structure.
 */
const jc_cap_class_ref
ConstantPool_Handler::getClassRef(const jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_CLASSREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.class_ref_info.class_ref;
}

/*
 * Convert class index to jc_cap_class_info *
 *
 * @param[claz_index] class index
 * @return associated pointer to class info
 */
const jc_cap_class_info *
ConstantPool_Handler::getClassFromClassIndex(const jclass_index_t claz_index)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_class_info *claz_info = nullptr;
  auto classes = this->getPackage().getCap().getClass()->claz();

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (IS_INTERFACE(&classes[claz_index])) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  claz_info =
      reinterpret_cast<const jc_cap_class_info *>(classes.data() + claz_index);

  return claz_info;
}

/*
 * Convert class index to internal class ref
 *
 * @param[claz_index] class index
 * @return internal class ref
 */
const jc_cap_class_ref ConstantPool_Handler::getClassRefFromClassIndex(
    const jclass_index_t claz_index) {
  // Instance class indexes are CAP Class component byte offsets, not the
  // ordinal position of the class/interface record. Store the offset in CAP
  // byte order so resolveClassref() converts it back to the host integer.
  jc_cap_class_ref classref;
  classref.internal_classref = HTONS(claz_index);
  return classref;
}

/*
 * Converting constant pool offset to struct jc_cap_instance_field_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_instance_field_ref_info structure.
 */
const jc_cap_instance_field_ref_info
ConstantPool_Handler::getInstanceFieldRef(const jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_INSTANCEFIELDREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.instance_field_ref_info;
}

/*
 * Converting constant pool offset to struct jc_cap_virtual_method_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_virtual_method_ref_info structure.
 */
const jc_cap_virtual_method_ref_info
ConstantPool_Handler::getVirtualMethodRef(const jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_VIRTUALMETHODREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.virtual_method_ref_info;
}

/*
 * Converting constant pool offset to struct jc_cap_super_method_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_super_method_ref_info structure.
 */
const jc_cap_super_method_ref_info
ConstantPool_Handler::getSuperMethodRef(const jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_SUPERMETHODREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.super_method_ref_info;
}

/*
 * Converting a union jc_cap_class_ref to struct jc_cap_class_info structure.
 *
 * @param[classref] classref which will be translated.
 * @return a struct jc_cap_class_info structure.
 */
std::pair<Package, const jc_cap_class_info *>
ConstantPool_Handler::classref2class(const jc_cap_class_ref classref)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  auto claz = this->resolveClassref(classref);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (IS_INTERFACE(claz.second)) { // Classref is an interface!
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return std::make_pair(
      claz.first, reinterpret_cast<const jc_cap_class_info *>(claz.second));
}

/*
 * Converting a union jc_cap_class_ref to struct jc_cap_interface_info
 * structure.
 *
 * @param[classref] classref which will be translated.
 * @return a struct jc_cap_interface_info structure.
 */
std::pair<Package, const jc_cap_interface_info *>
ConstantPool_Handler::classref2interface(const jc_cap_class_ref classref)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  auto interface = this->resolveClassref(classref);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (IS_CLASS(interface.second)) { // Classref is a class!
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return std::make_pair(
      interface.first,
      reinterpret_cast<const jc_cap_interface_info *>(interface.second));
}

/*
 * Converting a union jc_cap_class_ref to struct jc_cap_class or struct
 * jc_cap_interface structure.
 *
 * @param[cap] the CAP file used to resolve the classref entry.
 * @param[classref] classref which will be translated.
 * @return a struct jc_cap_class or struct jc_cap_interface structure.
 */
std::pair<Package, const uint8_t *>
ConstantPool_Handler::resolveClassref(const jc_cap_class_ref classref)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
#ifndef PC_VERSION
  choupi_install_stage(11);
#endif
  Package &package = this->package;
  auto cap = this->package.getCap();

  uint16_t class_token;
  const uint8_t *classFound = nullptr;

  // Is it an external class_ref?
  if (classref.isExternalClassRef()) {
    Import_Handler import_handler(package);

    //  High byte is set to one => clear it
    const uint8_t imported_package_token =
        classref.external_classref.package_token & 0x7F;
#ifndef PC_VERSION
    choupi_install_stage(12);
    choupi_install_context(
        (static_cast<uint32_t>(imported_package_token) << 8) |
        classref.external_classref.class_token);
#endif
    auto imported_package_ID =
        import_handler.getPackageAID(imported_package_token);
#ifndef PC_VERSION
    choupi_install_stage(13);
#endif
#ifndef PC_VERSION
    choupi_install_stage(14);
#endif
    auto package_index = import_handler.getPackageIndex(imported_package_ID);
#ifndef PC_VERSION
    choupi_install_stage(15);
    choupi_install_context(
        (static_cast<uint32_t>(package_index) << 16) |
        classref.external_classref.class_token);
#endif

    package = Package(package_index);
    cap = package.getCap();

    class_token = BYTE_TO_SHORT(classref.external_classref.class_token);
#ifndef PC_VERSION
    choupi_install_stage(18);
    choupi_install_context(
        (static_cast<uint32_t>(package_index) << 16) | class_token);
#endif
    Export_Handler export_handler(package);
    class_token = HTONS(export_handler.getExportedClassOffset(class_token));
#ifndef PC_VERSION
    choupi_install_stage(19);
    choupi_install_context(
        (static_cast<uint32_t>(package_index) << 16) | class_token);
#endif
  } else { // is internal class_ref
    class_token = HTONS(classref.internal_classref);
  }

#ifndef PC_VERSION
  choupi_install_stage(16);
  choupi_install_context((static_cast<uint32_t>(package.getPackageID()) << 16) |
                         class_token);
#endif
  classFound = &(cap.getClass()->claz()[class_token]);
#ifndef PC_VERSION
  choupi_install_stage(17);
#endif

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  uint16_t classref_length = sizeof(jc_cap_class_info);

  if (IS_INTERFACE(classFound)) {
    classref_length = sizeof(struct jc_cap_interface_info);
    classref_length +=
        sizeof(union jc_cap_class_ref) *
        (((struct jc_cap_interface_info *)classFound)->interface_count);
  }

  if ((class_token + classref_length) > HTONS(cap.getClass()->size)) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return std::make_pair(package, classFound);
};

/*
 * Converting constant pool offset to struct jc_cap_static_field_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_static_field_ref_info structure.
 */
const jc_cap_static_field_ref_info
ConstantPool_Handler::getStaticFieldRefInfo(jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_STATICFIELDREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.static_field_ref_info;
}

/*
 * Converting constant pool offset to struct jc_cap_static_method_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_static_method_ref_info structure.
 */
const jc_cap_static_method_ref_info
ConstantPool_Handler::getStaticMethodRefInfo(jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_STATICMETHODREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.static_method_ref_info;
}

} // namespace jcvm
