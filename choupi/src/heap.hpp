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

#ifndef _HEAP_HPP
#define _HEAP_HPP

#include "exceptions.hpp"
#include "jc_config.h"
#include "jc_handlers/flashmemory.hpp"
#include "jc_types/jc_array_type.hpp"
#include "jc_types/jc_instance.hpp"
#include "jc_types/jref_t.hpp"
#include "jc_utils.hpp"
#include "jcvm_types/list.hpp"
#include "types.hpp"

#include <memory>
#include <vector>

namespace jcvm {

class JC_Array; // Forward declaration of JC_Array
enum ClearEvent : uint8_t;

class Heap {
public:
  struct StaticReferenceRoot {
    jpackage_ID_t package_id;
    uint16_t field_number;
    jref_t reference;
  };

  struct RuntimeExceptionRoot {
    jpackage_ID_t package_id;
    jclass_index_t class_index;
    jref_t reference;
  };

  struct ReachabilitySummary {
    uint32_t arrays_total = 0;
    uint32_t arrays_reachable = 0;
    uint32_t instances_total = 0;
    uint32_t instances_reachable = 0;
    uint32_t arrays_reclaimed = 0;
    uint32_t instances_reclaimed = 0;
    uint32_t invalid_references = 0;
    uint32_t edge_load_failures = 0;
    bool complete = true;
  };

private:
  ///
  const japplet_ID_t owner;
  /// True only for the runtime-wide heap that owns global arrays.
  const bool global_heap;

  /// List of array in the heap.
  List<std::shared_ptr<JC_Array>> arrays;
  /// List of instance in the heap.
  List<std::shared_ptr<JC_Instance>> instances;

  /// Roots for the ROM and imported class views of the JCRE CryptoException.
  jref_t runtime_crypto_exception_rom;
  jref_t runtime_crypto_exception_imported;
  std::vector<RuntimeExceptionRoot> runtime_exception_roots;
  /// Canonical handles for serialized Java static reference fields.
  std::vector<StaticReferenceRoot> static_reference_roots;

  /// Mark roots and typed children into stable-slot bitmaps.
  ReachabilitySummary traceAndMark(
      const std::vector<jref_t> &roots,
      std::vector<uint8_t> &marked_arrays,
      std::vector<uint8_t> &marked_instances);

  /// Getting field reference from an instance reference.
  // jc_field_t &getFieldFromInstanceRef(jref_t objectref, uint16_t index);

public:
  /// Default constructor.
  Heap(const japplet_ID_t owner, bool global_heap = false) noexcept;

  /// Getting the applet ID owner
  japplet_ID_t getOwner() const;
  bool isGlobalHeap() const noexcept;

  /// Allocate and resolve JCRE-owned temporary global arrays.
  static jref_t addGlobalArray(uint16_t length, jc_array_type type);
  static std::shared_ptr<JC_Array> getGlobalArray(jref_t reference);
  static void clearGlobalArrays(ClearEvent event);

  /// Adding new array in the transient heap.
  jref_t addArray(const uint16_t nb_entry, const jc_array_type type);
  /// Adding new reference array in the transient heap.
  jref_t addArray(const uint16_t nb_entry, const jc_array_type type,
                  const jc_cp_offset_t array_reference_type);
  /// Adding an array in in the transient heap.
  jref_t addArray(JC_Array array);
  /// Clear transient arrays associated with a lifecycle event.
  void clearTransientArrays(ClearEvent event);
  /// Adding new instance in the transient heap.
  jref_t addInstance(const jpackage_ID_t packageID,
                     const jclass_index_t instantiated_class);
  /// Adding an instance in in the transient heap.
  jref_t addInstance(JC_Instance &&instance);
  /// Get the per-context JCRE-owned CryptoException instance.
  jref_t addRuntimeCryptoException(const jpackage_ID_t packageID,
                                   const jclass_index_t classIndex);
  /// Get a per-context JCRE-owned Java runtime exception instance.
  jref_t addRuntimeException(const jpackage_ID_t packageID,
                             const jclass_index_t classIndex);
  /// Look up or update a package/static-field reference cached for this heap.
  bool getStaticReferenceRoot(jpackage_ID_t package_id,
                              uint16_t field_number,
                              jref_t &reference) const noexcept;
  void setStaticReferenceRoot(jpackage_ID_t package_id,
                              uint16_t field_number, jref_t reference);
  /// Append VM-owned roots held by this heap.
  void appendRootReferences(std::vector<jref_t> &references) const;
  /// Append typed object/array children for one live reference.
  void appendChildReferences(const jref_t reference,
                             std::vector<jref_t> &references);
  /// Resolve a heap-owned array pointer back to its stable handle.
  bool getArrayReference(const JC_Array *array, jref_t &reference) const;
  /// Resolve an already-loaded persistent object/array by its flash tag.
  bool getPersistentReference(const fs::Tag &tag,
                              jref_t &reference) const noexcept;
  /// Mark reachable handles from a supplied root set without freeing slots.
  ReachabilitySummary traceReachableFrom(
      const std::vector<jref_t> &roots);
  /// Reclaim unmarked local slots only after a complete mark pass.
  ReachabilitySummary collectUnreachableFrom(
      const std::vector<jref_t> &roots);

  /// Getting array from the transient heap.
  std::shared_ptr<JC_Array> getArray(const jref_t objectref)
#ifndef JCVM_SECURE_HEAP_ACCESS
      noexcept
#endif /* JCVM_SECURE_HEAP_ACCESS */
      ;

  /// Getting instance from the transient heap.
  std::shared_ptr<JC_Instance> getInstance(const jref_t objectref)
#ifndef JCVM_SECURE_HEAP_ACCESS
      noexcept
#endif /* JCVM_SECURE_HEAP_ACCESS */
      ;
};

} // namespace jcvm

#endif /* _HEAP_HPP */
