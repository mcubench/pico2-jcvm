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

#ifndef _CONTEXT_HPP
#define _CONTEXT_HPP

#include "heap.hpp"
#include "jc_config.h"
#include "jc_handlers/package.hpp"
#include "jc_types/jref_t.hpp"
#include "jcvm_types/list.hpp"
#include "stack.hpp"
#include "types.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

namespace jcvm {

class JC_Array;

class Context {
private:
  enum class SensitiveResultKind : uint8_t { Unassigned, Boolean, Short, Object };
  struct RemoteObjectEntry {
    jref_t reference;
    uint16_t object_id = 0xffff;
    bool exported = false;
    bool session_reference = false;
  };
  struct ShareableProxyEntry {
    jref_t proxy_reference;
    jref_t server_reference;
    std::array<uint8_t, 16> server_aid{};
    uint8_t server_aid_length = 0;
    jpackage_ID_t server_package_id = 0;
  };
  /// State owned by one installed applet instance. Kept separate from the
  /// CAP execution context so one context can later retain sibling AIDs,
  /// lifecycle roots, and registration state without merging ownership.
  struct AppletInstanceState {
    japplet_ID_t owner = 0;
    std::array<uint8_t, 16> aid{};
    uint8_t aid_length = 0;
    jref_t aid_object_ref;
    bool registered = false;
    bool installation_active = false;
    jref_t registered_applet_ref;
    bool restored = false;
    jref_t restored_applet_ref;
    bool active = false;
    std::shared_ptr<Heap> owner_heap;
    // RMI identities and SIO routing belong to the applet instance. Siblings
    // share a firewall Context and Heap storage, not these registries.
    // Keep headroom beyond Java Card's 16-object minimum for the applet's
    // already-exported initial reference while it exercises the limit.
    std::array<RemoteObjectEntry, 32> remote_objects{};
    std::array<ShareableProxyEntry, 32> shareable_proxies{};
    std::array<jref_t, 32> shareable_object_roots{};
    uint16_t next_remote_object_id = 1;
  };
  std::vector<AppletInstanceState> applet_instances;
  std::size_t active_applet_index = 0;
  /// Package containing the active applet instance.
  jpackage_ID_t applet_package_ID;
  /// Applet AID active immediately before the last context switch.
  std::array<uint8_t, 16> previous_context_aid{};
  uint8_t previous_context_aid_length = 0;
  jref_t apdu_buffer_ref;
  std::shared_ptr<JC_Array> apdu_java_buffer;
  jref_t apdu_object_ref;
  bool apdu_process_active = false;
  jref_t apdu_outgoing_buffer_ref;
  std::shared_ptr<JC_Array> apdu_outgoing_buffer;
  uint16_t apdu_incoming_length = 0;
  uint16_t apdu_expected_response_length = 0;
  bool apdu_has_expected_response_length = false;
  uint16_t apdu_response_declared_length = 0;
  uint16_t apdu_response_sent_length = 0;
  uint8_t apdu_state = 0;
  bool apdu_outgoing_no_chaining = false;
  /// Reason code for the next VM-raised SystemException.
  uint16_t pending_system_exception_reason = 1;
  /// Reason code for the next VM-raised CryptoException.
  uint16_t pending_crypto_exception_reason = 1;
  /// Reason code for the next VM-raised TransactionException.
  uint16_t pending_transaction_exception_reason = 4;
  // SensitiveResult is VM-wide and survives Java Card context switches.
  static SensitiveResultKind sensitive_result_kind;
  static bool sensitive_result_boolean;
  static jshort_t sensitive_result_short;
  static Stack::ReferenceValue sensitive_result_object;
  std::array<uint8_t, 1022> apdu_response_data{};
  uint16_t apdu_outgoing_offset = 0;
  uint16_t apdu_outgoing_length = 0;
  /// Current Java Card stack.
  Stack stack;
  /// Linked-list where the executed Packages ID is stored. The Head of the
  /// linked-list is the current executed package ID.
  List<jpackage_ID_t> packagesID;
  /// Context's heap
  Heap heap;

