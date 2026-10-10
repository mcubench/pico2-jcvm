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

#include "interpretor.hpp"
#include "debug.hpp"
#include "jc_bytecodes/bytecode_values.hpp"
#include "jc_bytecodes/bytecodes.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/jc_method.hpp"

#include <cstdio>

#ifdef DEBUG
#include <string>
#endif /* DEBUG */

namespace jcvm {

namespace {
class ActiveContextScope {
private:
  Context *previous;

public:
  explicit ActiveContextScope(Context &context) noexcept
      : previous(Context::setActiveContext(&context)) {}

  ~ActiveContextScope() noexcept { Context::setActiveContext(previous); }
};
} // namespace

/**
 * Default constructor.
 *
 * @param[selectedPackageID] Index package where the method to execute is
 * located
 * @param[selectedClass] Index class where the method to execute is located
 * @param[method] Index public method to execute
 * @param[isStaticMethod] Is the method static?
 */
Interpretor::Interpretor(
    const japplet_ID_t appletID, const jpackage_ID_t selectedPackageID,
    const uint8_t selectedClass, const uint8_t method,
    bool isStaticMethod) noexcept(noexcept(std::declval<List<Context> &>()
                                               .push_back(std::declval<
                                                          Context &>()))) {
  Context context(appletID, selectedPackageID);
  this->contexts.push_back(context);

  this->startingClass = selectedClass;
  this->startingMethod = method;
  this->isStaticStatingMethod = isStaticMethod;
}

/**
 * Runs the Java Card interpretor.
 */
uint16_t Interpretor::run() noexcept {

  Context &context = this->getCurrentContext();
  ActiveContextScope active_context(context);
  Package package = context.getCurrentPackage();

  TRACE_JCVM_DEBUG("Executing starting applet");

  Method_Handler methodHandler(context);

  if (this->isStaticStatingMethod) { // NOTE: Static method ref => no this.

    TRACE_JCVM_DEBUG("From static method");

    Export_Handler exportHandler(package);

    uint16_t methodOffset = exportHandler.getExportedStaticMethodOffset(
        this->startingClass, this->startingMethod);

    methodHandler.callStaticMethod(methodOffset);

  } else { // TODO: Virtual method => this is requirequired.
    // stack.push_Reference(THIS);
  }

  Stack &stack = context.getStack();

  //  the interpretor runs until the Java Card stack is empty
  uint32_t instruction_count = 0;
  while (stack.empty() == false) {
    stack.getCurrentFrame().setInstructionPC(stack.getPC().getValue());

    // fetch: reading byte code value
    uint8_t bytecode = stack.getPC().getNextByte();
    ++instruction_count;
    if (instruction_count % 1000000u == 0) {
      std::printf("CHOUPI:VM_PROGRESS ops=%lu opcode=0x%02X pc=%p\r\n",
                  static_cast<unsigned long>(instruction_count), bytecode,
                  static_cast<const void *>(stack.getPC().getValue()));
    }

    // bytecodes interface
    Bytecodes bytecodes(context);

    // decode: call the corresponding native function
    // #ifdef DEBUG
    //     std::cout << "(@" << static_cast<const void
    //     *>(stack.getPC().getValue())
    //               << ") "
    //               << "[0x" << std::hex << std::setw(2) << std::setfill('0')
    //               << (int)bytecode << "] " << std::dec;
    // #endif /* DEBUG */
    try {
      auto bc = bytecodes.decode(bytecode);

      // execute
      (bytecodes.*bc)();
    } catch (Exceptions e) {
      std::printf("CHOUPI:VM_EXCEPTION id=%u opcode=0x%02X pc=%p\r\n",
                  static_cast<unsigned>(e), bytecode,
                  static_cast<const void *>(stack.getPC().getValue()));
      TRACE_JCVM_ERR("Exception %u at opcode 0x%02X pc=%p",
                     static_cast<unsigned>(e), bytecode,
                     static_cast<const void *>(stack.getPC().getValue()));
      this->startJCVMException(e);
      return this->status_word;
    } catch (...) {
      std::printf("CHOUPI:VM_EXCEPTION id=unknown opcode=0x%02X pc=%p\r\n",
                  bytecode,
                  static_cast<const void *>(stack.getPC().getValue()));
      this->startJCVMException(Exceptions::SecurityException);
      return this->status_word;
    }
  }

  return this->status_word;
}

/**
 * Get the current context
 */
Context &Interpretor::getCurrentContext() noexcept(
    noexcept(std::declval<List<Context> &>().at(std::declval<uint8_t &>()))) {
  return this->contexts.front();
}

void Interpretor::startJCVMException(Exceptions e) {

#ifdef DEBUG

  std::string msg;

  switch (e) {
  case NotYetImplemented:
    msg = "NotYetImplemented";
    break;
  case StackUnderflowException:
    msg = "StackUnderflowException";
    break;
  case StackOverflowException:
    msg = "StackOverflowException";
    break;
  case CardException:
    msg = "CardException";
    break;
  case UserException:
    msg = "UserException";
    break;
  case IOException:
    msg = "IOException";
    break;
  case RemoteException:
    msg = "RemoteException";
    break;
  case RuntimeException:
    msg = "RuntimeException";
    break;
  case ArithmeticException:
    msg = "ArithmeticException";
    break;
  case ArrayStoreException:
    msg = "ArrayStoreException";
    break;
  case CardRuntimeException:
    msg = "CardRuntimeException";
    break;
  case APDUException:
    msg = "APDUException";
    break;
  case Bio1NException:
    msg = "Bio1NException";
    break;
  case BioException:
    msg = "BioException";
    break;
  case CryptoException:
    msg = "CryptoException";
    break;
  case ExternalException:
    msg = "ExternalException";
    break;
  case ISOException:
    msg = "ISOException";
    break;
  case PINException:
    msg = "PINException";
    break;
  case StringException:
    msg = "StringException";
    break;
  case ServiceException:
    msg = "ServiceException";
    break;
  case SystemException:
    msg = "SystemException";
    break;
  case TLVException:
    msg = "TLVException";
    break;
  case TransactionException:
    msg = "TransactionException";
    break;
  case UtilException:
    msg = "UtilException";
    break;
  case ClassCastException:
    msg = "ClassCastException";
    break;
  case IndexOutOfBoundsException:
    msg = "IndexOutOfBoundsException";
    break;
  case ArrayIndexOutOfBoundsException:
    msg = "ArrayIndexOutOfBoundsException";
    break;
  case NegativeArraySizeException:
    msg = "NegativeArraySizeException";
    break;
  case NullPointerException:
    msg = "NullPointerException";
    break;
  case SecurityException:
    msg = "SecurityException";
    break;
  case FullMemoryException:
    msg = "FullMemoryException";
    break;
  }

  TRACE_JCVM_ERR("The exception %s was thrown but not caught!", msg.c_str());

#endif /* DEBUG */

#ifdef PC_VERSION
  TRACE_JCVM_ERR("^C to stop the program execution ...");
#endif /* PC_VERSION */
  this->status_word = 0x6F00;
}

} // namespace jcvm
