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
#include "execution_budget.hpp"
#include "jc_bytecodes/bytecode_values.hpp"
#include "jc_bytecodes/bytecodes.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/jc_method.hpp"
#include "../../src/vm_diagnostics.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef PC_VERSION
extern "C" void choupi_watchdog_update(void);
extern "C" void choupi_vm_cooperative_progress(void);
extern "C" uint32_t choupi_time_us_32(void);
static jcvm::ExecutionBudget applet_execution_budget;
static constexpr uint32_t kAppletExecutionBudgetUs = 600000000u;

extern "C" bool choupi_vm_native_progress(void) {
  const uint32_t now_us = choupi_time_us_32();
  applet_execution_budget.reportProgress(now_us);
  return !applet_execution_budget.expired(now_us);
}
#endif

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

Interpretor::Interpretor(const japplet_ID_t appletID,
                         const jpackage_ID_t selectedPackageID,
                         const uint16_t methodOffset,
                         const uint8_t *installData,
                         const uint8_t installLength) {
  Context context(appletID, selectedPackageID);
  context.setAppletInstallationActive(true);
  this->contexts.push_back(context);
  this->startingClass = 0;
  this->startingMethod = 0;
  this->isStaticStatingMethod = true;
  this->startingMethodOffset = methodOffset;
  this->installData = installData;
  this->installLength = installLength;
  this->startsAtMethodOffset = true;
  this->installMethodEntry = true;
}

Interpretor::Interpretor(Context &context, const uint16_t methodOffset,
                         const jpackage_ID_t methodPackage,
                         const jref_t *initial_references,
                         const uint8_t initial_reference_count)
    : external_context(&context), startingClass(0), startingMethod(0),
      isStaticStatingMethod(false), startingMethodOffset(methodOffset),
      startingMethodPackage(methodPackage),
      initialReferences(initial_references),
      initialReferenceCount(initial_reference_count),
      entryFrameDepth(context.getStack().frameDepth()),
      startsAtMethodOffset(true) {}

Interpretor::Interpretor(
    Context &context, const uint16_t methodOffset,
    const jpackage_ID_t methodPackage,
    const InterpretorInitialLocal *initial_locals,
    const uint8_t initial_local_count,
    const bool captureUncaughtExceptions)
    : external_context(&context), startingClass(0), startingMethod(0),
      isStaticStatingMethod(false), startingMethodOffset(methodOffset),
      startingMethodPackage(methodPackage), initialLocals(initial_locals),
      initialLocalCount(initial_local_count),
      entryFrameDepth(context.getStack().frameDepth()),
      startsAtMethodOffset(true),
      capture_uncaught_exceptions(captureUncaughtExceptions) {}

/**
 * Runs the Java Card interpretor.
 */