  AppletInstanceState &activeAppletInstance() noexcept;
  const AppletInstanceState &activeAppletInstance() const noexcept;
  void pruneForgottenAppletInstances() noexcept;

public:
  /// Set the active VM context and return the context previously active.
  static Context *setActiveContext(Context *context) noexcept;
  /// Get the context used by context-free native method implementations.
  static Context &getActiveContext();

  /// Resolve a retained package context without consulting another heap's
  /// compact handle table. Returns null when the package is unavailable or
  /// has multiple independent retained contexts.
  static Context *findRetainedPackageContext(jpackage_ID_t package_id) noexcept;
  /// Default constructor
  Context(const uint8_t appletID, const jpackage_ID_t packageID);
  ~Context() noexcept;
  /// Select or create per-applet lifecycle state held by this CAP context.
  bool activateAppletInstance(const japplet_ID_t owner, const uint8_t *aid,
                              uint8_t aid_length) noexcept;
  bool hasAppletInstance(const japplet_ID_t owner, const uint8_t *aid,
                         uint8_t aid_length) const noexcept;
  /// Forget one deleted applet while retaining its owner facade for heap objects.
  bool forgetAppletInstance(const uint8_t *aid,
                            uint8_t aid_length) noexcept;
  /// Retain a deleted owner's identity while its facade still owns live objects.
  bool isAppletOwnerRetained(japplet_ID_t owner) noexcept;
  /// Get the context ID.
  jpackage_ID_t getCurrentPackageID() noexcept;
  /// Get applet ID
  japplet_ID_t getAppletID() const noexcept;
  jpackage_ID_t getAppletPackageID() const noexcept;
  bool setAppletAID(const uint8_t *aid, uint8_t aid_length) noexcept;
  bool copyAppletAID(uint8_t aid[16], uint8_t &aid_length) const noexcept;
  bool copyPreviousContextAID(uint8_t aid[16],
                              uint8_t &aid_length) const noexcept;
  jref_t getAppletAIDObjectRef() const noexcept;
  void setAppletAIDObjectRef(jref_t reference) noexcept;
  /// Record and query Applet.register() calls during installation.
  void markAppletRegistered(jref_t applet_ref) noexcept;
  bool wasAppletRegistered() const noexcept;
  jref_t getRegisteredAppletRef() const noexcept;
  void setAppletInstallationActive(bool active) noexcept;
  bool isAppletInstallationActive() const noexcept;
  /// Restore the persisted applet root and eagerly decode its field graph.
  bool restoreAppletInstance(jpackage_ID_t package_id,
                             jclass_index_t class_index) noexcept;
  bool hasRestoredAppletInstance() const noexcept;
  jref_t getRestoredAppletRef() const noexcept;
  void setAppletActive(bool active) noexcept;
  bool isAppletActive() const noexcept;
  /// Invalidate runtime Shareable routes and server roots on card reset.
  void clearShareableSessionState() noexcept;
  /// Track exported Java Card RMI objects in the applet context and as GC roots.
  uint16_t exportRemoteObject(jref_t reference);
  void unexportRemoteObject(jref_t reference) noexcept;
  bool retainRemoteObjectForSession(jref_t reference) noexcept;
  uint16_t remoteObjectId(jref_t reference) const noexcept;
  bool remoteObject(uint16_t object_id, jref_t &reference) const noexcept;
  jref_t findShareableProxy(const uint8_t *server_aid,
                            uint8_t server_aid_length,
                            jref_t server_reference) const noexcept;
  bool isShareableProxy(jref_t proxy_reference) const noexcept;
  /// Detect a restored Shareable placeholder whose runtime routing metadata
  /// is unavailable; never treat it as an ordinary local object.
  bool isStaleShareableProxy(jref_t reference) noexcept;
  bool registerShareableProxy(jref_t proxy_reference,
                             const uint8_t *server_aid,
                             uint8_t server_aid_length,
                             jref_t server_reference,
                             jpackage_ID_t server_package_id) noexcept;
  void forgetShareableProxiesForPackage(
      jpackage_ID_t server_package_id) noexcept;
  bool getShareableProxy(jref_t proxy_reference, uint8_t server_aid[16],
                         uint8_t &server_aid_length,
                         jref_t &server_reference) const noexcept;
  bool retainShareableObjectRoot(jref_t reference) noexcept;
  /// Set and query the APDU buffer and response window for the current command.
  void setAPDUBufferRef(jref_t reference, uint16_t incoming_length,
                        uint16_t expected_response_length,
                        bool has_expected_response_length) noexcept;
  void setAPDUJavaBuffer(std::shared_ptr<JC_Array> buffer);
  std::shared_ptr<JC_Array> getAPDUJavaBuffer() const noexcept;
  jref_t getAPDUBufferRef() const noexcept;
  void setAPDUObjectRef(jref_t reference) noexcept;
  jref_t getAPDUObjectRef() const noexcept;
  void setAPDUProcessActive(bool active) noexcept;
  bool isAPDUProcessActive() const noexcept;
  uint16_t getAPDUIncomingLength() const noexcept;
  uint16_t getAPDUExpectedResponseLength() const noexcept;
  uint16_t beginAPDUOutgoing(bool no_chaining);
  void setAPDUOutgoingLength(uint16_t length);
  uint8_t getAPDUState() const noexcept;
  bool isAPDUOutgoingNoChaining() const noexcept;
  void markAPDUIncomingReceived();
  void setAPDUOutgoing(std::shared_ptr<JC_Array> buffer, uint16_t offset,
                       uint16_t length);
  uint8_t getAPDUOutgoingByte(uint16_t index) const;
  std::shared_ptr<JC_Array> getAPDUOutgoingBuffer() const noexcept;
  jref_t getAPDUOutgoingBufferRef() const noexcept;
  uint16_t getAPDUOutgoingOffset() const noexcept;
  uint16_t getAPDUOutgoingLength() const noexcept;
  void resetSensitiveResult() noexcept;
  void setSensitiveResultBoolean(bool value) noexcept;
  void setSensitiveResultShort(jshort_t value) noexcept;
  void setSensitiveResultObject(jref_t value) noexcept;
  void assertSensitiveResultObject(jref_t value) const;
  void assertSensitiveResultBoolean(bool value) const;
  void assertSensitiveResultShort(jshort_t value, uint8_t comparison) const;
  void setPendingSystemExceptionReason(uint16_t reason) noexcept;
  uint16_t consumePendingSystemExceptionReason() noexcept;
  void setPendingCryptoExceptionReason(uint16_t reason) noexcept;
  uint16_t consumePendingCryptoExceptionReason() noexcept;
  void setPendingTransactionExceptionReason(uint16_t reason) noexcept;
  uint16_t consumePendingTransactionExceptionReason() noexcept;
  uint16_t pendingSystemExceptionReason() const noexcept {
    return pending_system_exception_reason;
  }
  uint16_t pendingTransactionExceptionReason() const noexcept {
    return pending_transaction_exception_reason;
  }
  /// Append context and heap-owned Java references for root tracing.
  void appendRootReferences(std::vector<jref_t> &references) const;
  /// Visit context and heap-owned references without a temporary root vector.
  void visitRootReferences(Heap::ReferenceVisitor visitor,
                           void *visitor_context) const;
  /// Get context Java Card stack.
  Stack &getStack() noexcept;
  /// Get context heap.
  Heap &getHeap() noexcept;
  const Heap &getHeap() const noexcept;
  /// Get current current package.
  Package getCurrentPackage();
  /// Shift context package ID
  void changePackageID(const uint8_t packageID) noexcept;
  /// Back to the previous package ID.
  void backToPreviousPackageID() noexcept;
};

} // namespace jcvm

#endif /* _CONTEXT_HPP */
