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

#include <memory>

namespace jcvm {

class JC_Array;

class Context {
private:
  /// Applet ID.
  japplet_ID_t applet_ID;
  /// Set when the active install method invokes Applet.register().
  bool applet_registered = false;
  jref_t registered_applet_ref;
  bool applet_instance_restored = false;
  jref_t restored_applet_ref;
  jref_t apdu_buffer_ref;
  std::shared_ptr<JC_Array> apdu_java_buffer;
  jref_t apdu_object_ref;
  jref_t apdu_outgoing_buffer_ref;
  std::shared_ptr<JC_Array> apdu_outgoing_buffer;
  uint16_t apdu_incoming_length = 0;
  uint16_t apdu_outgoing_offset = 0;
  uint16_t apdu_outgoing_length = 0;
  /// Current Java Card stack.
  Stack stack;
  /// Linked-list where the executed Packages ID is stored. The Head of the
  /// linked-list is the current executed package ID.
  List<jpackage_ID_t> packagesID;
  /// Context's heap
  Heap heap;

public:
  /// Set the active VM context and return the context previously active.
  static Context *setActiveContext(Context *context) noexcept;
  /// Get the context used by context-free native method implementations.
  static Context &getActiveContext();

  /// Default constructor
  Context(const uint8_t appletID, const jpackage_ID_t packageID) noexcept;
  /// Get the context ID.
  jpackage_ID_t getCurrentPackageID() noexcept;
  /// Get applet ID
  japplet_ID_t getAppletID() noexcept;
  /// Record and query Applet.register() calls during installation.
  void markAppletRegistered(jref_t applet_ref) noexcept;
  bool wasAppletRegistered() const noexcept;
  jref_t getRegisteredAppletRef() const noexcept;
  /// Restore the persisted applet root and eagerly decode its field graph.
  bool restoreAppletInstance(jpackage_ID_t package_id,
                             jclass_index_t class_index) noexcept;
  bool hasRestoredAppletInstance() const noexcept;
  jref_t getRestoredAppletRef() const noexcept;
  /// Set and query the APDU buffer and response window for the current command.
  void setAPDUBufferRef(jref_t reference, uint16_t incoming_length) noexcept;
  void setAPDUJavaBuffer(std::shared_ptr<JC_Array> buffer);
  std::shared_ptr<JC_Array> getAPDUJavaBuffer() const noexcept;
  jref_t getAPDUBufferRef() const noexcept;
  void setAPDUObjectRef(jref_t reference) noexcept;
  jref_t getAPDUObjectRef() const noexcept;
  uint16_t getAPDUIncomingLength() const noexcept;
  void setAPDUOutgoing(std::shared_ptr<JC_Array> buffer, uint16_t offset,
                       uint16_t length);
  std::shared_ptr<JC_Array> getAPDUOutgoingBuffer() const noexcept;
  jref_t getAPDUOutgoingBufferRef() const noexcept;
  uint16_t getAPDUOutgoingOffset() const noexcept;
  uint16_t getAPDUOutgoingLength() const noexcept;
  /// Get context Java Card stack.
  Stack &getStack() noexcept;
  /// Get context heap.
  Heap &getHeap() noexcept;
  /// Get current current package.
  Package getCurrentPackage();
  /// Shift context package ID
  void changePackageID(const uint8_t packageID) noexcept;
  /// Back to the previous package ID.
  void backToPreviousPackageID() noexcept;
};

} // namespace jcvm

#endif /* _CONTEXT_HPP */