uint16_t Interpretor::run() noexcept {

#ifndef PC_VERSION
  const uint32_t applet_execution_budget_us =
      this->installMethodEntry ? 5000000u : kAppletExecutionBudgetUs;
  jcvm::ExecutionBudgetScope execution_scope(
      applet_execution_budget, this->startsAtMethodOffset,
      choupi_time_us_32(), applet_execution_budget_us);
#endif
  Context &context = this->getCurrentContext();
  ActiveContextScope active_context(context);
  Package package = context.getCurrentPackage();
#ifdef PC_VERSION
  const char *trace_apdu_setting = std::getenv("CHOUPI_VM_TRACE_APDU");
  const bool trace_apdu = trace_apdu_setting != nullptr &&
                          std::strcmp(trace_apdu_setting, "1") == 0;
#endif

  TRACE_JCVM_DEBUG("Executing starting applet");

  Method_Handler methodHandler(context);

  if (this->isStaticStatingMethod) { // NOTE: Static method ref => no this.

    TRACE_JCVM_DEBUG("From static method");

    Export_Handler exportHandler(package);

    if (this->startsAtMethodOffset) {
      try {
        methodHandler.callStaticMethod(this->startingMethodOffset);
        Stack &stack = context.getStack();
        const jref_t install_array = context.getHeap().addArray(
            this->installLength, JAVA_ARRAY_T_BYTE);
        const auto install_bytes = context.getHeap().getArray(install_array);
        for (uint16_t index = 0; index < this->installLength; ++index) {
          install_bytes->setByteEntry(
              index, static_cast<jbyte_t>(this->installData[index]));
        }
        stack.writeLocal_Reference(0, install_array);
        stack.writeLocal_Short(1, 0);
        stack.writeLocal_Short(2, static_cast<int8_t>(this->installLength));
#ifdef PC_VERSION
        const char *expected_install_hex =
            std::getenv("CHOUPI_EXPECT_INSTALL_PARAMS");
        if (expected_install_hex != nullptr) {
          const auto hex_value = [](const char digit, uint8_t &value) {
            if (digit >= '0' && digit <= '9') {
              value = static_cast<uint8_t>(digit - '0');
              return true;
            }
            if (digit >= 'A' && digit <= 'F') {
              value = static_cast<uint8_t>(digit - 'A' + 10);
              return true;
            }
            if (digit >= 'a' && digit <= 'f') {
              value = static_cast<uint8_t>(digit - 'a' + 10);
              return true;
            }
            return false;
          };
          const size_t expected_hex_length =
              std::strlen(expected_install_hex);
          bool arguments_match =
              expected_hex_length ==
                  static_cast<size_t>(this->installLength) * 2 &&
              stack.readLocal_Reference(0) == install_array &&
              stack.readLocal_Short(1) == 0 &&
              stack.readLocal_Short(2) ==
                  static_cast<int8_t>(this->installLength) &&
              install_bytes->size() == this->installLength;
          for (uint16_t index = 0;
               arguments_match && index < this->installLength; ++index) {
            uint8_t high = 0;
            uint8_t low = 0;
            const size_t hex_index = static_cast<size_t>(index) * 2;
            if (!hex_value(expected_install_hex[hex_index], high) ||
                !hex_value(expected_install_hex[hex_index + 1], low)) {
              arguments_match = false;
              break;
            }
            const uint8_t expected_byte =
                static_cast<uint8_t>((high << 4) | low);
            const uint8_t actual_byte = static_cast<uint8_t>(
                install_bytes->getByteEntry(index));
            arguments_match = actual_byte == expected_byte;
          }
          std::fprintf(stderr,
                       "VPCD:INSTALL_VM_ARGS status=%s offset=%d length=%u\n",
                       arguments_match ? "PASS" : "FAIL",
                       static_cast<int>(stack.readLocal_Short(1)),
                       static_cast<unsigned>(this->installLength));
          if (!arguments_match) {
            stack.unwindToDepth(this->entryFrameDepth);
            return 0x6f00;
          }
        }
#endif
      } catch (Exceptions e) {
        this->last_exception_id = e;
        this->startJCVMException(e);
        context.getStack().unwindToDepth(this->entryFrameDepth);
        return this->status_word;
      } catch (...) {
        this->last_exception_id = Exceptions::SecurityException;
        this->startJCVMException(Exceptions::SecurityException);
        context.getStack().unwindToDepth(this->entryFrameDepth);
        return this->status_word;
      }
    } else {
      uint16_t methodOffset = exportHandler.getExportedStaticMethodOffset(
          this->startingClass, this->startingMethod);
      methodHandler.callStaticMethod(methodOffset);
    }

  } else if (this->startsAtMethodOffset) {
    if ((this->initialReferences == nullptr ||
         this->initialReferenceCount == 0) &&
        (this->initialLocals == nullptr || this->initialLocalCount == 0)) {
      this->last_exception_id = Exceptions::SecurityException;
      this->startJCVMException(Exceptions::SecurityException);
      context.getStack().unwindToDepth(this->entryFrameDepth);
      return this->status_word;
    }
    try {
#ifdef PC_VERSION
      if (trace_apdu) {
        std::fprintf(stderr,
                     "CHOUPI:VM_ENTRY stage=call_begin package=%u method=%u\n",
                     static_cast<unsigned>(this->startingMethodPackage),
                     static_cast<unsigned>(this->startingMethodOffset));
      }
#endif
      methodHandler.setPackage(Package(this->startingMethodPackage));
      methodHandler.callVirtualMethod(this->startingMethodOffset,
                                      this->initialLocals == nullptr);
#ifdef PC_VERSION
      if (trace_apdu) {
        std::fprintf(stderr, "CHOUPI:VM_ENTRY stage=call_done\n");
      }
#endif
      Stack &startup_stack = context.getStack();
      const Frame &startup_frame = startup_stack.getCurrentFrame();
      if (this->initialReferenceCount >
          startup_frame.getOP() - startup_frame.getFP()) {
        this->last_exception_id = Exceptions::SecurityException;
        this->startJCVMException(Exceptions::SecurityException);
        startup_stack.unwindToDepth(this->entryFrameDepth);
        return this->status_word;
      }
      for (uint8_t index = 0; index < this->initialReferenceCount; ++index) {
        startup_stack.writeLocal_Reference(index,
                                           this->initialReferences[index]);
      }
      if (this->initialLocals != nullptr) {
        uint16_t required_locals = 0;
        for (uint8_t index = 0; index < this->initialLocalCount; ++index) {
          required_locals +=
              this->initialLocals[index].type == InterpretorLocalType::Int
                  ? 2
                  : 1;
        }
        if (required_locals > static_cast<uint16_t>(
                                  startup_frame.getOP() -
                                  startup_frame.getFP())) {
          throw Exceptions::SecurityException;
        }
        uint16_t local_index = 0;
        for (uint8_t index = 0; index < this->initialLocalCount; ++index) {
          const auto &local = this->initialLocals[index];
          switch (local.type) {
          case InterpretorLocalType::Reference:
            startup_stack.writeLocal_Reference(
                static_cast<uint8_t>(local_index),
                jref_t(static_cast<uint16_t>(local.value)),
                local.reference_owner);
            ++local_index;
            break;
          case InterpretorLocalType::Byte:
            startup_stack.writeLocal_Short(
                static_cast<uint8_t>(local_index),
                static_cast<jshort_t>(static_cast<int8_t>(local.value)));
            ++local_index;
            break;
          case InterpretorLocalType::Short:
            startup_stack.writeLocal_Short(
                static_cast<uint8_t>(local_index),
                static_cast<jshort_t>(local.value));
            ++local_index;
            break;
          case InterpretorLocalType::Int:
#ifdef JCVM_INT_SUPPORTED
            startup_stack.writeLocal_Int(
                static_cast<uint8_t>(local_index),
                static_cast<jint_t>(local.value));
            local_index += 2;
#else
            throw Exceptions::NotYetImplemented;
#endif
            break;
          }
        }
      }
#ifdef PC_VERSION
      if (trace_apdu) {
        std::fprintf(stderr, "CHOUPI:VM_ENTRY stage=locals_ready\n");
      }
#endif
    } catch (Exceptions e) {
      this->last_exception_id = e;
      std::fprintf(stderr,
                   "CHOUPI:VM_ENTRY_EXCEPTION id=%u package=%u offset=%u\n",
                   static_cast<unsigned>(e),
                   static_cast<unsigned>(this->startingMethodPackage),
                   static_cast<unsigned>(this->startingMethodOffset));
      context.getStack().unwindToDepth(this->entryFrameDepth);
      return 0x6f00;
    } catch (...) {
      this->last_exception_id = Exceptions::SecurityException;
      std::fprintf(stderr,
                   "CHOUPI:VM_ENTRY_EXCEPTION id=unknown package=%u offset=%u\n",
                   static_cast<unsigned>(this->startingMethodPackage),
                   static_cast<unsigned>(this->startingMethodOffset));
      context.getStack().unwindToDepth(this->entryFrameDepth);
      return 0x6f00;
    }
  } else { // TODO: exported virtual method entry by class/method token.
  }

  Stack &stack = context.getStack();

  //  the interpretor runs until the Java Card stack is empty
  uint32_t instruction_count = 0;
#ifdef PC_VERSION
  uint32_t progress_interval = 0;
  if (const char *setting = std::getenv("CHOUPI_VM_PROGRESS_INTERVAL")) {
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(setting, &end, 10);
    if (end != setting && *end == '\0' && parsed > 0 &&
        parsed <= UINT32_MAX) {
      progress_interval = static_cast<uint32_t>(parsed);
    }
  }
#endif
#ifndef PC_VERSION
  applet_execution_budget.reportProgress(choupi_time_us_32());
#endif
  while (stack.frameDepth() > this->entryFrameDepth) {
#ifndef PC_VERSION
    choupi_watchdog_update();
#endif
    stack.getCurrentFrame().setInstructionPC(stack.getPC().getValue());

    // fetch: reading byte code value
    const auto instruction_pc = stack.getPC().getValue();
    uint8_t bytecode = stack.getPC().getNextByte();
    ++instruction_count;
#ifndef PC_VERSION
    // Keep CCID control traffic and reader time extensions alive during long
    // interpreted applet loops (for example, persistent Util copies or
    // software crypto). This callback does not refresh the execution budget:
    // the 120-second guard still bounds the complete applet operation.
    if ((instruction_count & 0x0fffu) == 0) {
      choupi_vm_cooperative_progress();
    }
#endif
    const auto operand_depth = static_cast<uint8_t>(
        stack.getCurrentFrame().getTOS() - stack.getCurrentFrame().getOP());
    this->opcode_history = (this->opcode_history << 8) | bytecode;
    this->depth_history = (this->depth_history << 8) | operand_depth;
#ifndef PC_VERSION
    if (this->startsAtMethodOffset) {
      choupi_install_trace(
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(instruction_pc)),
          bytecode, instruction_count);
    }
    // Bound the complete top-level applet operation. Nested interpreter
    // entries share its immutable deadline; native progress updates only the
    // service timestamp and cannot renew the execution budget.
    if (this->startsAtMethodOffset && (instruction_count & 0xffu) == 0 &&
        applet_execution_budget.expired(choupi_time_us_32())) {
      this->last_exception_id = Exceptions::RuntimeException;
      this->last_exception_opcode = bytecode;
      this->last_exception_pc =
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(instruction_pc));
      this->last_exception_prefix = 0;
      this->last_exception_operand = 0;
      if (bytecode == 0x8b) {
        this->last_exception_prefix =
            (static_cast<uint32_t>(instruction_pc[-4]) << 24) |
            (static_cast<uint32_t>(instruction_pc[-3]) << 16) |
            (static_cast<uint32_t>(instruction_pc[-2]) << 8) |
            static_cast<uint32_t>(instruction_pc[-1]);
        this->last_exception_operand = static_cast<uint16_t>(
            (static_cast<uint16_t>(instruction_pc[1]) << 8) |
            instruction_pc[2]);
      }
      this->last_exception_stack_depth = operand_depth;
      this->last_exception_opcode_history = this->opcode_history;
      this->last_exception_depth_history = this->depth_history;
      this->status_word = 0x6400;
      stack.unwindToDepth(this->entryFrameDepth);
      return this->status_word;
    }
