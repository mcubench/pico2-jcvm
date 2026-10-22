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
#include "ffi.h"

#include "jc_handlers/jc_class.hpp"
#include "jc_handlers/jc_cp.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_import.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_array_type.hpp"
#include "../../src/short_apdu.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

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
bool selected_applet_reselection_pending = false;
bool selected_applet_is_reselecting = false;
bool selected_applet_is_selecting = false;
bool selected_applet_select_process_pending = false;
bool selected_applet_reselection_process_pending = false;
pico2_jcvm::ShortApduResponseChain pending_apdu_response;

bool is_select_for_applet(const uint8_t *command, const uint16_t length,
                          const jcvm::Context &context) noexcept {
  uint8_t aid[16]{};
  uint8_t aid_length = 0;
  if (command == nullptr || length < 5 || command[0] != 0x00 ||
      command[1] != 0xa4 || command[2] != 0x04 ||
      (command[3] & 0xef) != 0 ||
      !context.copyAppletAID(aid, aid_length) || command[4] != aid_length ||
      (length != static_cast<uint16_t>(5 + aid_length) &&
       length != static_cast<uint16_t>(6 + aid_length))) {
    return false;
  }
  return std::memcmp(command + 5, aid, aid_length) == 0;
}

class SelectingAppletProcessScope {
public:
  SelectingAppletProcessScope(const uint8_t *command, const uint16_t length,
                              const jcvm::Context *context) noexcept {
    const bool was_pending = selected_applet_select_process_pending;
    selected_applet_select_process_pending = false;
    selected_applet_is_selecting =
        was_pending && context != nullptr &&
        is_select_for_applet(command, length, *context);
    selected_applet_is_reselecting =
        selected_applet_is_selecting &&
        selected_applet_reselection_process_pending;
    selected_applet_reselection_process_pending = false;
  }

  ~SelectingAppletProcessScope() {
    selected_applet_is_selecting = false;
    selected_applet_is_reselecting = false;
  }

  SelectingAppletProcessScope(const SelectingAppletProcessScope &) = delete;
  SelectingAppletProcessScope &operator=(
      const SelectingAppletProcessScope &) = delete;
};

class APDUProcessScope {
public:
  explicit APDUProcessScope(jcvm::Context &context) noexcept
      : context_(context) {
    this->context_.setAPDUProcessActive(true);
  }

  ~APDUProcessScope() { this->context_.setAPDUProcessActive(false); }

  APDUProcessScope(const APDUProcessScope &) = delete;
  APDUProcessScope &operator=(const APDUProcessScope &) = delete;

private:
  jcvm::Context &context_;
};

#ifndef PC_VERSION
void enumerate_selected_applet_roots(
    jcvm::Heap::ReferenceVisitor visitor, void *visitor_context,
    void *context) {
  static_cast<const jcvm::Context *>(context)->visitRootReferences(
      visitor, visitor_context);
}
#endif

