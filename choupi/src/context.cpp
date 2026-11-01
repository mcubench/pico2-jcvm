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

#include "context.hpp"
#include "exceptions.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_types/jc_array.hpp"
#include "../../src/vm_diagnostics.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace jcvm {

namespace {
Context *active_context = nullptr;
}

Context::SensitiveResultKind Context::sensitive_result_kind =
    Context::SensitiveResultKind::Unassigned;
bool Context::sensitive_result_boolean = false;
jshort_t Context::sensitive_result_short = 0;
jref_t Context::sensitive_result_object;

Context *Context::setActiveContext(Context *context) noexcept {
  Context *previous = active_context;
  if (context != nullptr && context != previous) {
    context->previous_context_aid.fill(0);
    context->previous_context_aid_length = 0;
    if (previous != nullptr && previous->applet_aid_length >= 5 &&
        previous->applet_aid_length <= previous->applet_aid.size()) {
      context->previous_context_aid = previous->applet_aid;
      context->previous_context_aid_length = previous->applet_aid_length;
    }
  }
  active_context = context;
  return previous;
}

Context &Context::getActiveContext() {
  if (active_context == nullptr) {
    throw Exceptions::RuntimeException;
  }
  return *active_context;
}

/**
 * Default constructor
 *
 * @param[packageID] Starting package ID.
 */
Context::Context(const japplet_ID_t appletID,
                 const jpackage_ID_t packageID) noexcept
    : applet_ID(appletID), applet_package_ID(packageID), stack(), packagesID(),
      heap(appletID) {
  this->packagesID.push_front(packageID);
}

/**
 * Get current context ID.
 *
 * @return current context ID
 */
jpackage_ID_t Context::getCurrentPackageID() noexcept {
  return *(this->packagesID.cbegin());
}

/**
 * Get applet class ID.
 *
 * @return current applet class ID
 */
jpackage_ID_t Context::getAppletID() const noexcept { return this->applet_ID; }

jpackage_ID_t Context::getAppletPackageID() const noexcept {
  return this->applet_package_ID;
}

bool Context::setAppletAID(const uint8_t *aid,
                           const uint8_t aid_length) noexcept {
  this->applet_aid.fill(0);
  this->applet_aid_length = 0;
  this->applet_aid_object_ref = jref_t();
  if (aid == nullptr || aid_length < 5 || aid_length > this->applet_aid.size()) {
    return false;
  }
  std::copy(aid, aid + aid_length, this->applet_aid.begin());
  this->applet_aid_length = aid_length;
  return true;
}

bool Context::copyAppletAID(uint8_t aid[16],
                            uint8_t &aid_length) const noexcept {
  aid_length = this->applet_aid_length;
  if (aid == nullptr || aid_length < 5 || aid_length > this->applet_aid.size()) {
    aid_length = 0;
    return false;
  }
  std::copy_n(this->applet_aid.begin(), aid_length, aid);
  return true;
}

bool Context::copyPreviousContextAID(uint8_t aid[16],
                                     uint8_t &aid_length) const noexcept {
  aid_length = this->previous_context_aid_length;
  if (aid == nullptr || aid_length < 5 ||
      aid_length > this->previous_context_aid.size()) {
    aid_length = 0;
    return false;
  }
  std::copy_n(this->previous_context_aid.begin(), aid_length, aid);
  return true;
}

jref_t Context::getAppletAIDObjectRef() const noexcept {
  return this->applet_aid_object_ref;
}

void Context::setAppletAIDObjectRef(const jref_t reference) noexcept {
  this->applet_aid_object_ref = reference;
}

void Context::markAppletRegistered(const jref_t applet_ref) noexcept {
  this->registered_applet_ref = applet_ref;
  this->applet_registered = true;
}

bool Context::wasAppletRegistered() const noexcept {
  return this->applet_registered;
}

jref_t Context::getRegisteredAppletRef() const noexcept {
  return this->registered_applet_ref;
}

void Context::setAppletInstallationActive(const bool active) noexcept {
  this->applet_installation_active = active;
}

bool Context::isAppletInstallationActive() const noexcept {
  return this->applet_installation_active;
}

