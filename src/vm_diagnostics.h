#ifndef PICO2_JCVM_VM_DIAGNOSTICS_H
#define PICO2_JCVM_VM_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void choupi_vm_runtime_debug_state(
    uint16_t *status, uint16_t *exception_id, uint8_t *opcode, uint32_t *pc,
    uint16_t *stack_depth, uint32_t *opcode_history, uint32_t *depth_history,
    uint8_t *field_bounds_failure, uint16_t *field_index,
    uint16_t *field_count, uint16_t *field_package, uint16_t *field_class,
    uint16_t *field_value_ref);
void choupi_vm_field_cp_state(uint8_t *recorded, uint8_t *succeeded,
                              uint8_t *package_id, uint16_t *cp_index,
                              uint8_t *token, uint8_t *exception_id);
void choupi_vm_field_heap_ref_state(uint8_t *recorded, uint16_t *objectref,
                                    uint16_t *offset, uint8_t *is_array);
void choupi_vm_persistent_restore_failure_state(
    uint8_t *recorded, uint16_t *package_id, uint16_t *class_index,
    uint16_t *field_index, uint8_t *field_type, uint8_t *stage,
    uint8_t *exception_id);
void choupi_vm_putfield_stage_state(uint8_t *stage, uint16_t *objectref,
                                    uint16_t *value, uint8_t *exception_id);
void choupi_vm_field_write_state(uint8_t *stage, uint16_t *package_id,
                                 uint16_t *class_index, uint16_t *field_index,
                                 uint16_t *value, uint16_t *field_count,
                                 uint8_t *flags, uint8_t *exception_id);
void choupi_vm_array_write_failure_state(
    uint8_t *recorded, uint8_t *stage, uint16_t *index, uint16_t *size,
    uint16_t *ref, uint8_t *type, uint8_t *exception_id);

void choupi_vm_record_field_bounds(uint16_t field_index, uint16_t field_count,
                                   uint16_t field_package,
                                   uint16_t field_class,
                                   uint16_t field_value_ref);
void choupi_vm_record_field_cp_lookup(uint8_t package_id, uint16_t cp_index,
                                     uint8_t succeeded, uint8_t token,
                                     uint8_t exception_id);
void choupi_vm_record_field_heap_ref(uint16_t objectref, uint16_t offset,
                                     uint8_t is_array);
void choupi_vm_record_persistent_restore_failure(
    uint16_t package_id, uint16_t class_index, uint16_t field_index,
    uint8_t field_type, uint8_t stage, uint8_t exception_id);
void choupi_vm_record_putfield_stage(uint8_t stage, uint16_t objectref,
                                     uint16_t value, uint8_t exception_id);
void choupi_vm_record_field_write(uint8_t stage, uint16_t package_id,
                                  uint16_t class_index, uint16_t field_index,
                                  uint16_t value, uint16_t field_count,
                                  uint8_t flags, uint8_t exception_id);
void choupi_vm_record_array_write_failure(uint8_t stage, uint16_t index,
                                          uint16_t size, uint16_t ref,
                                          uint8_t type, uint8_t exception_id);

void choupi_ccid_debug_state(uint8_t *stage, uint8_t *endpoint,
                             uint16_t *rx_events, uint16_t *task_count,
                             uint8_t *last_apdu_length,
                             uint8_t *last_apdu_header,
                             uint8_t *last_message_type,
                             uint8_t *last_payload_length,
                             uint16_t *xfr_block_count,
                             uint16_t *out_xfer_errors,
                             uint16_t *in_xfer_errors,
                             uint16_t *out_rearm_errors,
                             uint16_t *in_submit_errors,
                             uint16_t *invalid_messages,
                             uint8_t *last_xfer_result);
typedef struct choupi_ccid_native_progress_stats {
  uint32_t calls;
  uint32_t total_us;
  uint32_t max_call_us;
  uint32_t usb_service_calls;
  uint32_t usb_service_total_us;
  uint32_t usb_service_max_us;
  uint32_t usb_events;
  uint32_t usb_events_max;
  uint32_t extensions_submitted;
  uint32_t extensions_completed;
  uint8_t sequence;
} choupi_ccid_native_progress_stats_t;
bool choupi_ccid_debug_take_native_progress_stats(
    choupi_ccid_native_progress_stats_t *stats);
void choupi_ccid_debug_response(uint8_t *apdu_header,
                                uint16_t *response_length,
                                uint16_t *status_word);
bool choupi_ccid_debug_take_response_data(uint8_t *data, uint8_t capacity,
                                          uint8_t *length);
void choupi_ccid_debug_unknown(uint8_t *last_message,
                               uint16_t *message_count);
void choupi_ccid_debug_recent(uint8_t *commands, uint8_t command_capacity,
                              uint16_t *command_count);

#ifndef PC_VERSION
void choupi_install_trace(uint32_t pc, uint8_t opcode,
                          uint32_t instruction_count);
void choupi_install_stage(uint8_t stage);
void choupi_install_context(uint32_t context);
void choupi_install_origin(uint32_t origin);
uint8_t choupi_install_debug_stage(void);
uint32_t choupi_install_debug_context(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // PICO2_JCVM_VM_DIAGNOSTICS_H