#ifdef PC_VERSION
void trace_heap_after_apdu_if_enabled() {
  const char *trace_setting = std::getenv("PICO2_HEAP_TRACE");
  const char *collect_setting = std::getenv("PICO2_HEAP_GC");
  const bool collect_enabled = collect_setting != nullptr &&
                               std::strcmp(collect_setting, "1") == 0;
  const bool trace_enabled = trace_setting != nullptr;
  if ((!trace_enabled && !collect_enabled) || !selected_applet_context) {
    return;
  }

  unsigned long repeat_count = 1;
  if (trace_enabled) {
    char *end = nullptr;
    repeat_count = std::strtoul(trace_setting, &end, 10);
    if (end == trace_setting || *end != '\0' || repeat_count == 0) {
      repeat_count = 1;
    }
  }
  if (repeat_count > 4) {
    repeat_count = 4;
  }

  jcvm::Context &context = *selected_applet_context;
  if (!context.getStack().empty()) {
    std::fprintf(stderr, "HEAP_TRACE:SKIP reason=active_java_frames\n");
    return;
  }

  for (unsigned long pass = 1; pass <= repeat_count; ++pass) {
    try {
      std::vector<jcvm::jref_t> roots;
      context.appendRootReferences(roots);
      const auto summary = collect_enabled
                               ? context.getHeap().collectUnreachableFrom(roots)
                               : context.getHeap().traceReachableFrom(roots);
      std::fprintf(stderr,
                   "HEAP_TRACE: pass=%lu roots=%u arrays=%u/%u "
                   "instances=%u/%u reclaimed_arrays=%u "
                   "reclaimed_instances=%u invalid=%u edge_failures=%u "
                   "complete=%u\n",
                   pass, static_cast<unsigned>(roots.size()),
                   static_cast<unsigned>(summary.arrays_reachable),
                   static_cast<unsigned>(summary.arrays_total),
                   static_cast<unsigned>(summary.instances_reachable),
                   static_cast<unsigned>(summary.instances_total),
                   static_cast<unsigned>(summary.arrays_reclaimed),
                   static_cast<unsigned>(summary.instances_reclaimed),
                   static_cast<unsigned>(summary.invalid_references),
                   static_cast<unsigned>(summary.edge_load_failures),
                   summary.complete ? 1u : 0u);
    } catch (...) {
      std::fprintf(stderr, "HEAP_TRACE:ERROR pass=%lu root_enumeration_failed\n",
                   pass);
      break;
    }
  }
}
#else
void collect_heap_after_apdu() {
  if (!selected_applet_context) {
    return;
  }

#ifdef CHOUPI_DEBUG_HEAP_GC
  static uint32_t collection_sequence = 0;
#endif

  jcvm::Context &context = *selected_applet_context;
  if (!context.getStack().empty()) {
    std::printf("CHOUPI:HEAP_GC_SKIP reason=active_java_frames\r\n");
    return;
  }

  try {
    const auto summary = context.getHeap().collectUnreachableFrom(
        enumerate_selected_applet_roots, &context);
#ifdef CHOUPI_DEBUG_HEAP_GC
    ++collection_sequence;
    if ((collection_sequence % 32) == 0 || !summary.complete) {
      std::printf(
          "CHOUPI:HEAP_GC summary seq=%lu arrays=%lu/%lu reclaimed=%lu "
          "instances=%lu/%lu reclaimed=%lu roots_static=%lu "
          "roots_runtime=%lu tagged_arrays=%lu tagged_instances=%lu "
          "invalid=%lu edge_failures=%lu\r\n",
          static_cast<unsigned long>(collection_sequence),
          static_cast<unsigned long>(summary.arrays_reachable),
          static_cast<unsigned long>(summary.arrays_total),
          static_cast<unsigned long>(summary.arrays_reclaimed),
          static_cast<unsigned long>(summary.instances_reachable),
          static_cast<unsigned long>(summary.instances_total),
          static_cast<unsigned long>(summary.instances_reclaimed),
          static_cast<unsigned long>(summary.static_roots),
          static_cast<unsigned long>(summary.runtime_roots),
          static_cast<unsigned long>(summary.persistent_tagged_arrays),
          static_cast<unsigned long>(summary.persistent_tagged_instances),
          static_cast<unsigned long>(summary.invalid_references),
          static_cast<unsigned long>(summary.edge_load_failures));
      std::fflush(stdout);
    }
#endif
    if (!summary.complete) {
      std::printf(
          "CHOUPI:HEAP_GC_SKIP invalid=%u edge_failures=%u\r\n",
          static_cast<unsigned>(summary.invalid_references),
          static_cast<unsigned>(summary.edge_load_failures));
    }
  } catch (...) {
    std::printf("CHOUPI:HEAP_GC_SKIP reason=root_enumeration_failed\r\n");
  }
}
#endif