bool Context::restoreAppletInstance(const jpackage_ID_t package_id,
                                   const jclass_index_t class_index) noexcept {
  this->applet_instance_restored = false;
  this->restored_applet_ref = jref_t();
  try {
    const fs::Tag tag = FlashMemory_Handler::getAppletInstanceTag(
        this->applet_ID, package_id, class_index);
    const jref_t reference =
        FlashMemory_Handler::getPersistentField_Reference(tag, this->heap);
    if (reference.isNullPointer() || reference.isArray()) {
      std::printf("CHOUPI:RESTORE_REF null=%u array=%u package=%u class=%04X\r\n",
                  reference.isNullPointer() ? 1u : 0u,
                  reference.isArray() ? 1u : 0u,
                  static_cast<unsigned>(package_id),
                  static_cast<unsigned>(class_index));
      std::fflush(stdout);
      return false;
    }
    const auto instance = this->heap.getInstance(reference);
    if (instance->getPackageID() != package_id ||
        instance->getClassIndex() != class_index) {
      std::printf("CHOUPI:RESTORE_MISMATCH got=%u/%04X want=%u/%04X\r\n",
                  static_cast<unsigned>(instance->getPackageID()),
                  static_cast<unsigned>(instance->getClassIndex()),
                  static_cast<unsigned>(package_id),
                  static_cast<unsigned>(class_index));
      std::fflush(stdout);
      return false;
    }
    (void)instance->getFields();
    this->restored_applet_ref = reference;
    this->applet_instance_restored = true;
    return true;
  } catch (Exceptions exception) {
    std::printf("CHOUPI:RESTORE_EXCEPTION id=%u\r\n",
                static_cast<unsigned>(exception));
#ifndef PC_VERSION
    uint8_t recorded = 0;
    uint16_t failed_package = 0;
    uint16_t failed_class = 0;
    uint16_t failed_field = 0;
    uint8_t failed_type = 0;
    uint8_t failed_stage = 0;
    uint8_t failed_exception = 0;
    choupi_vm_persistent_restore_failure_state(
        &recorded, &failed_package, &failed_class, &failed_field,
        &failed_type, &failed_stage, &failed_exception);
    if (recorded != 0) {
      std::printf("CHOUPI:RESTORE_FIELD_FAIL package=%u class=%04X field=%u "
                  "type=%u stage=%u exception=%u\r\n",
                  static_cast<unsigned>(failed_package),
                  static_cast<unsigned>(failed_class),
                  static_cast<unsigned>(failed_field),
                  static_cast<unsigned>(failed_type),
                  static_cast<unsigned>(failed_stage),
                  static_cast<unsigned>(failed_exception));
    }
#endif
    std::fflush(stdout);
    return false;
  } catch (...) {
    std::printf("CHOUPI:RESTORE_EXCEPTION id=unknown\r\n");
    std::fflush(stdout);
    return false;
  }
}

bool Context::hasRestoredAppletInstance() const noexcept {
  return this->applet_instance_restored;
}

jref_t Context::getRestoredAppletRef() const noexcept {
  return this->restored_applet_ref;
}

void Context::setAppletActive(const bool active) noexcept {
  this->applet_active = active;
  if (active) {
    return;
  }
  for (auto &entry : this->remote_objects) {
    entry.session_reference = false;
    if (!entry.exported) {
      entry.reference = jref_t();
      entry.object_id = 0xffff;
    }
  }
}

bool Context::isAppletActive() const noexcept {
  return this->applet_active;
}

uint16_t Context::exportRemoteObject(const jref_t reference) {
  for (auto &entry : this->remote_objects) {
    if (entry.object_id != 0xffff && entry.reference == reference) {
      entry.exported = true;
      return entry.object_id;
    }
  }

  RemoteObjectEntry *free_entry = nullptr;
  for (auto &entry : this->remote_objects) {
    if (entry.object_id == 0xffff) {
      free_entry = &entry;
      break;
    }
  }
  if (free_entry == nullptr) {
    return 0xffff;
  }

  for (uint32_t attempt = 0; attempt < 0xffffu; ++attempt) {
    const uint16_t candidate = this->next_remote_object_id++;
    if (this->next_remote_object_id == 0xffff) {
      this->next_remote_object_id = 1;
    }
    bool in_use = false;
    for (const auto &entry : this->remote_objects) {
      if (entry.object_id == candidate) {
        in_use = true;
        break;
      }
    }
    if (!in_use && candidate != 0xffff) {
      free_entry->reference = reference;
      free_entry->object_id = candidate;
      free_entry->exported = true;
      return candidate;
    }
  }
  return 0xffff;
}

