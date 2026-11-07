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
#include <array>
#include <vector>

namespace jcvm {

class JC_Array; // Forward declaration of JC_Array
class Context;
enum ClearEvent : uint8_t;

class Heap {
public:
  using ReferenceVisitor = void (*)(jref_t, void *);
  using RootReferenceEnumerator = bool (*)(ReferenceVisitor, void *, void *);

  struct StaticReferenceRoot {
    jpackage_ID_t package_id;
    uint16_t field_number;
    jref_t reference;
    uint32_t rollback_generation;
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
    uint32_t root_enumeration_failures = 0;
    uint32_t static_roots = 0;
    uint32_t runtime_roots = 0;
    uint32_t persistent_tagged_arrays = 0;
    uint32_t persistent_tagged_instances = 0;
    bool complete = true;
  };

private:
  ///
  const japplet_ID_t owner;
  /// True only for the runtime-wide heap that owns global arrays.
  const bool global_heap;

  struct SharedStorage {
    List<std::shared_ptr<JC_Array>> arrays;
    List<std::shared_ptr<JC_Instance>> instances;
    // Arrays with at least one volatile reference edge into another package.
    // The package context registry routes these edges into the target heap's
    // root walk before it can reclaim the referenced handle.
    std::vector<JC_Array *> foreign_reference_arrays;
    std::vector<RuntimeExceptionRoot> runtime_exception_roots;
    std::vector<StaticReferenceRoot> static_reference_roots;
  };
  static std::vector<std::weak_ptr<SharedStorage>> &
  foreignReferenceStorageRegistry() noexcept;
  std::shared_ptr<SharedStorage> shared_storage;

  struct GlobalAIDObject {
    std::array<uint8_t, 16> aid{};
    uint8_t aid_length = 0;
    jpackage_ID_t package_id = 0;
    jclass_index_t class_index = 0;
    jref_t reference;
  };

  std::vector<GlobalAIDObject> global_aid_objects;

  /// Roots for the ROM and imported class views of the JCRE CryptoException.
  jref_t runtime_crypto_exception_rom;
  jref_t runtime_crypto_exception_imported;
  /// Mark roots and typed children into stable-slot bitmaps.
  ReachabilitySummary traceAndMark(
      RootReferenceEnumerator enumerate_roots, void *root_context);

  /// Visit children on an already-resolved stable heap slot.
  void visitChildReferences(JC_Array &array,
                            ReferenceVisitor visitor,
                            void *visitor_context);
  void visitChildReferences(const JC_Instance &instance,
                            ReferenceVisitor visitor,
                            void *visitor_context);

  /// Getting field reference from an instance reference.
  // jc_field_t &getFieldFromInstanceRef(jref_t objectref, uint16_t index);

public:
  /// Default constructor.
  Heap(const japplet_ID_t owner, bool global_heap = false);
  /// Create a stable owner facade over another applet heap's shared storage.
  Heap(const japplet_ID_t owner, const Heap &shared_heap);

  /// Getting the applet ID owner
  japplet_ID_t getOwner() const;
  bool isGlobalHeap() const noexcept;
  /// Whether this facade still owns objects retained in the shared handle table.
  bool hasLiveObjects() const noexcept;

  /// Allocate and resolve JCRE-owned temporary global arrays.
  static jref_t addGlobalArray(uint16_t length, jc_array_type type);
  static jref_t addGlobalRetainedArray(uint16_t length,
                                       jc_array_type type);
  static std::shared_ptr<JC_Array> getGlobalArray(jref_t reference);
  static void releaseGlobalArray(jref_t reference) noexcept;
  static uint32_t reclaimTransientGlobalArrays() noexcept;
  static void clearGlobalArrays(ClearEvent event);
  static void visitGlobalArrayReferencesForPackage(
      jpackage_ID_t package_id, ReferenceVisitor visitor,
      void *visitor_context);
  static void clearGlobalArrayReferencesForPackage(
      jpackage_ID_t package_id) noexcept;
  /// Allocate and resolve runtime-wide JCRE Entry Point Object instances.
  static jref_t addGlobalInstance(jpackage_ID_t package_id,
                                  jclass_index_t class_index);
  static std::shared_ptr<JC_Instance> getGlobalInstance(jref_t reference);
  static void releaseGlobalInstance(jref_t reference) noexcept;
  static jref_t findGlobalAIDObject(const uint8_t *aid,
                                    uint8_t aid_length) noexcept;
  static jref_t addGlobalAIDObject(const uint8_t *aid, uint8_t aid_length,
                                   jpackage_ID_t package_id,
                                   jclass_index_t class_index);
  static void releaseGlobalAIDObject(const uint8_t *aid,
                                     uint8_t aid_length) noexcept;
  static bool describeGlobalAIDObject(jref_t reference, uint8_t aid[16],
                                      uint8_t &aid_length,
                                      jpackage_ID_t &package_id,
                                      jclass_index_t &class_index) noexcept;