struct RuntimeDiagnostics {
  uint16_t status = 0x6f00;
  uint16_t exception_id = 0;
  uint16_t stack_depth = 0;
  uint8_t opcode = 0;
  uint32_t pc = 0;
  uint32_t opcode_history = 0;
  uint32_t depth_history = 0;
  bool field_bounds_failure = false;
  uint16_t field_index = 0;
  uint16_t field_count = 0;
  uint16_t field_package = 0;
  uint16_t field_class = 0;
  uint16_t field_value_ref = 0;
  bool field_cp_lookup_recorded = false;
  bool field_cp_lookup_succeeded = false;
  uint16_t field_cp_index = 0;
  uint8_t field_cp_package = 0;
  uint8_t field_cp_token = 0;
  uint8_t field_cp_exception = 0;
  bool field_heap_ref_recorded = false;
  uint16_t field_heap_ref = 0;
  uint16_t field_heap_offset = 0;
  bool field_heap_is_array = false;
  bool persistent_restore_failure = false;
  uint16_t persistent_restore_index = 0;
  uint16_t persistent_restore_package = 0;
  uint16_t persistent_restore_class = 0;
  uint8_t persistent_restore_type = 0;
  uint8_t persistent_restore_stage = 0;
  uint8_t persistent_restore_exception = 0;
  uint8_t putfield_stage = 0;
  uint16_t putfield_stage_object = 0;
  uint16_t putfield_stage_value = 0;
  uint8_t putfield_stage_exception = 0;
  uint8_t field_write_stage = 0;
  uint16_t field_write_package = 0;
  uint16_t field_write_class = 0;
  uint16_t field_write_index = 0;
  uint16_t field_write_value = 0;
  uint16_t field_write_count = 0;
  uint8_t field_write_flags = 0;
  uint8_t field_write_exception = 0;
  bool array_write_failure = false;
  uint8_t array_write_stage = 0;
  uint16_t array_write_index = 0;
  uint16_t array_write_size = 0;
  uint16_t array_write_ref = 0;
  uint8_t array_write_type = 0;
  uint8_t array_write_exception_id = 0;
};
RuntimeDiagnostics runtime_diagnostics;
}

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PC_VERSION
uint8_t choupi_pc_initialize_storage(void);
#endif

uint16_t choupi_vm_last_native_method_index();
#ifndef PC_VERSION
void choupi_install_stage(uint8_t stage);
void choupi_install_context(uint32_t context);
uint8_t choupi_install_debug_stage(void);
uint32_t choupi_install_debug_context(void);
#endif

void choupi_ccid_task(void);

void choupi_vm_runtime_debug_state(uint16_t *status, uint16_t *exception_id,
                                   uint8_t *opcode, uint32_t *pc,
                                   uint16_t *stack_depth,
                                   uint32_t *opcode_history,
                                   uint32_t *depth_history,
                                   uint8_t *field_bounds_failure,
                                   uint16_t *field_index,
                                   uint16_t *field_count,
                                   uint16_t *field_package,
                                   uint16_t *field_class,
                                   uint16_t *field_value_ref) {
  if (status != nullptr) *status = runtime_diagnostics.status;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.exception_id;
  if (opcode != nullptr) *opcode = runtime_diagnostics.opcode;
  if (pc != nullptr) *pc = runtime_diagnostics.pc;
  if (stack_depth != nullptr) *stack_depth = runtime_diagnostics.stack_depth;
  if (opcode_history != nullptr)
    *opcode_history = runtime_diagnostics.opcode_history;
  if (depth_history != nullptr)
    *depth_history = runtime_diagnostics.depth_history;
  if (field_bounds_failure != nullptr)
    *field_bounds_failure = runtime_diagnostics.field_bounds_failure ? 1 : 0;
  if (field_index != nullptr) *field_index = runtime_diagnostics.field_index;
  if (field_count != nullptr) *field_count = runtime_diagnostics.field_count;
  if (field_package != nullptr)
    *field_package = runtime_diagnostics.field_package;
  if (field_class != nullptr) *field_class = runtime_diagnostics.field_class;
  if (field_value_ref != nullptr)
    *field_value_ref = runtime_diagnostics.field_value_ref;
}

void choupi_vm_record_field_bounds(uint16_t field_index, uint16_t field_count,
                                  uint16_t field_package,
                                  uint16_t field_class,
                                  uint16_t field_value_ref) {
  runtime_diagnostics.field_bounds_failure = true;
  runtime_diagnostics.field_index = field_index;
  runtime_diagnostics.field_count = field_count;
  runtime_diagnostics.field_package = field_package;
  runtime_diagnostics.field_class = field_class;
  runtime_diagnostics.field_value_ref = field_value_ref;
}

void choupi_vm_record_field_cp_lookup(uint8_t package_id, uint16_t cp_index,
                                      uint8_t succeeded, uint8_t token,
                                      uint8_t exception_id) {
  runtime_diagnostics.field_cp_lookup_recorded = true;
  runtime_diagnostics.field_cp_lookup_succeeded = succeeded != 0;
  runtime_diagnostics.field_cp_package = package_id;
  runtime_diagnostics.field_cp_index = cp_index;
  runtime_diagnostics.field_cp_token = token;
  runtime_diagnostics.field_cp_exception = exception_id;
}