void Context::unexportRemoteObject(const jref_t reference) noexcept {
  for (auto &entry : this->remote_objects) {
    if (entry.object_id != 0xffff && entry.reference == reference) {
      entry.exported = false;
      if (!this->applet_active || !entry.session_reference) {
        entry.reference = jref_t();
        entry.object_id = 0xffff;
        entry.session_reference = false;
      }
      return;
    }
  }
}

bool Context::retainRemoteObjectForSession(const jref_t reference) noexcept {
  for (auto &entry : this->remote_objects) {
    if (entry.object_id != 0xffff && entry.reference == reference &&
        entry.exported) {
      entry.session_reference = true;
      return true;
    }
  }
  return false;
}

uint16_t Context::remoteObjectId(const jref_t reference) const noexcept {
  for (const auto &entry : this->remote_objects) {
    if (entry.object_id != 0xffff && entry.reference == reference &&
        entry.exported) {
      return entry.object_id;
    }
  }
  return 0xffff;
}

bool Context::remoteObject(const uint16_t object_id,
                           jref_t &reference) const noexcept {
  if (object_id == 0xffff) {
    return false;
  }
  for (const auto &entry : this->remote_objects) {
    if (entry.object_id == object_id &&
        (entry.exported ||
         (this->applet_active && entry.session_reference))) {
      reference = entry.reference;
      return true;
    }
  }
  return false;
}

jref_t Context::findShareableProxy(const uint8_t *server_aid,
                                   const uint8_t server_aid_length,
                                   const jref_t server_reference) const noexcept {
  if (server_aid == nullptr || server_aid_length < 5 ||
      server_aid_length > 16 || server_reference.isNullPointer() ||
      server_reference.isArray()) {
    return jref_t{};
  }
  for (const auto &entry : this->shareable_proxies) {
    if (entry.proxy_reference.isNullPointer() ||
        entry.server_aid_length != server_aid_length ||
        entry.server_reference != server_reference) {
      continue;
    }
    if (std::equal(server_aid, server_aid + server_aid_length,
                   entry.server_aid.begin())) {
      return entry.proxy_reference;
    }
  }
  return jref_t{};
}

bool Context::registerShareableProxy(const jref_t proxy_reference,
                                     const uint8_t *server_aid,
                                     const uint8_t server_aid_length,
                                     const jref_t server_reference) noexcept {
  if (proxy_reference.isNullPointer() || proxy_reference.isArray() ||
      server_aid == nullptr || server_aid_length < 5 || server_aid_length > 16 ||
      server_reference.isNullPointer() || server_reference.isArray()) {
    return false;
  }
  for (auto &entry : this->shareable_proxies) {
    if (!entry.proxy_reference.isNullPointer()) {
      if (entry.proxy_reference == proxy_reference) {
        return entry.server_reference == server_reference &&
               entry.server_aid_length == server_aid_length &&
               std::equal(server_aid, server_aid + server_aid_length,
                          entry.server_aid.begin());
      }
      continue;
    }
    entry.proxy_reference = proxy_reference;
    entry.server_reference = server_reference;
    std::copy(server_aid, server_aid + server_aid_length,
              entry.server_aid.begin());
    entry.server_aid_length = server_aid_length;
    return true;
  }
  return false;
}

