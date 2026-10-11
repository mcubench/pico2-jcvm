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
    : applet_ID(appletID), stack(), packagesID(), heap(appletID) {
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