void choupi_vm_field_cp_state(uint8_t *recorded, uint8_t *succeeded,
                              uint8_t *package_id, uint16_t *cp_index,
                              uint8_t *token, uint8_t *exception_id) {
  if (recorded != nullptr)
    *recorded = runtime_diagnostics.field_cp_lookup_recorded ? 1 : 0;
  if (succeeded != nullptr)
    *succeeded = runtime_diagnostics.field_cp_lookup_succeeded ? 1 : 0;
  if (package_id != nullptr)
    *package_id = runtime_diagnostics.field_cp_package;
  if (cp_index != nullptr) *cp_index = runtime_diagnostics.field_cp_index;
  if (token != nullptr) *token = runtime_diagnostics.field_cp_token;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.field_cp_exception;
}

void choupi_vm_record_field_heap_ref(uint16_t objectref, uint16_t offset,
                                     uint8_t is_array) {
  runtime_diagnostics.field_heap_ref_recorded = true;
  runtime_diagnostics.field_heap_ref = objectref;
  runtime_diagnostics.field_heap_offset = offset;
  runtime_diagnostics.field_heap_is_array = is_array != 0;
}

void choupi_vm_field_heap_ref_state(uint8_t *recorded, uint16_t *objectref,
                                    uint16_t *offset, uint8_t *is_array) {
  if (recorded != nullptr)
    *recorded = runtime_diagnostics.field_heap_ref_recorded ? 1 : 0;
  if (objectref != nullptr) *objectref = runtime_diagnostics.field_heap_ref;
  if (offset != nullptr) *offset = runtime_diagnostics.field_heap_offset;
  if (is_array != nullptr)
    *is_array = runtime_diagnostics.field_heap_is_array ? 1 : 0;
}

void choupi_vm_record_persistent_restore_failure(uint16_t package_id,
                                                uint16_t class_index,
                                                uint16_t field_index,
                                                uint8_t field_type,
                                                uint8_t stage,
                                                uint8_t exception_id) {
  runtime_diagnostics.persistent_restore_failure = true;
  runtime_diagnostics.persistent_restore_package = package_id;
  runtime_diagnostics.persistent_restore_class = class_index;
  runtime_diagnostics.persistent_restore_index = field_index;
  runtime_diagnostics.persistent_restore_type = field_type;
  runtime_diagnostics.persistent_restore_stage = stage;
  runtime_diagnostics.persistent_restore_exception = exception_id;
}

void choupi_vm_persistent_restore_failure_state(
    uint8_t *recorded, uint16_t *package_id, uint16_t *class_index,
    uint16_t *field_index, uint8_t *field_type, uint8_t *stage,
    uint8_t *exception_id) {
  if (recorded != nullptr)
    *recorded = runtime_diagnostics.persistent_restore_failure ? 1 : 0;
  if (package_id != nullptr)
    *package_id = runtime_diagnostics.persistent_restore_package;
  if (class_index != nullptr)
    *class_index = runtime_diagnostics.persistent_restore_class;
  if (field_index != nullptr)
    *field_index = runtime_diagnostics.persistent_restore_index;
  if (field_type != nullptr)
    *field_type = runtime_diagnostics.persistent_restore_type;
  if (stage != nullptr) *stage = runtime_diagnostics.persistent_restore_stage;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.persistent_restore_exception;
}

void choupi_vm_record_putfield_stage(uint8_t stage, uint16_t objectref,
                                     uint16_t value, uint8_t exception_id) {
  runtime_diagnostics.putfield_stage = stage;
  runtime_diagnostics.putfield_stage_object = objectref;
  runtime_diagnostics.putfield_stage_value = value;
  runtime_diagnostics.putfield_stage_exception = exception_id;
}

void choupi_vm_putfield_stage_state(uint8_t *stage, uint16_t *objectref,
                                    uint16_t *value, uint8_t *exception_id) {
  if (stage != nullptr) *stage = runtime_diagnostics.putfield_stage;
  if (objectref != nullptr)
    *objectref = runtime_diagnostics.putfield_stage_object;
  if (value != nullptr) *value = runtime_diagnostics.putfield_stage_value;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.putfield_stage_exception;
}