#endif
#ifdef PC_VERSION
    if (progress_interval != 0 &&
        instruction_count % progress_interval == 0) {
      const Package active_package = context.getCurrentPackage();
      const auto *method_component = active_package.getCap().getMethod();
      const auto methods = method_component == nullptr
                               ? JCVMArray<const uint8_t>(0, nullptr)
                               : method_component->methods();
      const uintptr_t pc_address = reinterpret_cast<uintptr_t>(instruction_pc);
      const uintptr_t methods_address =
          reinterpret_cast<uintptr_t>(methods.data());
      const uintptr_t method_start_address = reinterpret_cast<uintptr_t>(
          stack.getCurrentFrame().getMethodStartPC());
      const long method_pc_offset =
          pc_address >= methods_address &&
                  pc_address - methods_address < methods.size()
              ? static_cast<long>(pc_address - methods_address)
              : -1;
      const long method_start_offset =
          method_start_address >= methods_address &&
                  method_start_address - methods_address < methods.size()
              ? static_cast<long>(method_start_address - methods_address)
              : -1;
      std::fprintf(
          stderr,
          "CHOUPI:VM_PROGRESS phase=%s ops=%lu package=%u opcode=%02X pc=%p method_pc=%ld method_start=%ld "
          "operand_depth=%u op_history=%08X depth_history=%08X\n",
          this->installMethodEntry ? "install" :
              (this->startsAtMethodOffset ? "applet" : "runtime"),
          static_cast<unsigned long>(instruction_count),
          static_cast<unsigned>(context.getCurrentPackageID()), bytecode,
          static_cast<const void *>(stack.getPC().getValue()),
          method_pc_offset, method_start_offset,
          static_cast<unsigned>(operand_depth), this->opcode_history,
          this->depth_history);
    }
