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

#include "debug.hpp"
#include "interpretor.hpp"
#include "jc_config.h"
#include "types.hpp"

#include "jc_handlers/jc_class.hpp"
#include "jc_handlers/jc_cp.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_import.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_array_type.hpp"
#include "../../src/short_apdu.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

#ifdef PC_VERSION
extern int main_pc(int argc, char *argv[]);
#else
extern int main_arm(void);
#endif /* PC_VERSION */

int main
#ifdef PC_VERSION
    (int argc, char *argv[])
#else
    ()
#endif /* PC_VERSION */
{
#ifdef PC_VERSION
  return main_pc(argc, argv);
#else
  return main_arm();
#endif /* PC_VERSION */
}

namespace {
std::unique_ptr<jcvm::Context> selected_applet_context;
bool selected_applet_is_active = false;
pico2_jcvm::ShortApduResponseChain pending_apdu_response;
}

#ifdef __cplusplus
extern "C" {
#endif

uint16_t choupi_vm_last_native_method_index();
#ifndef PC_VERSION
void choupi_install_stage(uint8_t stage);
void choupi_install_context(uint32_t context);
uint8_t choupi_install_debug_stage(void);
uint32_t choupi_install_debug_context(void);
#endif

void choupi_ccid_task(void);

uint8_t choupi_vm_restore_applet(uint8_t package_id,
                                 uint16_t class_index, const uint8_t *aid,
                                 uint8_t aid_length) {
  pending_apdu_response.queue(nullptr, 0);
  try {
    auto context = std::make_unique<jcvm::Context>(
        0, static_cast<jcvm::jpackage_ID_t>(package_id));
    if (!context->setAppletAID(aid, aid_length) ||
        !context->restoreAppletInstance(
            static_cast<jcvm::jpackage_ID_t>(package_id),
            static_cast<jcvm::jclass_index_t>(class_index)) ||
        !context->hasRestoredAppletInstance()) {
      selected_applet_context.reset();
      selected_applet_is_active = false;
      return 1;
    }
    selected_applet_context = std::move(context);
    selected_applet_is_active = false;
    return 0;
  } catch (...) {
    selected_applet_context.reset();
    selected_applet_is_active = false;
    return 1;
  }
}

void choupi_vm_clear_selected_applet(void) {
  pending_apdu_response.queue(nullptr, 0);
  selected_applet_context.reset();
  selected_applet_is_active = false;
}

uint16_t choupi_vm_invoke_applet_method(uint16_t method_offset,
                                       uint8_t method_package,
                                       const jcvm::jref_t *initial_references,
                                       uint8_t initial_reference_count) {
  if (!selected_applet_context || initial_references == nullptr ||
      initial_reference_count == 0) {
    return 0x6f00;
  }
  jcvm::Interpretor interpretor(*selected_applet_context, method_offset,
                                static_cast<jcvm::jpackage_ID_t>(method_package),
                                initial_references,
                                initial_reference_count);
  return interpretor.run();
}

namespace {
uint16_t invoke_selected_applet_method(const uint8_t method_token,
                                       uint32_t *return_value) {
  if (!selected_applet_context) {
    return 0x6999;
  }
  try {
    jcvm::Context &context = *selected_applet_context;
    const jcvm::jref_t applet_ref = context.getRestoredAppletRef();
    if (applet_ref.isNullPointer() || applet_ref.isArray()) {
      return 0x6f00;
    }
    const auto applet = context.getHeap().getInstance(applet_ref);
    jcvm::Package package(applet->getPackageID());
    jcvm::ConstantPool_Handler cp_handler(package);
    jcvm::jc_cap_virtual_method_ref_info method_ref;
    method_ref.class_ref = cp_handler.getClassRefFromClassIndex(
        applet->getClassIndex());
    method_ref.token = method_token;
    const auto method = jcvm::Class_Handler(package).getMethodOffset(method_ref);
    const jcvm::jref_t initial_references[] = {applet_ref};
    jcvm::Interpretor interpretor(
        context, method.second, method.first.getPackageID(),
        initial_references, 1);
    const uint16_t status = interpretor.run();
    if (status == 0x9000 && return_value != nullptr) {
      const jcvm::Stack &stack = context.getStack();
      if (!stack.hasRootReturnValue()) {
        return 0x6f00;
      }
      *return_value = stack.getRootReturnValue();
    }
    return status;
  } catch (...) {
    return 0x6f00;
  }
}
} // namespace

uint16_t choupi_vm_select_applet(void) {
  pending_apdu_response.queue(nullptr, 0);
  uint32_t selected = 0;
  const uint16_t status = invoke_selected_applet_method(6, &selected);
  if (status != 0x9000) {
    selected_applet_context.reset();
    selected_applet_is_active = false;
    return status;
  }
  if (selected == 0) {
    selected_applet_context.reset();
    selected_applet_is_active = false;
    return 0x6999;
  }
  selected_applet_is_active = true;
  return 0x9000;
}

uint16_t choupi_vm_deselect_applet(void) {
  pending_apdu_response.queue(nullptr, 0);
  if (!selected_applet_context || !selected_applet_is_active) {
    selected_applet_context.reset();
    selected_applet_is_active = false;
    return 0x9000;
  }
  const uint16_t status = invoke_selected_applet_method(4, nullptr);
  selected_applet_context.reset();
  selected_applet_is_active = false;
  return status;
}

uint16_t choupi_vm_process_applet(const uint8_t *command,
                                 uint16_t command_length,
                                 uint8_t *response,
                                 uint16_t response_capacity,
                                 uint16_t *response_length) {
  if (response_length != nullptr) {
    *response_length = 0;
  }
  if (response == nullptr || response_length == nullptr ||
      response_capacity < 2) {
    return 0x6f00;
  }

  uint16_t status = 0x6f00;
  uint16_t data_length = 0;
  try {
    if (!selected_applet_context || !selected_applet_is_active) {
      return 0xffff; // CCID maps the no-selection sentinel to its default SW.
    }
    if (command == nullptr || command_length < 4) {
      status = 0x6700;
      throw status;
    }

    jcvm::Context &context = *selected_applet_context;
    jcvm::Heap &heap = context.getHeap();
    pico2_jcvm::ShortApduCommand parsed_command;
    if (!pico2_jcvm::parse_short_apdu(command, command_length,
                                      parsed_command)) {
      pending_apdu_response.queue(nullptr, 0);
      status = 0x6700;
      throw status;
    }

    const bool get_response = command[0] == 0x00 && command[1] == 0xc0;
    if (get_response) {
      if (command[2] != 0 || command[3] != 0) {
        status = 0x6a86;
      } else if (!parsed_command.has_expected_response_length) {
        status = 0x6700;
      } else if (!pending_apdu_response.has_pending()) {
        status = 0x6a86;
      } else {
        data_length = pending_apdu_response.take(
            parsed_command.expected_response_length, response,
            static_cast<uint16_t>(response_capacity - 2), status);
      }
      response[data_length] = static_cast<uint8_t>(status >> 8);
      response[data_length + 1] = static_cast<uint8_t>(status);
      *response_length = static_cast<uint16_t>(data_length + 2);
      return status;
    }
    pending_apdu_response.queue(nullptr, 0);

    const uint16_t lc = parsed_command.incoming_length;

    jcvm::jref_t buffer_ref = context.getAPDUBufferRef();
    auto buffer = context.getAPDUJavaBuffer();
    if (!buffer && buffer_ref.isNullPointer()) {
      buffer_ref = heap.addArray(260, jcvm::JAVA_ARRAY_T_BYTE);
    }
    if (!buffer) {
      buffer = heap.getArray(buffer_ref);
    }
    if (!buffer || buffer->getType() != jcvm::JAVA_ARRAY_T_BYTE ||
        buffer->size() < static_cast<uint16_t>(5 + lc)) {
      status = 0x6f00;
      throw status;
    }
    for (uint16_t index = 0; index < buffer->size(); ++index) {
      buffer->setByteEntry(index, 0);
    }
    for (uint16_t index = 0; index < 4; ++index) {
      buffer->setByteEntry(index, static_cast<jcvm::jbyte_t>(command[index]));
    }
    // For case 2S P3 is Le; for case 3S/4S it is Lc.
    buffer->setByteEntry(
        4, static_cast<jcvm::jbyte_t>(command_length > 4 ? command[4] : 0));
    for (uint16_t index = 0; index < lc; ++index) {
      buffer->setByteEntry(static_cast<uint16_t>(5 + index),
                           static_cast<jcvm::jbyte_t>(command[5 + index]));
    }
    context.setAPDUBufferRef(buffer_ref, lc,
                             parsed_command.expected_response_length,
                             parsed_command.has_expected_response_length);

    jcvm::jref_t apdu_ref = context.getAPDUObjectRef();
    if (apdu_ref.isNullPointer()) {
      const jcvm::jref_t applet_ref = context.getRestoredAppletRef();
      if (applet_ref.isNullPointer() || applet_ref.isArray()) {
        status = 0x6f00;
        throw status;
      }
      const auto applet = heap.getInstance(applet_ref);
      jcvm::Package applet_package(applet->getPackageID());
      const auto applet_cap = applet_package.getCap();
      const auto *applet_package_info =
          applet_cap.getHeader()->package.AID_length
              ? &applet_cap.getHeader()->package
              : nullptr;
      if (applet_package_info == nullptr) {
        status = 0x6f00;
        throw status;
      }

      constexpr uint8_t framework_aid[] = {0xa0, 0x00, 0x00, 0x00,
                                           0x62, 0x01, 0x01};
      jcvm::Import_Handler import_handler(applet_package);
      const auto *imports = applet_package.getCap().getImport();
      const jcvm::jc_cap_package_info *framework_info = nullptr;
      for (uint8_t index = 0; index < imports->count; ++index) {
        const auto *candidate = import_handler.getPackageAID(index);
        if (candidate->AID_length == sizeof(framework_aid) &&
            std::memcmp(candidate->AID, framework_aid,
                        sizeof(framework_aid)) == 0) {
          framework_info = candidate;
          break;
        }
      }
      if (framework_info == nullptr) {
        status = 0x6f00;
        throw status;
      }
      const auto framework_package_id =
          import_handler.getPackageIndex(framework_info);
      jcvm::Package framework_package(framework_package_id);
      const uint8_t apdu_export_token =
          jcvm::remap_imported_class_token(*framework_info, 10);
      const uint16_t apdu_class_index = HTONS(
          jcvm::Export_Handler(framework_package)
              .getExportedClassOffset(apdu_export_token));
      apdu_ref = heap.addInstance(framework_package_id, apdu_class_index);
      context.setAPDUObjectRef(apdu_ref);
    }

    const jcvm::jref_t applet_ref = context.getRestoredAppletRef();
    const auto applet = heap.getInstance(applet_ref);
    jcvm::Package applet_package(applet->getPackageID());
    jcvm::ConstantPool_Handler cp_handler(applet_package);
    jcvm::jc_cap_virtual_method_ref_info method_ref;
    method_ref.class_ref = cp_handler.getClassRefFromClassIndex(
        applet->getClassIndex());
    method_ref.token = 7; // Applet.process(APDU)
    const auto method = jcvm::Class_Handler(applet_package).getMethodOffset(
        method_ref, applet->getPackageID(), applet->getClassIndex());
    const jcvm::jref_t initial_references[] = {applet_ref, apdu_ref};
    jcvm::Interpretor interpretor(context, method.second,
                                  method.first.getPackageID(),
                                  initial_references, 2);
    status = interpretor.run();
    if (status == 0x9000) {
      data_length = context.getAPDUOutgoingLength();
      if (static_cast<uint32_t>(data_length) + 2 > response_capacity ||
          context.getAPDUState() == 3 || context.getAPDUState() == 4 ||
          context.getAPDUState() == 5) {
        status = 0x6f00;
        data_length = 0;
      } else {
        for (uint16_t index = 0; index < data_length; ++index) {
          response[index] = context.getAPDUOutgoingByte(index);
        }
      }
    }
    if (status == 0x9000 && parsed_command.has_expected_response_length &&
        data_length > parsed_command.expected_response_length) {
      const uint16_t first_chunk = parsed_command.expected_response_length;
      const uint16_t remaining =
          static_cast<uint16_t>(data_length - first_chunk);
      if (!pending_apdu_response.queue(response + first_chunk, remaining)) {
        status = 0x6f00;
        data_length = 0;
      } else {
        data_length = first_chunk;
        const uint16_t sw2 = remaining >= 256 ? 0 : remaining;
        status = static_cast<uint16_t>(0x6100 | sw2);
      }
    }
  } catch (uint16_t caught_status) {
    status = caught_status;
    data_length = 0;
  } catch (...) {
    status = 0x6f00;
    data_length = 0;
  }

  if (static_cast<uint32_t>(data_length) + 2 > response_capacity) {
    status = 0x6f00;
    data_length = 0;
  }
  response[data_length] = static_cast<uint8_t>(status >> 8);
  response[data_length + 1] = static_cast<uint8_t>(status);
  *response_length = static_cast<uint16_t>(data_length + 2);
  return status;
}

uint16_t runtime(uint8_t id_package, uint8_t id_class, uint8_t id_method) {
  TRACE_JCVM_DEBUG("Starting JCVM");

  jcvm::japplet_ID_t id_applet = 0;

  // TODO: is a static method?
  jcvm::Interpretor interpretor(id_applet,
                                static_cast<jcvm::jpackage_ID_t>(id_package),
                                id_class, id_method, true);
  const uint16_t status = interpretor.run();
#ifndef PC_VERSION
  if (status != 0x9000) {
    std::printf(
        "CHOUPI:VM_RUNTIME_FAILURE sw=%04X exception=%u opcode=%02X "
        "pc=%08lX stack=%u op_history=%08lX depth_history=%08lX\r\n",
        static_cast<unsigned>(status),
        static_cast<unsigned>(interpretor.lastExceptionId()),
        static_cast<unsigned>(interpretor.lastExceptionOpcode()),
        static_cast<unsigned long>(interpretor.lastExceptionPc()),
        static_cast<unsigned>(interpretor.lastExceptionStackDepth()),
        static_cast<unsigned long>(interpretor.lastExceptionOpcodeHistory()),
        static_cast<unsigned long>(interpretor.lastExceptionDepthHistory()));
    std::fflush(stdout);
  }
#endif
  return status;
}

uint16_t runtime_install_method(uint8_t id_package, uint16_t method_offset,
                                const uint8_t *install_data,
                                uint8_t install_length,
                                const uint8_t *instance_aid,
                                uint8_t instance_aid_length,
                                uint8_t *applet_registered,
                                uint16_t *applet_class_index,
                                uint8_t *exception_id,
                                uint8_t *exception_opcode,
                                uint32_t *exception_pc,
                                uint32_t *exception_prefix,
                                uint16_t *exception_operand,
                                uint8_t *exception_stack_depth,
                                uint32_t *exception_opcode_history,
                                uint32_t *exception_depth_history,
                                uint8_t *install_debug_stage,
                                uint32_t *install_debug_context) {
  TRACE_JCVM_DEBUG("Starting CAP install method at offset %u",
                   static_cast<unsigned>(method_offset));
#ifndef PC_VERSION
  choupi_install_stage(0);
  choupi_install_context(0);
#endif
  jcvm::Interpretor interpretor(0,
                                static_cast<jcvm::jpackage_ID_t>(id_package),
                                method_offset, install_data, install_length);
  if (!interpretor.getCurrentContext().setAppletAID(instance_aid,
                                                    instance_aid_length)) {
    return 0x6a80;
  }
  const uint16_t status = interpretor.run();
  bool applet_persisted = false;
  uint16_t registered_class_index = 0xffff;
  if (status == 0x9000 && interpretor.installAppletRegistered()) {
    try {
      const jcvm::jref_t applet_ref = interpretor.installAppletReference();
      jcvm::Context &context = interpretor.getCurrentContext();
      const auto applet = context.getHeap().getInstance(applet_ref);
      registered_class_index = applet->getClassIndex();
      const jcvm::fs::Tag instance_tag =
          jcvm::FlashMemory_Handler::getAppletInstanceTag(
              context.getAppletID(), applet->getPackageID(),
              applet->getClassIndex());
      jcvm::FlashMemory_Handler::setPersistentField_Instance(
          instance_tag, *applet, context.getHeap());
      applet_persisted = true;
    } catch (...) {
      TRACE_JCVM_ERR("Failed to persist registered applet instance");
    }
  }
  if (applet_registered != nullptr) {
    *applet_registered = applet_persisted ? 1 : 0;
  }
  if (applet_class_index != nullptr) {
    *applet_class_index = applet_persisted ? registered_class_index : 0xffff;
  }
  if (exception_id != nullptr) {
    *exception_id = interpretor.lastExceptionId();
  }
  if (exception_opcode != nullptr) {
    *exception_opcode = interpretor.lastExceptionOpcode();
  }
  if (exception_pc != nullptr) {
    *exception_pc = interpretor.lastExceptionPc();
  }
  if (exception_prefix != nullptr) {
    *exception_prefix = interpretor.lastExceptionPrefix();
  }
  if (exception_operand != nullptr) {
    *exception_operand = interpretor.lastExceptionOperand();
    if (interpretor.lastExceptionOpcode() == 0xFE) {
      *exception_operand = choupi_vm_last_native_method_index();
    }
  }
  if (exception_stack_depth != nullptr) {
    *exception_stack_depth = interpretor.lastExceptionStackDepth();
  }
  if (exception_opcode_history != nullptr) {
    *exception_opcode_history = interpretor.lastExceptionOpcodeHistory();
  }
  if (exception_depth_history != nullptr) {
    *exception_depth_history = interpretor.lastExceptionDepthHistory();
  }
#ifndef PC_VERSION
  if (install_debug_stage != nullptr) {
    *install_debug_stage = choupi_install_debug_stage();
  }
  if (install_debug_context != nullptr) {
    *install_debug_context = choupi_install_debug_context();
  }
#else
  if (install_debug_stage != nullptr) *install_debug_stage = 0;
  if (install_debug_context != nullptr) *install_debug_context = 0;
#endif
  return applet_persisted || !interpretor.installAppletRegistered()
             ? status
             : 0x6f00;
}

#ifdef __cplusplus
}
#endif