void choupi_vm_record_field_write(uint8_t stage, uint16_t package_id,
                                  uint16_t class_index, uint16_t field_index,
                                  uint16_t value, uint16_t field_count,
                                  uint8_t flags, uint8_t exception_id) {
  runtime_diagnostics.field_write_stage = stage;
  runtime_diagnostics.field_write_package = package_id;
  runtime_diagnostics.field_write_class = class_index;
  runtime_diagnostics.field_write_index = field_index;
  runtime_diagnostics.field_write_value = value;
  runtime_diagnostics.field_write_count = field_count;
  runtime_diagnostics.field_write_flags = flags;
  runtime_diagnostics.field_write_exception = exception_id;
}

void choupi_vm_field_write_state(uint8_t *stage, uint16_t *package_id,
                                 uint16_t *class_index, uint16_t *field_index,
                                 uint16_t *value, uint16_t *field_count,
                                 uint8_t *flags, uint8_t *exception_id) {
  if (stage != nullptr) *stage = runtime_diagnostics.field_write_stage;
  if (package_id != nullptr)
    *package_id = runtime_diagnostics.field_write_package;
  if (class_index != nullptr)
    *class_index = runtime_diagnostics.field_write_class;
  if (field_index != nullptr)
    *field_index = runtime_diagnostics.field_write_index;
  if (value != nullptr) *value = runtime_diagnostics.field_write_value;
  if (field_count != nullptr)
    *field_count = runtime_diagnostics.field_write_count;
  if (flags != nullptr) *flags = runtime_diagnostics.field_write_flags;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.field_write_exception;
}

void choupi_vm_record_array_write_failure(uint8_t stage, uint16_t index,
                                          uint16_t size, uint16_t ref,
                                          uint8_t type,
                                          uint8_t exception_id) {
  if (runtime_diagnostics.array_write_failure) return;
  runtime_diagnostics.array_write_failure = true;
  runtime_diagnostics.array_write_stage = stage;
  runtime_diagnostics.array_write_index = index;
  runtime_diagnostics.array_write_size = size;
  runtime_diagnostics.array_write_ref = ref;
  runtime_diagnostics.array_write_type = type;
  runtime_diagnostics.array_write_exception_id = exception_id;
}

void choupi_vm_array_write_failure_state(uint8_t *recorded, uint8_t *stage,
                                         uint16_t *index, uint16_t *size,
                                         uint16_t *ref, uint8_t *type,
                                         uint8_t *exception_id) {
  if (recorded != nullptr)
    *recorded = runtime_diagnostics.array_write_failure ? 1 : 0;
  if (stage != nullptr) *stage = runtime_diagnostics.array_write_stage;
  if (index != nullptr) *index = runtime_diagnostics.array_write_index;
  if (size != nullptr) *size = runtime_diagnostics.array_write_size;
  if (ref != nullptr) *ref = runtime_diagnostics.array_write_ref;
  if (type != nullptr) *type = runtime_diagnostics.array_write_type;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.array_write_exception_id;
}