#endif
    if (!this->startsAtMethodOffset && instruction_count % 1000000u == 0) {
      std::printf("CHOUPI:VM_PROGRESS ops=%lu opcode=0x%02X pc=%p\r\n",
                  static_cast<unsigned long>(instruction_count), bytecode,
                  static_cast<const void *>(stack.getPC().getValue()));
    }

    // bytecodes interface
    Bytecodes bytecodes(context, this->entryFrameDepth,
                        this->capture_uncaught_exceptions);

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
    } catch (const UncaughtJavaException &uncaught) {
#ifndef PC_VERSION
      std::printf("CHOUPI:UNCAUGHT_JAVA ref=%08X opcode=%02X depth=%u\r\n",
                  static_cast<unsigned>(uncaught.reference), bytecode,
                  static_cast<unsigned>(stack.frameDepth()));
      std::fflush(stdout);
#endif
      this->last_exception_id = Exceptions::NotYetImplemented;
      this->last_uncaught_exception_reference = uncaught.reference;
      this->status_word = 0x6f00;
      stack.unwindToDepth(this->entryFrameDepth);
      return this->status_word;
    } catch (const ISOExceptionStatusWord &iso_exception) {
      this->last_exception_id = Exceptions::ISOException;
      this->last_exception_reason = iso_exception.value;
      this->last_exception_opcode = bytecode;
      this->last_exception_pc =
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(instruction_pc));
      this->status_word = iso_exception.value;
      stack.unwindToDepth(this->entryFrameDepth);
      return this->status_word;
    } catch (Exceptions e) {
#ifndef PC_VERSION
      std::printf("CHOUPI:VM_CAUGHT_EXCEPTION id=%u opcode=%02X depth=%u\r\n",
                  static_cast<unsigned>(e), bytecode,
                  static_cast<unsigned>(stack.getCurrentFrame().getTOS() -
                                        stack.getCurrentFrame().getOP()));
      std::fflush(stdout);
#endif
      if (e == Exceptions::SystemException) {
        this->last_exception_reason = context.pendingSystemExceptionReason();
      } else if (e == Exceptions::APDUException) {
        this->last_exception_reason = 1; // APDUException.ILLEGAL_USE
      } else if (e == Exceptions::TransactionException) {
        this->last_exception_reason =
            context.pendingTransactionExceptionReason();
      }
      try {
        if (bytecodes.dispatchJavaException(e)) {
          continue;
        }
      } catch (...) {
        // If no Java handler matches, retain the existing VM-fatal path.
      }
      this->last_exception_id = e;
      this->last_exception_opcode = bytecode;
      this->last_exception_pc =
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(instruction_pc));
      this->last_exception_prefix = bytecode == 0x8b
          ? (static_cast<uint32_t>(instruction_pc[-4]) << 24) |
                (static_cast<uint32_t>(instruction_pc[-3]) << 16) |
                (static_cast<uint32_t>(instruction_pc[-2]) << 8) |
                static_cast<uint32_t>(instruction_pc[-1])
          : 0;
      if (bytecode == 0x8b) {
        this->last_exception_operand = static_cast<uint16_t>(
            (static_cast<uint16_t>(instruction_pc[1]) << 8) |
            instruction_pc[2]);
      }
      this->last_exception_stack_depth = static_cast<uint8_t>(
          stack.getCurrentFrame().getTOS() -
          stack.getCurrentFrame().getOP());
      this->last_exception_opcode_history = this->opcode_history;
      this->last_exception_depth_history = this->depth_history;
      if (!this->startsAtMethodOffset) {
        std::printf("CHOUPI:VM_EXCEPTION id=%u opcode=0x%02X pc=%p\r\n",
                    static_cast<unsigned>(e), bytecode,
                    static_cast<const void *>(stack.getPC().getValue()));
        TRACE_JCVM_ERR("Exception %u at opcode 0x%02X pc=%p",
                       static_cast<unsigned>(e), bytecode,
                       static_cast<const void *>(stack.getPC().getValue()));
      }
      this->startJCVMException(e);
      stack.unwindToDepth(this->entryFrameDepth);
      return this->status_word;
    } catch (...) {
      this->last_exception_id = Exceptions::SecurityException;
      this->last_exception_opcode = bytecode;
      this->last_exception_pc =
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(instruction_pc));
      this->last_exception_prefix = bytecode == 0x8b
          ? (static_cast<uint32_t>(instruction_pc[-4]) << 24) |
                (static_cast<uint32_t>(instruction_pc[-3]) << 16) |
                (static_cast<uint32_t>(instruction_pc[-2]) << 8) |
                static_cast<uint32_t>(instruction_pc[-1])
          : 0;
      this->last_exception_stack_depth = static_cast<uint8_t>(
          stack.getCurrentFrame().getTOS() -
          stack.getCurrentFrame().getOP());
      this->last_exception_opcode_history = this->opcode_history;
      this->last_exception_depth_history = this->depth_history;
      if (!this->startsAtMethodOffset) {
        std::printf("CHOUPI:VM_EXCEPTION id=unknown opcode=0x%02X pc=%p\r\n",
                    bytecode,
                    static_cast<const void *>(stack.getPC().getValue()));
      }
      this->startJCVMException(Exceptions::SecurityException);
      stack.unwindToDepth(this->entryFrameDepth);
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
  if (this->external_context != nullptr) {
    return *this->external_context;
  }
  return this->contexts.front();
}

bool Interpretor::installAppletRegistered() noexcept {
  return this->installMethodEntry &&
         this->getCurrentContext().wasAppletRegistered();
}

jref_t Interpretor::installAppletReference() noexcept {
  if (!this->installAppletRegistered()) {
    return jref_t();
  }
  return this->getCurrentContext().getRegisteredAppletRef();
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
