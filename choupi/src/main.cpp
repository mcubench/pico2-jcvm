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
#include "jc_handlers/flashmemory.hpp"

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
                                 uint16_t class_index) {
  try {
    auto context = std::make_unique<jcvm::Context>(
        0, static_cast<jcvm::jpackage_ID_t>(package_id));
    if (!context->restoreAppletInstance(
            static_cast<jcvm::jpackage_ID_t>(package_id),
            static_cast<jcvm::jclass_index_t>(class_index)) ||
        !context->hasRestoredAppletInstance()) {
      selected_applet_context.reset();
      return 1;
    }
    selected_applet_context = std::move(context);
    return 0;
  } catch (...) {
    selected_applet_context.reset();
    return 1;
  }
}

void choupi_vm_clear_selected_applet(void) {
  selected_applet_context.reset();
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
  uint32_t selected = 0;
  const uint16_t status = invoke_selected_applet_method(7, &selected);
  if (status != 0x9000) {
    return status;
  }
  return selected != 0 ? 0x9000 : 0x6999;
}

uint16_t choupi_vm_deselect_applet(void) {
  // Applet's internal public-method token 5 is deselect(); imported JC 3.0.5
  // tokens are remapped separately by Class_Handler for invokevirtual.
  const uint16_t status = invoke_selected_applet_method(5, nullptr);
  selected_applet_context.reset();
  return status;
}

uint16_t runtime(uint8_t id_package, uint8_t id_class, uint8_t id_method) {
  TRACE_JCVM_DEBUG("Starting JCVM");

  jcvm::japplet_ID_t id_applet = 0;

  // TODO: is a static method?
  jcvm::Interpretor interpretor(id_applet,
                                static_cast<jcvm::jpackage_ID_t>(id_package),
                                id_class, id_method, true);
  return interpretor.run();
}

uint16_t runtime_install_method(uint8_t id_package, uint16_t method_offset,
                                const uint8_t *install_data,
                                uint8_t install_length,
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