uint8_t choupi_vm_restore_applet(uint8_t package_id,
                                 uint16_t class_index, const uint8_t *aid,
                                 uint8_t aid_length) {
  pending_apdu_response.queue(nullptr, 0);
  selected_applet_is_selecting = false;
  selected_applet_select_process_pending = false;
  selected_applet_reselection_process_pending = false;
  try {
    if (selected_applet_context &&
        selected_applet_context->hasRestoredAppletInstance()) {
      uint8_t retained_aid[16]{};
      uint8_t retained_aid_length = 0;
      const jcvm::jref_t applet_ref =
          selected_applet_context->getRestoredAppletRef();
      const auto applet =
          selected_applet_context->getHeap().getInstance(applet_ref);
      if (applet->getPackageID() == package_id &&
          applet->getClassIndex() == class_index &&
          selected_applet_context->copyAppletAID(retained_aid,
                                                 retained_aid_length) &&
          retained_aid_length == aid_length &&
          std::memcmp(retained_aid, aid, aid_length) == 0) {
        selected_applet_is_active = false;
        return 0;
      }
    }

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
  selected_applet_reselection_pending = false;
  selected_applet_is_reselecting = false;
  selected_applet_is_selecting = false;
  selected_applet_select_process_pending = false;
  selected_applet_reselection_process_pending = false;
  if (selected_applet_context) {
    selected_applet_context->setAppletActive(false);
    selected_applet_context->getHeap().clearTransientArrays(
        jcvm::ClearEvent::CLEAR_ON_RESET);
  }
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
  selected_applet_context->resetSensitiveResult();
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
  selected_applet_select_process_pending = false;
  uint32_t selected = 0;
  selected_applet_reselection_process_pending =
      selected_applet_reselection_pending;
  selected_applet_is_reselecting =
      selected_applet_reselection_process_pending;
  selected_applet_reselection_pending = false;
  selected_applet_is_selecting = true;
  selected_applet_context->setAppletActive(true);
  const uint16_t status = invoke_selected_applet_method(6, &selected);
  selected_applet_is_selecting = false;
  selected_applet_is_reselecting = false;
  if (status != 0x9000) {
    selected_applet_context->setAppletActive(false);
    selected_applet_context.reset();
    selected_applet_is_active = false;
    selected_applet_reselection_process_pending = false;
    return status;
  }
  if (selected == 0) {
    selected_applet_context->setAppletActive(false);
    selected_applet_context.reset();
    selected_applet_is_active = false;
    selected_applet_reselection_process_pending = false;
    return 0x6999;
  }
  selected_applet_is_active = true;
  selected_applet_select_process_pending = true;
  return 0x9000;
}

extern "C" uint16_t choupi_vm_deselect_applet(
    const uint8_t selecting_same_applet) {
  pending_apdu_response.queue(nullptr, 0);
  selected_applet_reselection_pending = false;
  selected_applet_is_reselecting = false;
  selected_applet_is_selecting = false;
  selected_applet_select_process_pending = false;
  selected_applet_reselection_process_pending = false;
  if (!selected_applet_context || !selected_applet_is_active) {
    selected_applet_is_active = false;
    return 0x9000;
  }
  const uint16_t status = invoke_selected_applet_method(4, nullptr);
  selected_applet_context->setAppletActive(false);
  selected_applet_context->getHeap().clearTransientArrays(
      jcvm::ClearEvent::CLEAR_ON_DESELECT);
  selected_applet_is_active = false;
  selected_applet_reselection_pending = selecting_same_applet != 0 &&
                                         status == 0x9000;
  return status;
}

extern "C" uint8_t choupi_vm_is_reselecting_applet(void) {
  return selected_applet_is_reselecting ? 1 : 0;
}

extern "C" uint8_t choupi_vm_is_selecting_applet(void) {
  return selected_applet_is_selecting ? 1 : 0;
}

uint16_t choupi_vm_process_applet(const uint8_t *command,
                                 uint16_t command_length,
                                 uint8_t *response,
                                 uint16_t response_capacity,
                                 uint16_t *response_length) {
#ifdef PC_VERSION
  const char *apdu_trace_setting = std::getenv("CHOUPI_VM_TRACE_APDU");
  const bool apdu_trace = apdu_trace_setting != nullptr &&
                          std::strcmp(apdu_trace_setting, "1") == 0;
  if (apdu_trace) {
    std::fprintf(stderr,
                 "CHOUPI:APDU_PROCESS stage=begin len=%u ins=%02X\n",
                 static_cast<unsigned>(command_length),
                 command != nullptr && command_length > 1 ? command[1] : 0);
  }
#endif
#ifndef PC_VERSION
  std::printf("CHOUPI:APDU_PROCESS stage=begin\r\n");
  std::fflush(stdout);
#endif
  SelectingAppletProcessScope selection_scope(
      command, command_length,
      selected_applet_context ? selected_applet_context.get() : nullptr);
  if (selected_applet_context) {
    selected_applet_context->resetSensitiveResult();
  }
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
#ifndef PC_VERSION
      std::printf("CHOUPI:APDU_PROCESS stage=no_active_applet\r\n");
      std::fflush(stdout);
#endif
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
#ifdef PC_VERSION
    if (apdu_trace) {
      std::fprintf(stderr, "CHOUPI:APDU_PROCESS stage=buffer_ready\n");
    }
#endif

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

#ifdef PC_VERSION
    if (apdu_trace) {
      std::fprintf(stderr, "CHOUPI:APDU_PROCESS stage=method_lookup_done\n");
    }
#endif
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
#ifdef PC_VERSION
    if (apdu_trace) {
      std::fprintf(stderr,
                   "CHOUPI:APDU_PROCESS stage=interpreter_begin package=%u "
                   "method=%u\n",
                   static_cast<unsigned>(method.first.getPackageID()),
                   static_cast<unsigned>(method.second));
    }
#else
    std::printf("CHOUPI:APDU_PROCESS stage=interpreter_begin package=%u method=%u\r\n",
                static_cast<unsigned>(method.first.getPackageID()),
                static_cast<unsigned>(method.second));
    std::fflush(stdout);
#endif
    {
      APDUProcessScope process_scope(context);
      status = interpretor.run();
    }
#ifndef PC_VERSION
    std::printf("CHOUPI:APDU_PROCESS stage=interpreter_end sw=%04X\r\n",
                static_cast<unsigned>(status));
    std::fflush(stdout);
#endif
#ifdef PC_VERSION
    if (apdu_trace) {
      std::fprintf(stderr, "CHOUPI:APDU_PROCESS stage=interpreter_end sw=%04X\n",
                   static_cast<unsigned>(status));
    }
#endif
    if (status != 0x9000) {
      fprintf(stderr,
                   "VPCD:APPLET_PROCESS status=%04X exception=%u opcode=%02X "
                   "pc=%08X stack=%u op_history=%08X depth_history=%08X\n",
                   status, static_cast<unsigned>(interpretor.lastExceptionId()),
                   static_cast<unsigned>(interpretor.lastExceptionOpcode()),
                   static_cast<unsigned>(interpretor.lastExceptionPc()),
                   static_cast<unsigned>(interpretor.lastExceptionStackDepth()),
                   static_cast<unsigned>(interpretor.lastExceptionOpcodeHistory()),
                   static_cast<unsigned>(interpretor.lastExceptionDepthHistory()));
    }
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

#ifdef PC_VERSION
  // The try scope above owns the interpreter and temporary native references.
  // Run diagnostics only after those objects have been destroyed.
  trace_heap_after_apdu_if_enabled();
#else
  // Keep Java handles stable by collecting only after interpreter/native call
  // scopes have returned and the complete typed root walk has succeeded.
  std::printf("CHOUPI:APDU_PROCESS stage=gc_begin\r\n");
  std::fflush(stdout);
  collect_heap_after_apdu();
  std::printf("CHOUPI:APDU_PROCESS stage=gc_end\r\n");
  std::fflush(stdout);
#endif

  if (static_cast<uint32_t>(data_length) + 2 > response_capacity) {
    status = 0x6f00;
    data_length = 0;
  }
  response[data_length] = static_cast<uint8_t>(status >> 8);
  response[data_length + 1] = static_cast<uint8_t>(status);
  *response_length = static_cast<uint16_t>(data_length + 2);
#ifndef PC_VERSION
  std::printf("CHOUPI:APDU_PROCESS stage=end sw=%04X len=%u\r\n",
              static_cast<unsigned>(status),
              static_cast<unsigned>(*response_length));
  std::fflush(stdout);
#endif
  return status;
}

uint16_t runtime(uint8_t id_package, uint8_t id_class, uint8_t id_method) {
  TRACE_JCVM_DEBUG("Starting JCVM");
#ifdef PC_VERSION
  if (choupi_pc_initialize_storage() != 0) return 0x6f00;
#endif

  jcvm::japplet_ID_t id_applet = 0;

  // TODO: is a static method?
  jcvm::Interpretor interpretor(id_applet,
                                static_cast<jcvm::jpackage_ID_t>(id_package),
                                id_class, id_method, true);
  const uint16_t status = interpretor.run();
  runtime_diagnostics.status = status;
  runtime_diagnostics.exception_id = interpretor.lastExceptionId();
  runtime_diagnostics.opcode = interpretor.lastExceptionOpcode();
  runtime_diagnostics.pc = interpretor.lastExceptionPc();
  runtime_diagnostics.stack_depth = interpretor.lastExceptionStackDepth();
  runtime_diagnostics.opcode_history =
      interpretor.lastExceptionOpcodeHistory();
  runtime_diagnostics.depth_history =
      interpretor.lastExceptionDepthHistory();
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
  interpretor.getCurrentContext().resetSensitiveResult();
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
