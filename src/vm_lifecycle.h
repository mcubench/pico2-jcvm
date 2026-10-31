#ifndef PICO2_JCVM_VM_LIFECYCLE_H
#define PICO2_JCVM_VM_LIFECYCLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t runtime(uint8_t package_id, uint8_t class_id, uint8_t method_id);

uint16_t runtime_install_method(
    uint8_t package_id, uint8_t applet_owner, uint16_t method_offset,
    const uint8_t *install_data, uint8_t install_length,
    const uint8_t *instance_aid, uint8_t instance_aid_length,
    uint8_t *applet_registered, uint16_t *applet_class_index,
    uint8_t *exception_id, uint8_t *exception_opcode, uint32_t *exception_pc,
    uint32_t *exception_prefix, uint16_t *exception_operand,
    uint8_t *exception_stack_depth, uint32_t *exception_opcode_history,
    uint32_t *exception_depth_history, uint8_t *install_debug_stage,
    uint32_t *install_debug_context);

uint8_t choupi_vm_restore_applet(uint8_t package_id, uint16_t class_index,
                                 const uint8_t *aid, uint8_t aid_length);
void choupi_vm_clear_selected_applet(void);
void choupi_vm_forget_selected_applet(void);
uint16_t choupi_vm_select_applet(void);
uint16_t choupi_vm_deselect_applet(uint8_t selecting_same_applet);
uint16_t choupi_vm_process_applet(const uint8_t *command,
                                  uint16_t command_length, uint8_t *response,
                                  uint16_t response_capacity,
                                  uint16_t *response_length);

#ifdef __cplusplus
}
#endif

#endif // PICO2_JCVM_VM_LIFECYCLE_H
