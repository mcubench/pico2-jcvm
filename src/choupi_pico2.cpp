#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <malloc.h>
#include <unistd.h>

#include "ffi.h"
#include "jni.hpp"
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "tusb.h"
#include "reader_led.hpp"
#include "vm_lifecycle.h"
#include "vm_diagnostics.h"
#include "usb_task_budget.hpp"

extern "C" char __StackLimit;

extern "C" void choupi_ccid_task(void);
extern "C" void choupi_ccid_native_crypto_progress(void);
extern "C" void choupi_vm_native_progress(void);

extern "C" void choupi_watchdog_update(void) { watchdog_update(); }
extern "C" void choupi_native_crypto_progress(void) {
  choupi_vm_native_progress();
  watchdog_update();
  choupi_ccid_native_crypto_progress();
}
extern "C" uint32_t choupi_time_us_32(void) { return time_us_32(); }
extern "C" void choupi_install_trace(uint32_t pc, uint8_t opcode,
                                      uint32_t instruction_count) {
  watchdog_hw->scratch[0] = 0x494e5354; // "INST"
  watchdog_hw->scratch[1] = pc;
  watchdog_hw->scratch[2] = (instruction_count & 0x00ffffffu) |
                            (static_cast<uint32_t>(opcode) << 24);
  watchdog_hw->scratch[3] = 0;
  watchdog_hw->scratch[5] = 0;
}
extern "C" void choupi_install_stage(uint8_t stage) {
  watchdog_hw->scratch[3] = stage;
}
extern "C" void choupi_install_context(uint32_t context) {
  watchdog_hw->scratch[4] = context;
}
extern "C" uint8_t choupi_install_debug_stage(void) {
  return static_cast<uint8_t>(watchdog_hw->scratch[3]);
}
extern "C" uint32_t choupi_install_debug_context(void) {
  return watchdog_hw->scratch[4];
}
extern "C" void choupi_install_origin(uint32_t origin) {
  watchdog_hw->scratch[5] = origin;
}
namespace {

bool watchdog_recovery_pending = false;
bool watchdog_recovery_by_watchdog = false;
uint32_t watchdog_recovery_pc = 0;
uint32_t watchdog_recovery_info = 0;
uint32_t watchdog_recovery_stage = 0;
uint32_t watchdog_recovery_context = 0;
uint32_t watchdog_recovery_origin = 0;

} // namespace

extern "C" uint32_t choupi_available_transient_memory_bytes(void) {
#if defined(__GLIBC__) && \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
  const struct mallinfo2 allocator = mallinfo2();
#else
  const struct mallinfo allocator = mallinfo();
#endif
  const uintptr_t heap_end = reinterpret_cast<uintptr_t>(sbrk(0));
  const uintptr_t heap_limit = reinterpret_cast<uintptr_t>(&__StackLimit);
  const size_t uncommitted = heap_end < heap_limit ? heap_limit - heap_end : 0;
  const uint64_t available =
      static_cast<uint64_t>(allocator.fordblks) + uncommitted;
#ifdef CHOUPI_DEBUG_HEAP_GC
  std::printf(
      "CHOUPI:MEMORY transient arena=%llu used=%llu free=%llu keep=%llu "
      "brk=%08lX limit=%08lX gap=%lu available=%llu\r\n",
      static_cast<unsigned long long>(allocator.arena),
      static_cast<unsigned long long>(allocator.uordblks),
      static_cast<unsigned long long>(allocator.fordblks),
      static_cast<unsigned long long>(allocator.keepcost),
      static_cast<unsigned long>(heap_end),
      static_cast<unsigned long>(heap_limit),
      static_cast<unsigned long>(uncommitted),
      static_cast<unsigned long long>(available));
#endif
  return available > UINT32_MAX ? UINT32_MAX
                                : static_cast<uint32_t>(available);
}

