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

#ifndef _JC_ARRAY_HPP
#define _JC_ARRAY_HPP

#include "../jc_config.h"
#include "../jc_handlers/flashmemory.hpp"
#include "../jcvm_types/jcvmarray.hpp"
#include "../types.hpp"
#include "jc_array_type.hpp"
#include "jc_object.hpp"
#include "jref_t.hpp"

#include <vector>

#ifdef JCVM_FIREWALL_CHECKS
#include "../context.hpp"
#endif /* JCVM_FIREWALL_CHECKS */

namespace jcvm {

/// Fast forward declation.
class Context;

enum ClearEvent : uint8_t {
  CLEAR_ON_RESET = (uint8_t)1,
  CLEAR_ON_DESELECT = (uint8_t)2,
  None = (uint8_t)0xFF,
};

class JC_Array : public JC_Object {
private:
  /// Is a transient array?
  const bool isTransient;
  /// Array type elements.
  const jc_array_type type;
  /// Logical element count, excluding any serialized tag prefix.
  const uint16_t logical_length;
  /* In case of array of reference, the element type should be saved.
   * cp_offset is a 2-byte offset in the constant pool component.
   */
  const jc_cp_offset_t reference_type;
  /// Data
  JCVMArray<uint8_t> array;
  /// Canonical flash identity once this array is serialized persistently.
  fs::Tag persistent_tag;
  bool has_persistent_tag = false;
  /// When to clear data
  ClearEvent clear;
  /// Applet package owning each non-global reference stored in a JCRE global
  /// Object[]. 0xFF means the slot is empty; 0xFE denotes a JCRE global
  /// instance which may be shared across contexts.
  std::vector<jpackage_ID_t> global_reference_owners;
  /// Provenance for opaque foreign references retained by applet-owned
  /// reference arrays. Empty slots and references local to this package use
  /// 0xFF; global-array ownership continues to use the vector above.
  std::vector<jpackage_ID_t> reference_value_owners;

  ///  Get an entry size from the size type.
  static const uint16_t getEntrySize(const jc_array_type type);
  /// Convert a logical element count to the 16-bit backing byte length.
  static uint16_t checkedBackingLength(const uint16_t size,
                                       const jc_array_type type,
                                       const bool is_transient);

  /// Compute tag value
  fs::Tag computeTag() const noexcept;

public:
  /// Get an entry size from the size type.
  uint16_t getEntrySize() const;

  JC_Array(Heap &owner, const uint16_t size, const jc_array_type type,
           const bool isTransientArray = false,
           const ClearEvent event = ClearEvent::None);
  JC_Array(Heap &owner, const uint16_t size, const jc_array_type type,
           const jc_cp_offset_t reference_type, const bool isTransient = false,
           const ClearEvent event = ClearEvent::None);
  /// Constructor for static array?
  JC_Array(Heap &owner, const jc_array_type type,
           const jc_cp_offset_t reference_type, const fs::Tag &tag,
           const bool isTransientArray,
           const ClearEvent event = ClearEvent::None,
           const uint16_t length = 0);

  /// Default destructor
  ~JC_Array() noexcept;

  /// Get array type
  jc_array_type getType() const noexcept;
  /// Get array Reference type
  jc_cp_offset_t getReferenceType() const
#ifndef JCVM_SECURE_HEAP_ACCESS
      noexcept
#endif /* JCVM_SECURE_HEAP_ACCESS */
      ;
  /// Get array size
  uint16_t size() const;

  /// Fetch a byte or a boolean element from an array.
  jbyte_t getByteEntry(const uint16_t index)
#ifndef JCVM_SECURE_HEAP_ACCESS
      const noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
          ;
  /// Fetch a short element from an array.
  jshort_t getShortEntry(const uint16_t index)
#ifndef JCVM_SECURE_HEAP_ACCESS
      const noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
          ;
#ifdef JCVM_INT_SUPPORTED
  /// Fetch an int element from an array.
  jint_t getIntEntry(const uint16_t index)
#ifndef JCVM_SECURE_HEAP_ACCESS
      const noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
          ;
#endif /* JCVM_INT_SUPPORTED */
  /// Fetch a reference element from an array.
  jref_t getReferenceEntry(const uint16_t index)
#ifndef JCVM_SECURE_HEAP_ACCESS
      noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
          ;
  /// Owner provenance for a reference element; local arrays return 0xff.
  jpackage_ID_t getReferenceEntryOwner(const uint16_t index);
  /// Whether a reference-array entry may be returned to this package.
  bool isReferenceEntryAccessible(const uint16_t index,
                                  jpackage_ID_t package_id);
  /// Visit references retained in this JCRE global Object[] by one package.
  void visitGlobalReferences(jpackage_ID_t package_id,
                             void (*visitor)(jref_t, void *),
                             void *visitor_context);
  /// Drop references owned by a package before its package ID is reused.
  void clearGlobalReferences(jpackage_ID_t package_id) noexcept;

  /// Write a byte or a boolean element from an array.
  void setByteEntry(const uint16_t index, const jbyte_t value)
#ifndef JCVM_SECURE_HEAP_ACCESS
      noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
          ;
  /// Write a byte without adding the write itself to the transaction journal.
  void setByteEntryNonAtomic(const uint16_t index, const jbyte_t value);
  /// Write a short element from an array.
  void setShortEntry(const uint16_t index, const jshort_t value)
#ifndef JCVM_SECURE_HEAP_ACCESS
      noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
          ;
#ifdef JCVM_INT_SUPPORTED
  /// Write an int element from an array.
  void setIntEntry(const uint16_t index, const jint_t value)
#ifndef JCVM_SECURE_HEAP_ACCESS
      noexcept(noexcept(std::declval<JCVMArray<uint8_t> &>()[index]))
#endif /* !JCVM_SECURE_HEAP_ACCESS */
          ;
#endif /* JCVM_INT_SUPPORTED */
  /// Write a reference element from an array.
  void setReferenceEntry(const uint16_t index, const jref_t value,
                         Context &context,
                         jpackage_ID_t value_owner = 0xff);

  /// Get a const pointer to array data
  const uint8_t *getData() const;

  /// Get the backing tag for a persistent array.
  fs::Tag getPersistentTag() const noexcept { return this->computeTag(); }
  /// Assign the persistent tag after first successful serialization.
  void setPersistentTag(const fs::Tag &tag) noexcept;
  /// Whether this array already has a canonical persistent identity.
  bool hasPersistentTag() const noexcept { return this->has_persistent_tag; }

  /// Is a transient array
  const bool isTransientArray() const noexcept;

  /// Get clear event
  const ClearEvent getClearEvent() const noexcept;
  /// Zero the contents of a transient array.
  void clearTransientData();
};

} // namespace jcvm
#endif /* _JC_ARRAY_HPP */