bool Context::getShareableProxy(const jref_t proxy_reference,
                                uint8_t server_aid[16],
                                uint8_t &server_aid_length,
                                jref_t &server_reference) const noexcept {
  server_aid_length = 0;
  server_reference = jref_t{};
  if (proxy_reference.isNullPointer() || proxy_reference.isArray() ||
      server_aid == nullptr) {
    return false;
  }
  for (const auto &entry : this->shareable_proxies) {
    if (entry.proxy_reference != proxy_reference ||
        entry.server_aid_length < 5 || entry.server_aid_length > 16 ||
        entry.server_reference.isNullPointer() ||
        entry.server_reference.isArray()) {
      continue;
    }
    std::copy_n(entry.server_aid.begin(), entry.server_aid_length, server_aid);
    server_aid_length = entry.server_aid_length;
    server_reference = entry.server_reference;
    return true;
  }
  return false;
}

bool Context::retainShareableObjectRoot(const jref_t reference) noexcept {
  if (reference.isNullPointer() || reference.isArray() ||
      reference.isGlobalInstance()) {
    return false;
  }
  for (auto &existing : this->shareable_object_roots) {
    if (existing == reference) return true;
    if (existing.isNullPointer()) {
      existing = reference;
      return true;
    }
  }
  return false;
}

void Context::setAPDUBufferRef(const jref_t reference,
                               const uint16_t incoming_length,
                               const uint16_t expected_response_length,
                               const bool has_expected_response_length) noexcept {
  this->apdu_buffer_ref = reference;
  this->apdu_incoming_length = incoming_length;
  this->apdu_expected_response_length = expected_response_length;
  this->apdu_has_expected_response_length = has_expected_response_length;
  this->apdu_response_declared_length = 0;
  this->apdu_response_sent_length = 0;
  this->apdu_state = 0;
  this->apdu_outgoing_no_chaining = false;
  this->apdu_response_data.fill(0);
  this->apdu_outgoing_offset = 0;
  this->apdu_outgoing_length = 0;
  this->apdu_outgoing_buffer_ref = reference;
  this->apdu_outgoing_buffer.reset();
}

void Context::setAPDUJavaBuffer(std::shared_ptr<JC_Array> buffer) {
  if (!buffer || buffer->getType() != JAVA_ARRAY_T_BYTE || buffer->size() < 5) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  this->apdu_java_buffer = std::move(buffer);
}

std::shared_ptr<JC_Array> Context::getAPDUJavaBuffer() const noexcept {
  return this->apdu_java_buffer;
}

jref_t Context::getAPDUBufferRef() const noexcept {
  return this->apdu_buffer_ref;
}

void Context::setAPDUObjectRef(const jref_t reference) noexcept {
  this->apdu_object_ref = reference;
}

jref_t Context::getAPDUObjectRef() const noexcept {
  return this->apdu_object_ref;
}

void Context::setAPDUProcessActive(const bool active) noexcept {
  this->apdu_process_active = active;
}

bool Context::isAPDUProcessActive() const noexcept {
  return this->apdu_process_active;
}

uint16_t Context::getAPDUIncomingLength() const noexcept {
  return this->apdu_incoming_length;
}

uint16_t Context::getAPDUExpectedResponseLength() const noexcept {
  return this->apdu_has_expected_response_length
             ? this->apdu_expected_response_length
             : 0;
}

uint16_t Context::beginAPDUOutgoing(const bool no_chaining) {
  if (this->apdu_state != 0 && this->apdu_state != 1 &&
      this->apdu_state != 2) {
    throw Exceptions::APDUException;
  }
  if (!this->apdu_has_expected_response_length) {
    throw Exceptions::APDUException;
  }
  this->apdu_outgoing_no_chaining = no_chaining;
  this->apdu_state = 3;
  return this->apdu_expected_response_length;
}

void Context::setAPDUOutgoingLength(const uint16_t length) {
  if (this->apdu_state != 3) {
    throw Exceptions::APDUException;
  }
  if (length > this->apdu_response_data.size() ||
      (this->apdu_outgoing_no_chaining &&
       (length > 252 || length > this->apdu_expected_response_length))) {
    throw Exceptions::APDUException;
  }
  this->apdu_response_declared_length = length;
  this->apdu_response_sent_length = 0;
  this->apdu_outgoing_length = 0;
  this->apdu_outgoing_offset = 0;
  this->apdu_state = 4;
}

uint8_t Context::getAPDUState() const noexcept { return this->apdu_state; }

