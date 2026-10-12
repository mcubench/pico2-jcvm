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

#ifndef _JC_INTERPRETOR_HPP
#define _JC_INTERPRETOR_HPP

#include "context.hpp"
#include "exceptions.hpp"
#include "jc_config.h"
#include "jcvm_types/list.hpp"
#include "types.hpp"

namespace jcvm {

class Interpretor {
public:
  /// Default constructor.
  Interpretor(
      const japplet_ID_t appletID, const jpackage_ID_t selectedPackageID,
      const uint8_t selectedClass, const uint8_t method,
      bool isStaticMethod) noexcept(noexcept(std::declval<List<Context> &>()
                                                 .push_back(std::declval<
                                                            Context &>())));

  /// Start a static method directly from its offset in the CAP Method
  /// component. The three arguments match Applet.install(byte[], short, byte).
  Interpretor(const japplet_ID_t appletID,
              const jpackage_ID_t selectedPackageID,
              const uint16_t methodOffset, const uint8_t *installData,
              const uint8_t installLength);

  /// Start a virtual method on the supplied receiver words using an existing
  /// context and heap. `initial_references[0]` is the `this` receiver.
  Interpretor(Context &context, const uint16_t methodOffset,
              const jpackage_ID_t methodPackage,
              const jref_t *initial_references,
              uint8_t initial_reference_count);

  /// Run the Java Card Interpretor and return its ISO 7816 status word.
  uint16_t run() noexcept; // All exceptions must be handling there.

  uint8_t lastExceptionId() const noexcept {
    return static_cast<uint8_t>(last_exception_id);
  }
  uint8_t lastExceptionOpcode() const noexcept {
    return last_exception_opcode;
  }
  uint32_t lastExceptionPc() const noexcept { return last_exception_pc; }
  uint32_t lastExceptionPrefix() const noexcept {
    return last_exception_prefix;
  }
  uint32_t lastExceptionOpcodeHistory() const noexcept {
    return last_exception_opcode_history;
  }
  uint32_t lastExceptionDepthHistory() const noexcept {
    return last_exception_depth_history;
  }
  uint16_t lastExceptionOperand() const noexcept {
    return last_exception_operand;
  }
  uint8_t lastExceptionStackDepth() const noexcept {
    return last_exception_stack_depth;
  }
  bool installAppletRegistered() noexcept;
  jref_t installAppletReference() noexcept;

  /// Get the current context
  Context &getCurrentContext() noexcept(
      noexcept(std::declval<List<Context> &>().at(std::declval<uint8_t &>())));

  void startJCVMException(Exceptions);

private:
  /// List of Java Card contexts.
  List<Context> contexts;
  Context *external_context = nullptr;
  /// Class and method where the interpretor start.
  uint8_t startingClass, startingMethod;
  bool isStaticStatingMethod;
  uint16_t startingMethodOffset = 0;
  jpackage_ID_t startingMethodPackage = 0;
  const uint8_t *installData = nullptr;
  uint8_t installLength = 0;
  const jref_t *initialReferences = nullptr;
  uint8_t initialReferenceCount = 0;
  bool startsAtMethodOffset = false;
  bool installMethodEntry = false;
  uint16_t status_word = 0x9000;
  Exceptions last_exception_id = Exceptions::NotYetImplemented;
  uint8_t last_exception_opcode = 0;
  uint32_t last_exception_pc = 0;
  uint32_t last_exception_prefix = 0;
  uint32_t opcode_history = 0;
  uint32_t depth_history = 0;
  uint32_t last_exception_opcode_history = 0;
  uint32_t last_exception_depth_history = 0;
  uint16_t last_exception_operand = 0;
  uint8_t last_exception_stack_depth = 0;
};

} // namespace jcvm

#endif /* _JC_INTERPRETOR_HPP */