  /// Adding new array in the transient heap.
  jref_t addArray(const uint16_t nb_entry, const jc_array_type type);
  /// Reject passing a clear event where an array component token is expected.
  jref_t addArray(const uint16_t nb_entry, const jc_array_type type,
                  const ClearEvent event) = delete;
  /// Adding new reference array in the transient heap.
  jref_t addArray(const uint16_t nb_entry, const jc_array_type type,
                  const jc_cp_offset_t array_reference_type);
  /// Allocate a transient primitive array with an explicit clear event.
  jref_t addTransientArray(const uint16_t nb_entry, const jc_array_type type,
                           const ClearEvent event);
  /// Allocate a transient reference array with an explicit component and event.
  jref_t addTransientArray(const uint16_t nb_entry, const jc_array_type type,
                           const jc_cp_offset_t array_reference_type,
                           const ClearEvent event);
  /// Adding an array in in the transient heap.
  jref_t addArray(JC_Array array);
  /// Register an already-created JCSystem transient array with Java exception mapping.
  jref_t addTransientArrayResult(const JC_Array &array);
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
  /// Whether the reference is a VM-rooted JCRE exception instance.
  bool isRuntimeExceptionReference(jref_t reference) const noexcept;
  /// Look up or update a package/static-field reference cached for this heap.
  bool getStaticReferenceRoot(jpackage_ID_t package_id,
                              uint16_t field_number,
                              jref_t &reference) const noexcept;
  /// Reserve a root slot before committing the corresponding persistent field.
  void reserveStaticReferenceRoot(jpackage_ID_t package_id,
                                  uint16_t field_number);
  void setStaticReferenceRoot(jpackage_ID_t package_id,
                              uint16_t field_number, jref_t reference);
  /// Append VM-owned roots held by this heap.
  void appendRootReferences(std::vector<jref_t> &references) const;
  /// Visit VM-owned roots without allocating a temporary reference vector.
  void visitRootReferences(ReferenceVisitor visitor,
                           void *visitor_context) const;
  /// Visit typed object/array children without allocating a temporary buffer.
  void visitChildReferences(const jref_t reference,
                            ReferenceVisitor visitor,
                            void *visitor_context);
  /// Resolve a heap-owned array pointer back to its stable handle.
  bool getArrayReference(const JC_Array *array, jref_t &reference) const;
  /// Resolve an already-loaded persistent object/array by its flash tag.
  bool getPersistentReference(const fs::Tag &tag,
                              jref_t &reference) const noexcept;
  /// Update or remove a volatile array with cross-package reference edges.
  bool registerForeignReferenceArray(JC_Array *array) noexcept;
  void unregisterForeignReferenceArray(JC_Array *array) noexcept;
  /// Route live foreign array entries into the referenced package's GC roots.
  static void visitForeignArrayReferencesForPackage(
      jpackage_ID_t package_id, ReferenceVisitor visitor,
      void *visitor_context);
  /// Clear foreign edges before a package owner ID can be reused.
  static void clearForeignArrayReferencesForPackage(
      jpackage_ID_t package_id) noexcept;
  /// Whether a foreign reference array points to an object allocated by the
  /// given applet instance. Handles are resolved in the target package heap.
  static bool hasForeignArrayReferenceToAppletOwner(
      jpackage_ID_t package_id, japplet_ID_t applet_owner,
      Heap &target_heap) noexcept;
  /// Mark reachable handles from a supplied root set without freeing slots.
  ReachabilitySummary traceReachableFrom(
      const std::vector<jref_t> &roots);
  /// Mark reachable handles from a streamed root set without freeing slots.
  ReachabilitySummary traceReachableFrom(
      RootReferenceEnumerator enumerate_roots, void *root_context);
  /// Test whether a serialized record is the identity or a direct two-byte
  /// field/element record of a currently marked persistent object. Deeper tag
  /// descendants require their own marked object. Call only immediately after
  /// a complete traceReachableFrom() pass.
  bool hasReachablePersistentTagPrefix(const fs::Tag &tag) const noexcept;
  /// Reclaim unmarked local slots only after a complete mark pass.
  ReachabilitySummary collectUnreachableFrom(
      const std::vector<jref_t> &roots);
  /// Reclaim using a streamed root set and no root/worklist buffers.
  ReachabilitySummary collectUnreachableFrom(
      RootReferenceEnumerator enumerate_roots, void *root_context);

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