bool Context::isAPDUOutgoingNoChaining() const noexcept {
  return this->apdu_outgoing_no_chaining;
}

void Context::markAPDUIncomingReceived() {
  if (this->apdu_state != 0) {
    throw Exceptions::APDUException;
  }
  this->apdu_state = 2;
}

void Context::setAPDUOutgoing(std::shared_ptr<JC_Array> buffer,
                              const uint16_t offset,
                              const uint16_t length) {
  if (!buffer || buffer->getType() != JAVA_ARRAY_T_BYTE) {
    throw Exceptions::NullPointerException;
  }
  if (static_cast<uint32_t>(offset) + length > buffer->size()) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  if ((this->apdu_state != 4 && this->apdu_state != 5) ||
      static_cast<uint32_t>(this->apdu_response_sent_length) + length >
          this->apdu_response_declared_length) {
    throw Exceptions::APDUException;
  }
  for (uint16_t index = 0; index < length; ++index) {
    this->apdu_response_data[this->apdu_response_sent_length + index] =
        static_cast<uint8_t>(buffer->getByteEntry(offset + index));
  }
  this->apdu_response_sent_length = static_cast<uint16_t>(
      this->apdu_response_sent_length + length);
  this->apdu_state = this->apdu_response_sent_length ==
                             this->apdu_response_declared_length
                         ? 6
                         : 5;
  this->apdu_outgoing_buffer = std::move(buffer);
  this->apdu_outgoing_offset = offset;
  this->apdu_outgoing_length = this->apdu_response_sent_length;
}

uint8_t Context::getAPDUOutgoingByte(const uint16_t index) const {
  if (index >= this->apdu_response_sent_length) {
    throw Exceptions::ArrayIndexOutOfBoundsException;
  }
  return this->apdu_response_data[index];
}

std::shared_ptr<JC_Array> Context::getAPDUOutgoingBuffer() const noexcept {
  return this->apdu_outgoing_buffer;
}

jref_t Context::getAPDUOutgoingBufferRef() const noexcept {
  return this->apdu_outgoing_buffer_ref;
}

uint16_t Context::getAPDUOutgoingOffset() const noexcept {
  return this->apdu_outgoing_offset;
}

uint16_t Context::getAPDUOutgoingLength() const noexcept {
  return this->apdu_outgoing_length;
}

void Context::resetSensitiveResult() noexcept {
  this->sensitive_result_kind = SensitiveResultKind::Unassigned;
  this->sensitive_result_boolean = false;
  this->sensitive_result_short = 0;
  this->sensitive_result_object = jref_t{};
}

void Context::setSensitiveResultBoolean(const bool value) noexcept {
  this->sensitive_result_kind = SensitiveResultKind::Boolean;
  this->sensitive_result_boolean = value;
}

void Context::setSensitiveResultShort(const jshort_t value) noexcept {
  this->sensitive_result_kind = SensitiveResultKind::Short;
  this->sensitive_result_short = value;
}

void Context::setSensitiveResultObject(const jref_t value) noexcept {
  this->sensitive_result_kind = SensitiveResultKind::Object;
  this->sensitive_result_object = value;
}

void Context::assertSensitiveResultObject(const jref_t value) const {
  if (this->sensitive_result_kind != SensitiveResultKind::Object ||
      this->sensitive_result_object != value) {
    throw Exceptions::SecurityException;
  }
}

void Context::assertSensitiveResultBoolean(const bool value) const {
  if (this->sensitive_result_kind != SensitiveResultKind::Boolean ||
      this->sensitive_result_boolean != value) {
    throw Exceptions::SecurityException;
  }
}

void Context::assertSensitiveResultShort(const jshort_t value,
                                         const uint8_t comparison) const {
  if (this->sensitive_result_kind != SensitiveResultKind::Short) {
    throw Exceptions::SecurityException;
  }
  bool matches = false;
  switch (comparison) {
  case 0:
    matches = this->sensitive_result_short == value;
    break;
  case 1:
    matches = this->sensitive_result_short > value;
    break;
  case 2:
    matches = this->sensitive_result_short < value;
    break;
  default:
    break;
  }
  if (!matches) {
    throw Exceptions::SecurityException;
  }
}

