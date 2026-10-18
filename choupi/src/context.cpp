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

#include <algorithm>
#include <utility>

namespace jcvm {

namespace {
Context *active_context = nullptr;
}

Context *Context::setActiveContext(Context *context) noexcept {
  Context *previous = active_context;
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
jpackage_ID_t Context::getAppletID() noexcept { return this->applet_ID; }

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
      return false;
    }
    const auto instance = this->heap.getInstance(reference);
    if (instance->getPackageID() != package_id ||
        instance->getClassIndex() != class_index) {
      return false;
    }
    (void)instance->getFields();
    this->restored_applet_ref = reference;
    this->applet_instance_restored = true;
    return true;
  } catch (...) {
    return false;
  }
}

bool Context::hasRestoredAppletInstance() const noexcept {
  return this->applet_instance_restored;
}

jref_t Context::getRestoredAppletRef() const noexcept {
  return this->restored_applet_ref;
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

void Context::setPendingSystemExceptionReason(const uint16_t reason) noexcept {
  this->pending_system_exception_reason = reason;
}

uint16_t Context::consumePendingSystemExceptionReason() noexcept {
  const uint16_t reason = this->pending_system_exception_reason;
  this->pending_system_exception_reason = 1;
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
  const jref_t direct_roots[] = {
      this->applet_aid_object_ref, this->registered_applet_ref,
      this->restored_applet_ref, this->apdu_buffer_ref,
      this->apdu_object_ref, this->apdu_outgoing_buffer_ref};
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