int main_arm(void) {
  choupi_reader_led_init();
  // A stalled VM can stop servicing USB requests. Preserve and report its last
  // INSTALL opcode after a reset, even if BOOTROM/USB recovery replaced the
  // original watchdog reset-cause flag before this image started.
  watchdog_recovery_by_watchdog = watchdog_enable_caused_reboot();
  if (watchdog_hw->scratch[0] == 0x494e5354) {
    watchdog_recovery_pending = true;
    watchdog_recovery_pc = watchdog_hw->scratch[1];
    watchdog_recovery_info = watchdog_hw->scratch[2];
    watchdog_recovery_stage = watchdog_hw->scratch[3];
    watchdog_recovery_context = watchdog_hw->scratch[4];
    watchdog_recovery_origin = watchdog_hw->scratch[5];
    // Consume the retained marker after copying it to RAM. A later reset
    // should report a new VM trace, not repeat this diagnostic indefinitely.
    watchdog_hw->scratch[0] = 0;
  }

  stdio_init_all();
  for (unsigned int attempt = 0; attempt < 120; ++attempt) {
    // IRQ background servicing is disabled so TinyUSB and CCID stay on one
    // context. Drive enumeration here too; stdio output alone does not poll
    // TinyUSB before the CDC host has connected.
    choupi::service_usb_task();
    if (stdio_usb_connected()) break;
    printf("BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi\r\n");
    fflush(stdout);
    sleep_ms(250);
  }
  sleep_ms(500);
  printf("BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi\r\n");
  printf("CHOUPI:STAGE fs_init:start\r\n");
  fflush(stdout);
  if (fs_init() != 0) {
    printf("TEST:FAIL choupi_fs_init\r\n");
    for (;;) {
      tight_loop_contents();
    }
  }
  printf("CHOUPI:STAGE fs_init:done\r\n");
  fflush(stdout);

  // Refresh once per VM instruction. If a native operation or handler wedges,
  // the next boot enters BOOTSEL instead of returning to an unresponsive app.
  watchdog_enable(15000, true);
  if (!watchdog_recovery_pending) {
    watchdog_hw->scratch[0] = 0;
    watchdog_hw->scratch[1] = 0;
    watchdog_hw->scratch[2] = 0;
    watchdog_hw->scratch[3] = 0;
    watchdog_hw->scratch[4] = 0;
    watchdog_hw->scratch[5] = 0;
  }

  uint16_t status = 0x6F00;
  try {
    printf("CHOUPI:STAGE runtime:start\r\n");
    fflush(stdout);
    status = runtime(STARTING_JAVACARD_PACKAGE, STARTING_JAVACARD_CLASS,
                     STARTING_JAVACARD_METHOD);
    printf("CHOUPI:STAGE runtime:done\r\n");
    fflush(stdout);
  } catch (...) {
    printf("TEST:FAIL choupi_uncaught_exception\r\n");
  }
  printf("CHOUPI:STATUS sw=%04X\r\n", status);
  const bool passed = status == 0x9000;
  absolute_time_t next_report = make_timeout_time_ms(1000);
  for (;;) {
    // Keep TinyUSB and the custom CCID callbacks on one thread. In particular,
    // the main-loop CCID handler must queue responses from the same context
    // that owns usbd endpoint state.
    choupi::service_usb_task();
    choupi_ccid_task();
    choupi_ccid_native_progress_stats_t native_progress_stats{};
    if (choupi_ccid_debug_take_native_progress_stats(
            &native_progress_stats)) {
      const uint32_t average_call_us = native_progress_stats.calls == 0
          ? 0
          : native_progress_stats.total_us / native_progress_stats.calls;
      printf("CHOUPI:USB_PROGRESS seq=%u calls=%lu total_us=%lu "
             "max_call_us=%lu avg_call_us=%lu usb_calls=%lu "
             "usb_total_us=%lu usb_max_us=%lu events=%lu events_max=%lu "
             "extensions=%lu completed=%lu\r\n",
             static_cast<unsigned>(native_progress_stats.sequence),
             static_cast<unsigned long>(native_progress_stats.calls),
             static_cast<unsigned long>(native_progress_stats.total_us),
             static_cast<unsigned long>(native_progress_stats.max_call_us),
             static_cast<unsigned long>(average_call_us),
             static_cast<unsigned long>(native_progress_stats.usb_service_calls),
             static_cast<unsigned long>(native_progress_stats.usb_service_total_us),
             static_cast<unsigned long>(native_progress_stats.usb_service_max_us),
             static_cast<unsigned long>(native_progress_stats.usb_events),
             static_cast<unsigned long>(native_progress_stats.usb_events_max),
             static_cast<unsigned long>(native_progress_stats.extensions_submitted),
             static_cast<unsigned long>(native_progress_stats.extensions_completed));
      fflush(stdout);
    }
    choupi_reader_led_task();
    watchdog_update();
    if (time_reached(next_report)) {
      uint8_t ccid_stage = 0;
      uint8_t ccid_endpoint = 0;
      uint16_t ccid_rx_events = 0;
      uint16_t ccid_task_count = 0;
      uint8_t last_apdu_length = 0;
      uint8_t last_apdu_header[5] = {};
      uint8_t last_message_type = 0;
      uint8_t last_payload_length = 0;
      uint16_t xfr_block_count = 0;
      uint16_t out_xfer_errors = 0;
      uint16_t in_xfer_errors = 0;
      uint16_t out_rearm_errors = 0;
      uint16_t in_submit_errors = 0;
      uint16_t invalid_messages = 0;
      uint8_t last_xfer_result = 0;
      uint8_t last_unknown_message = 0;
      uint16_t unknown_message_count = 0;
      uint8_t recent_commands[12] = {};
      uint16_t command_count = 0;
      uint8_t response_apdu_header[5] = {};
      uint16_t response_length = 0;
      uint16_t response_status = 0;
      choupi_ccid_debug_state(&ccid_stage, &ccid_endpoint, &ccid_rx_events,
                              &ccid_task_count, &last_apdu_length,
                              last_apdu_header, &last_message_type,
                              &last_payload_length, &xfr_block_count,
                              &out_xfer_errors, &in_xfer_errors,
                              &out_rearm_errors, &in_submit_errors,
                              &invalid_messages, &last_xfer_result);
      choupi_ccid_debug_response(response_apdu_header, &response_length,
                                 &response_status);
      choupi_ccid_debug_unknown(&last_unknown_message,
                                &unknown_message_count);
      choupi_ccid_debug_recent(recent_commands, sizeof(recent_commands),
                               &command_count);
      printf("CHOUPI:CCID stage=%u endpoint=%02X rx_events=%u tasks=%u xfr=%u msg=%02X len=%u apdu=%u:%02X%02X%02X%02X%02X\r\n",
             static_cast<unsigned>(ccid_stage),
             static_cast<unsigned>(ccid_endpoint),
             static_cast<unsigned>(ccid_rx_events),
             static_cast<unsigned>(ccid_task_count),
             static_cast<unsigned>(xfr_block_count),
             static_cast<unsigned>(last_message_type),
             static_cast<unsigned>(last_payload_length),
             static_cast<unsigned>(last_apdu_length), last_apdu_header[0],
             last_apdu_header[1], last_apdu_header[2], last_apdu_header[3],
             last_apdu_header[4]);
      if (out_xfer_errors != 0 || in_xfer_errors != 0 ||
          out_rearm_errors != 0 || in_submit_errors != 0 ||
          invalid_messages != 0) {
        printf("CHOUPI:CCID_ERRORS out_xfer=%u in_xfer=%u out_rearm=%u "
               "in_submit=%u invalid=%u last_result=%u\r\n",
               static_cast<unsigned>(out_xfer_errors),
               static_cast<unsigned>(in_xfer_errors),
               static_cast<unsigned>(out_rearm_errors),
               static_cast<unsigned>(in_submit_errors),
               static_cast<unsigned>(invalid_messages),
               static_cast<unsigned>(last_xfer_result));
      }
      printf("CHOUPI:CCID_RESPONSE apdu=%02X%02X%02X%02X%02X len=%u sw=%04X\r\n",
             response_apdu_header[0], response_apdu_header[1],
             response_apdu_header[2], response_apdu_header[3],
             response_apdu_header[4], static_cast<unsigned>(response_length),
             static_cast<unsigned>(response_status));
      uint8_t response_data[64] = {};
      uint8_t response_data_length = 0;
      if (choupi_ccid_debug_take_response_data(
              response_data, sizeof(response_data), &response_data_length)) {
        printf("CHOUPI:CCID_RESPONSE_DATA len=%u hex=",
               static_cast<unsigned>(response_data_length));
        for (uint8_t index = 0; index < response_data_length; ++index) {
          printf("%02X", response_data[index]);
        }
        printf("\r\n");
      }
      if (unknown_message_count != 0) {
        printf("CHOUPI:CCID_UNKNOWN last=%02X count=%u\r\n",
               static_cast<unsigned>(last_unknown_message),
               static_cast<unsigned>(unknown_message_count));
      }
      if (command_count != 0) {
        printf("CHOUPI:CCID_COMMANDS count=%u recent=",
               static_cast<unsigned>(command_count));
        for (uint8_t index = 0; index < sizeof(recent_commands); ++index) {
          printf("%02X", static_cast<unsigned>(recent_commands[index]));
        }
        printf("\r\n");
      }
      printf("CHOUPI:STATUS sw=%04X\r\n", status);
      printf("TEST:%s choupi_runtime\r\n", passed ? "PASS" : "FAIL");
      printf("TEST:SUMMARY pass=%u fail=%u\r\n", passed ? 1u : 0u,
             passed ? 0u : 1u);
      if (!passed) {
        uint16_t runtime_status = 0;
        uint16_t exception_id = 0;
        uint8_t opcode = 0;
        uint32_t pc = 0;
        uint16_t stack_depth = 0;
        uint32_t opcode_history = 0;
        uint32_t depth_history = 0;
        uint8_t field_bounds_failure = 0;
        uint16_t field_index = 0;
        uint16_t field_count = 0;
        uint16_t field_package = 0;
        uint16_t field_class = 0;
        uint16_t field_value_ref = 0;
        choupi_vm_runtime_debug_state(&runtime_status, &exception_id,
                                     &opcode, &pc, &stack_depth,
                                     &opcode_history, &depth_history,
                                     &field_bounds_failure, &field_index,
                                     &field_count, &field_package,
                                     &field_class, &field_value_ref);
        printf("CHOUPI:VM_RUNTIME_FAILURE sw=%04X exception=%u opcode=%02X "
               "pc=%08lX stack=%u op_history=%08lX depth_history=%08lX\r\n",
               static_cast<unsigned>(runtime_status),
               static_cast<unsigned>(exception_id),
               static_cast<unsigned>(opcode), static_cast<unsigned long>(pc),
               static_cast<unsigned>(stack_depth),
               static_cast<unsigned long>(opcode_history),
               static_cast<unsigned long>(depth_history));
        if (field_bounds_failure != 0) {
          printf("CHOUPI:FIELD_BOUNDS index=%u count=%u package=%u class=%u "
                 "value_ref=%04X\r\n",
                 static_cast<unsigned>(field_index),
                 static_cast<unsigned>(field_count),
                 static_cast<unsigned>(field_package),
                 static_cast<unsigned>(field_class),
                 static_cast<unsigned>(field_value_ref));
        }
        uint8_t cp_recorded = 0;
        uint8_t cp_succeeded = 0;
        uint8_t cp_package = 0;
        uint16_t cp_index = 0;
        uint8_t cp_token = 0;
        uint8_t cp_exception = 0;
        choupi_vm_field_cp_state(&cp_recorded, &cp_succeeded, &cp_package,
                                 &cp_index, &cp_token, &cp_exception);
        if (cp_recorded != 0) {
          printf("CHOUPI:FIELD_CP_LOOKUP package=%u cp=%u succeeded=%u "
                 "token=%u exception=%u\r\n",
                 static_cast<unsigned>(cp_package),
                 static_cast<unsigned>(cp_index),
                 static_cast<unsigned>(cp_succeeded),
                 static_cast<unsigned>(cp_token),
                 static_cast<unsigned>(cp_exception));
        }
        uint8_t heap_ref_recorded = 0;
        uint16_t heap_ref = 0;
        uint16_t heap_offset = 0;
        uint8_t heap_is_array = 0;
        choupi_vm_field_heap_ref_state(&heap_ref_recorded, &heap_ref,
                                       &heap_offset, &heap_is_array);
        if (heap_ref_recorded != 0) {
          printf("CHOUPI:PUTFIELD_HEAP_REF object=%04X offset=%u is_array=%u\r\n",
                 static_cast<unsigned>(heap_ref),
                 static_cast<unsigned>(heap_offset),
                 static_cast<unsigned>(heap_is_array));
        }
        uint8_t restore_recorded = 0;
        uint16_t restore_package = 0;
        uint16_t restore_class = 0;
        uint16_t restore_index = 0;
        uint8_t restore_type = 0;
        uint8_t restore_stage = 0;
        uint8_t restore_exception = 0;
        choupi_vm_persistent_restore_failure_state(
            &restore_recorded, &restore_package, &restore_class,
            &restore_index, &restore_type, &restore_stage,
            &restore_exception);
        if (restore_recorded != 0) {
          printf("CHOUPI:PERSISTENT_RESTORE_FAIL package=%u class=%u "
                 "field=%u type=%u stage=%u exception=%u\r\n",
                 static_cast<unsigned>(restore_package),
                 static_cast<unsigned>(restore_class),
                 static_cast<unsigned>(restore_index),
                 static_cast<unsigned>(restore_type),
                 static_cast<unsigned>(restore_stage),
                 static_cast<unsigned>(restore_exception));
        }
        uint8_t putfield_stage = 0;
        uint16_t putfield_object = 0;
        uint16_t putfield_value = 0;
        uint8_t putfield_exception = 0;
        choupi_vm_putfield_stage_state(&putfield_stage, &putfield_object,
                                       &putfield_value,
                                       &putfield_exception);
        if (putfield_stage != 0) {
          printf("CHOUPI:PUTFIELD_STAGE stage=%u object=%04X value=%04X exception=%u\r\n",
                 static_cast<unsigned>(putfield_stage),
                 static_cast<unsigned>(putfield_object),
                 static_cast<unsigned>(putfield_value),
                 static_cast<unsigned>(putfield_exception));
        }
        uint8_t write_stage = 0;
        uint16_t write_package = 0;
        uint16_t write_class = 0;
        uint16_t write_index = 0;
        uint16_t write_value = 0;
        uint16_t write_count = 0;
        uint8_t write_flags = 0;
        uint8_t write_exception = 0;
        choupi_vm_field_write_state(
            &write_stage, &write_package, &write_class, &write_index,
            &write_value, &write_count, &write_flags, &write_exception);
        if (write_stage != 0) {
          printf("CHOUPI:FIELD_WRITE stage=%u package=%u class=%u index=%u "
                 "value=%04X count=%u flags=%02X exception=%u\r\n",
                 static_cast<unsigned>(write_stage),
                 static_cast<unsigned>(write_package),
                 static_cast<unsigned>(write_class),
                 static_cast<unsigned>(write_index),
                 static_cast<unsigned>(write_value),
                 static_cast<unsigned>(write_count),
                 static_cast<unsigned>(write_flags),
                 static_cast<unsigned>(write_exception));
        }
        uint8_t array_failure_recorded = 0;
        uint8_t array_failure_stage = 0;
        uint16_t array_failure_index = 0;
        uint16_t array_failure_size = 0;
        uint16_t array_failure_ref = 0;
        uint8_t array_failure_type = 0;
        uint8_t array_failure_exception = 0;
        choupi_vm_array_write_failure_state(
            &array_failure_recorded, &array_failure_stage,
            &array_failure_index, &array_failure_size, &array_failure_ref,
            &array_failure_type, &array_failure_exception);
        if (array_failure_recorded != 0) {
          printf("CHOUPI:ARRAY_WRITE_FAIL stage=%u index=%u size=%u "
                 "ref=%04X type=%u exception=%u\r\n",
                 static_cast<unsigned>(array_failure_stage),
                 static_cast<unsigned>(array_failure_index),
                 static_cast<unsigned>(array_failure_size),
                 static_cast<unsigned>(array_failure_ref),
                 static_cast<unsigned>(array_failure_type),
                 static_cast<unsigned>(array_failure_exception));
        }
      }
      if (watchdog_recovery_pending) {
        printf("CHOUPI:WATCHDOG_RECOVERY marker=494E5354 cause=%s pc=%08lX opcode=%02lX count=%lu stage=%lu context=%08lX origin=%08lX\r\n",
               watchdog_recovery_by_watchdog ? "watchdog" : "other",
               static_cast<unsigned long>(watchdog_recovery_pc),
               static_cast<unsigned long>(watchdog_recovery_info >> 24),
               static_cast<unsigned long>(watchdog_recovery_info & 0x00ffffffu),
               static_cast<unsigned long>(watchdog_recovery_stage),
               static_cast<unsigned long>(watchdog_recovery_context),
               static_cast<unsigned long>(watchdog_recovery_origin));
        watchdog_recovery_pending = false;
      }
      fflush(stdout);
      next_report = delayed_by_ms(next_report, 1000);
    }
    tight_loop_contents();
  }
}