void Context::setPendingSystemExceptionReason(const uint16_t reason) noexcept {
  this->pending_system_exception_reason = reason;
}

uint16_t Context::consumePendingSystemExceptionReason() noexcept {
  const uint16_t reason = this->pending_system_exception_reason;
  this->pending_system_exception_reason = 1;
  return reason;
}

void Context::setPendingCryptoExceptionReason(const uint16_t reason) noexcept {
  this->pending_crypto_exception_reason = reason;
}

uint16_t Context::consumePendingCryptoExceptionReason() noexcept {
  const uint16_t reason = this->pending_crypto_exception_reason;
  this->pending_crypto_exception_reason = 1;
  return reason;
}

void Context::setPendingTransactionExceptionReason(
    const uint16_t reason) noexcept {
  this->pending_transaction_exception_reason = reason;
}

uint16_t Context::consumePendingTransactionExceptionReason() noexcept {
  const uint16_t reason = this->pending_transaction_exception_reason;
  this->pending_transaction_exception_reason = 4;
  return reason;
}

void Context::appendRootReferences(
    std::vector<jref_t> &references) const {
  const auto append_reference = [](const jref_t reference, void *opaque) {
    static_cast<std::vector<jref_t> *>(opaque)->push_back(reference);
  };
  this->visitRootReferences(append_reference, &references);
}

void Context::visitRootReferences(const Heap::ReferenceVisitor visitor,
                                  void *visitor_context) const {
  this->heap.visitRootReferences(visitor, visitor_context);
  for (const auto &entry : this->remote_objects) {
    if (entry.object_id != 0xffff && !entry.reference.isNullPointer()) {
      visitor(entry.reference, visitor_context);
    }
  }
  for (const auto &entry : this->shareable_proxies) {
    if (!entry.proxy_reference.isNullPointer()) {
      visitor(entry.proxy_reference, visitor_context);
    }
  }
  for (const auto &reference : this->shareable_object_roots) {
    if (!reference.isNullPointer()) visitor(reference, visitor_context);
  }
  const jref_t direct_roots[] = {
      this->applet_aid_object_ref, this->registered_applet_ref,
      this->restored_applet_ref, this->apdu_buffer_ref,
      this->apdu_object_ref, this->apdu_outgoing_buffer_ref,
      this->sensitive_result_object};
  for (const jref_t &reference : direct_roots) {
    if (!reference.isNullPointer()) {
      visitor(reference, visitor_context);
    }
  }
  jref_t root_return_reference;
  if (this->stack.getRootReferenceReturnValue(root_return_reference) &&
      !root_return_reference.isNullPointer()) {
    visitor(root_return_reference, visitor_context);
  }
  const auto append_array_mirror = [&](const std::shared_ptr<JC_Array> &array) {
    if (!array) {
      return;
    }
    jref_t reference;
    if (!this->heap.getArrayReference(array.get(), reference)) {
      throw Exceptions::SecurityException;
    }
    visitor(reference, visitor_context);
  };
  append_array_mirror(this->apdu_java_buffer);
  append_array_mirror(this->apdu_outgoing_buffer);
}

/**
 * Get context Java Card stack.
 *
 * @return context Java Card stack.
 */
Stack &Context::getStack() noexcept { return this->stack; }

/**
 * Get context heap.
 *
 * @return context Java Card stack.
 */
Heap &Context::getHeap() noexcept { return this->heap; }

/**
 * Get current package.
 *
 * @return current package.
 */
Package Context::getCurrentPackage() {
  return Package(this->getCurrentPackageID());
}

/**
 * Shift the context package ID. This function is call when an invoke
 * instruction is invoked. There, the security context ID does not change but
 * the executed package changed.
 *
 * @param[packageID] new executed package ID.
 */
void Context::changePackageID(const jpackage_ID_t packageID) noexcept {
  this->packagesID.push_front(packageID);
}

/**
 * Shift the context package ID to the caller one. This function is call when
 * a return instruction is executed. There, the security context ID does not
 * change but the executed package changed.
 */
void Context::backToPreviousPackageID() noexcept {
  this->packagesID.pop_front();
}

} // namespace jcvm
