#include <cstdint>
#include <cstdio>
#include <cstring>

#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "pico/unique_id.h"
#include "tusb.h"
#include "scp03.hpp"
#include "apdu_status.hpp"
#include "cap_load.hpp"
#include "gp_registry.hpp"
#include "gp_delete.hpp"
#include "gp_get_data.hpp"
#include "cap_store.hpp"
#include "reader_led.hpp"

extern "C" uint8_t choupi_vm_find_applet(const uint8_t *aid,
                                           uint8_t aid_length,
                                           uint8_t *package_id,
                                           uint16_t *install_offset);
extern "C" uint8_t choupi_vm_is_dynamic_package(uint8_t package_id);
extern "C" uint8_t choupi_cap_package_applet_aid(
    uint8_t package_id, uint8_t applet_index, uint8_t *aid,
    uint8_t *aid_length);
extern "C" uint8_t choupi_vm_restore_applet(uint8_t package_id,
                                            uint16_t class_index,
                                            const uint8_t *aid,
                                            uint8_t aid_length);
extern "C" void choupi_vm_clear_selected_applet(void);
extern "C" uint16_t choupi_vm_select_applet(void);
extern "C" uint16_t choupi_vm_deselect_applet(uint8_t selecting_same_applet);
extern "C" uint16_t choupi_vm_process_applet(const uint8_t *command,
                                              uint16_t command_length,
                                              uint8_t *response,
                                              uint16_t response_capacity,
                                              uint16_t *response_length);
extern "C" uint8_t fs_sync(void);

namespace {
constexpr uint8_t kCcidInterface = 2;
constexpr uint8_t kEndpointOut = 0x03;
constexpr uint8_t kEndpointIn = 0x83;
constexpr uint16_t kEndpointSize = 64;
constexpr size_t kMaxMessage = 320;
constexpr size_t kMaxCcidMessage = 300;
constexpr uint8_t kAtr[] = {0x3b, 0x80, 0x01, 0x81};

bool is_select_by_aid(const uint8_t *apdu, size_t apdu_length) {
  if (apdu == nullptr || apdu_length < 5 || apdu[0] != 0x00 ||
      apdu[1] != 0xa4 || apdu[2] != 0x04 || (apdu[3] & 0xef) != 0 ||
      apdu[4] < 5 || apdu[4] > 16) {
    return false;
  }
  return apdu_length == 5u + static_cast<size_t>(apdu[4]) ||
         apdu_length == 6u + static_cast<size_t>(apdu[4]);
}

// The standard CDC interface remains available for diagnostics. CCID is the
// third interface in this composite device and uses independent endpoints.
constexpr uint16_t kConfigLength = TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN +
                                  9 + 54 + 2 * 7;
const uint8_t kConfiguration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 3, 0, kConfigLength,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, kEndpointSize),
    9, TUSB_DESC_INTERFACE, kCcidInterface, 0, 2, 0x0b, 0, 0, 5,
    54, 0x21, 0x10, 0x01, 0x00, 0x07, 0x02, 0x00, 0x00, 0x00,
    0xa0, 0x0f, 0x00, 0x00, 0xa0, 0x0f, 0x00, 0x00, 0x00,
    0x80, 0x25, 0x00, 0x00, 0x80, 0x25, 0x00, 0x00, 0x00,
    0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, // dwMechanical
    // Fixed-parameter short-APDU exchange: automatic parameter negotiation
    // (0x42) plus short APDU level exchange (0x00020000). Advertising zero
    // here selects character-level transport, unlike our raw APDU XfrBlock.
    0x42, 0x00, 0x02, 0x00, // dwFeatures = 0x00020042
    0x0f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    7, TUSB_DESC_ENDPOINT, kEndpointOut, TUSB_XFER_BULK, 64, 0, 0,
    7, TUSB_DESC_ENDPOINT, kEndpointIn, TUSB_XFER_BULK, 64, 0, 0,
};
static_assert(sizeof(kConfiguration) == kConfigLength,
              "USB configuration descriptor length is inconsistent");

uint8_t rx_buffer[kMaxMessage];
uint8_t tx_buffer[kMaxMessage];
size_t rx_length = 0;
size_t rx_expected = 0;
uint8_t slot_state = 1; // present, inactive
uint8_t ep_out = 0;
uint8_t ep_in = 0;
volatile bool message_pending = false;
uint8_t pending_rhport = 0;
volatile uint8_t debug_stage = 0;
volatile uint8_t debug_endpoint = 0;
volatile uint16_t debug_rx_events = 0;
volatile uint16_t debug_task_count = 0;
volatile uint8_t debug_last_apdu_length = 0;
volatile uint8_t debug_last_apdu_header[5] = {};
volatile uint8_t debug_last_message_type = 0;
volatile uint8_t debug_last_payload_length = 0;
volatile uint16_t debug_xfr_block_count = 0;
volatile uint16_t debug_out_xfer_errors = 0;
volatile uint16_t debug_in_xfer_errors = 0;
volatile uint16_t debug_out_rearm_errors = 0;
volatile uint16_t debug_in_submit_errors = 0;
volatile uint16_t debug_invalid_messages = 0;
volatile uint8_t debug_last_xfer_result = 0;
volatile uint16_t debug_unknown_message_count = 0;
volatile uint8_t debug_last_unknown_message = 0;
volatile uint8_t debug_recent_commands[12] = {};
volatile uint8_t debug_recent_command_index = 0;
volatile uint16_t debug_command_count = 0;
Scp03Session secure_channel;
CapLoadSession cap_load_session;
bool get_status_first_seen = false;
uint16_t get_status_cursor = 0;

uint32_t read_le32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t build_dynamic_get_status_page(
    uint8_t p1, const uint8_t *search_data, size_t search_data_length,
    uint8_t *output, size_t capacity, size_t &output_length,
    bool &more_entries) noexcept {
  output_length = 0;
  more_entries = false;
  if (p1 == 0x80) {
    gp::RegistryView empty{};
    return gp::build_get_status_first_response(
        p1, search_data, search_data_length, empty, output, capacity,
        output_length);
  }
  if (p1 != 0x40 && p1 != 0x20 && p1 != 0x10) return 0x6a86;
  gp::AidSearch search;
  if (!gp::parse_aid_search(search_data, search_data_length, search))
    return 0x6a80;

  uint16_t ordinal = 0;
  for (uint8_t package_id = 56; package_id < 64; ++package_id) {
    uint32_t code_length = 0;
    if (cap_store_data_for_package(package_id, &code_length) == nullptr)
      continue;
    uint8_t package_aid[16]{};
    uint8_t package_aid_length = 0;
    if (!cap_store_package_aid(package_id, package_aid,
                               package_aid_length)) continue;
    const uint8_t instance_count =
        cap_store_package_instance_count(package_id);

    if (p1 == 0x40) {
      for (uint8_t index = 0; index < instance_count; ++index) {
        uint8_t applet_aid[16]{};
        uint8_t applet_aid_length = 0;
        uint8_t instance_aid[16]{};
        uint8_t instance_aid_length = 0;
        uint16_t class_index = 0xffff;
        if (!cap_store_package_instance_at(
                package_id, index, applet_aid, applet_aid_length,
                instance_aid, instance_aid_length, class_index) ||
            !gp::aid_matches_search(search, instance_aid,
                                    instance_aid_length)) continue;
        if (ordinal++ < get_status_cursor) continue;
        uint8_t entry[48]{};
        size_t entry_length = 0;
        if (!gp::append_registry_entry(
                entry, sizeof(entry), entry_length, instance_aid,
                instance_aid_length, package_aid, package_aid_length, 0x07))
          return 0x6f00;
        if (entry_length > capacity - output_length) {
          more_entries = true;
          return 0x6310;
        }
        std::memcpy(output + output_length, entry, entry_length);
        output_length += entry_length;
        ++get_status_cursor;
      }
    } else if (gp::aid_matches_search(search, package_aid,
                                      package_aid_length)) {
      uint8_t declared_applet_count = 0;
      if (p1 == 0x10) {
        while (declared_applet_count != UINT8_MAX) {
          uint8_t applet_aid[16]{};
          uint8_t applet_aid_length = 0;
          if (!choupi_cap_package_applet_aid(
                  package_id, declared_applet_count, applet_aid,
                  &applet_aid_length)) break;
          ++declared_applet_count;
        }
      }
      const uint8_t entry_count = p1 == 0x20
                                      ? 1
                                      : (declared_applet_count == 0
                                             ? 1
                                             : declared_applet_count);
      for (uint8_t index = 0; index < entry_count; ++index) {
        uint8_t applet_aid[16]{};
        uint8_t applet_aid_length = 0;
        if (p1 == 0x10 && declared_applet_count != 0 &&
            !choupi_cap_package_applet_aid(
                package_id, index, applet_aid, &applet_aid_length))
          continue;
        if (ordinal++ < get_status_cursor) continue;
        const bool include_module = p1 == 0x10 && declared_applet_count != 0;
        uint8_t entry[48]{};
        size_t entry_length = 0;
        if (!gp::append_load_file_entry(
                entry, sizeof(entry), entry_length, package_aid,
                package_aid_length, include_module ? applet_aid : nullptr,
                include_module ? applet_aid_length : 0, include_module))
          return 0x6f00;
        if (entry_length > capacity - output_length) {
          more_entries = true;
          return 0x6310;
        }
        std::memcpy(output + output_length, entry, entry_length);
        output_length += entry_length;
        ++get_status_cursor;
      }
    }
  }
  return 0x9000;
}

void queue_out(uint8_t rhport) {
  if (ep_out != 0 && !usbd_edpt_busy(rhport, ep_out)) {
    if (!usbd_edpt_xfer(rhport, ep_out, rx_buffer + rx_length,
                        static_cast<uint16_t>(kEndpointSize))) {
      ++debug_out_rearm_errors;
    }
  }
}

void send_response(uint8_t rhport, uint8_t type, const uint8_t *payload,
                   size_t payload_length, uint8_t status = 0,
                   uint8_t error = 0, uint8_t specific = 0) {
  if (payload_length + 10 > sizeof(tx_buffer)) {
    payload_length = 0;
    status = 0x40; // failed
    error = 0x01;  // command not supported
  }
  tx_buffer[0] = type;
  tx_buffer[1] = static_cast<uint8_t>(payload_length);
  tx_buffer[2] = static_cast<uint8_t>(payload_length >> 8);
  tx_buffer[3] = static_cast<uint8_t>(payload_length >> 16);
  tx_buffer[4] = static_cast<uint8_t>(payload_length >> 24);
  tx_buffer[5] = 0; // slot
  tx_buffer[6] = rx_buffer[6]; // sequence number
  tx_buffer[7] = status;
  tx_buffer[8] = error;
  tx_buffer[9] = specific;
  debug_stage = 6;
  if (payload_length != 0) {
    std::memcpy(tx_buffer + 10, payload, payload_length);
  }
  rx_length = 0;
  rx_expected = 0;
  if (!usbd_edpt_xfer(rhport, ep_in, tx_buffer,
                      static_cast<uint16_t>(payload_length + 10))) {
    ++debug_in_submit_errors;
  }
}

void process_message(uint8_t rhport) {
  const uint8_t command = rx_buffer[0];
  debug_recent_commands[debug_recent_command_index] = command;
  debug_recent_command_index =
      static_cast<uint8_t>((debug_recent_command_index + 1) %
                           sizeof(debug_recent_commands));
  ++debug_command_count;
  debug_last_message_type = command;
  debug_last_payload_length = static_cast<uint8_t>(
      rx_expected >= 10 && rx_expected - 10 < 256 ? rx_expected - 10 : 0xff);
  switch (command) {
  case 0x62: // PC_to_RDR_IccPowerOn
    // Treat every reader power-on as a card reset. This also drops the
    // previously selected applet and clears its CLEAR_ON_RESET arrays before
    // persistent applet state is restored for the next SELECT.
    choupi_vm_clear_selected_applet();
    slot_state = 0;
    secure_channel.reset();
    get_status_first_seen = false;
    cap_load_session.abortTransfer();
    cap_load_session.restoreInstalled();
    send_response(rhport, 0x80, kAtr, sizeof(kAtr));
    break;
  case 0x63: // PC_to_RDR_IccPowerOff
    (void)choupi_vm_deselect_applet(0);
    slot_state = 1;
    secure_channel.reset();
    get_status_first_seen = false;
    cap_load_session.abortTransfer();
    send_response(rhport, 0x81, nullptr, 0, slot_state);
    break;
  case 0x65: // PC_to_RDR_GetSlotStatus
    send_response(rhport, 0x81, nullptr, 0, slot_state);
    break;
  case 0x6f: { // PC_to_RDR_XfrBlock
    ++debug_xfr_block_count;
    const size_t apdu_length = rx_expected - 10;
    const uint8_t *apdu = rx_buffer + 10;
    debug_last_apdu_length =
        static_cast<uint8_t>(apdu_length < 5 ? apdu_length : 5);
    for (uint8_t index = 0; index < sizeof(debug_last_apdu_header); ++index) {
      debug_last_apdu_header[index] =
          index < debug_last_apdu_length ? apdu[index] : 0;
    }
    uint8_t response[96] = {0x6d, 0x00}; // instruction not supported
    size_t response_length = 2;
    if (apdu_length >= 2 && apdu[0] == 0x00 && apdu[1] == 0xa4) {
      std::printf("CHOUPI:SELECT_RX len=%u bytes=",
                  static_cast<unsigned>(apdu_length));
      const size_t shown = apdu_length < 16 ? apdu_length : 16;
      for (size_t index = 0; index < shown; ++index) {
        std::printf("%02X", static_cast<unsigned>(apdu[index]));
      }
      std::printf("\r\n");
      std::fflush(stdout);
    }
    if (apdu_length >= 2 && apdu[0] == 0x00 && apdu[1] == 0xa4) {
      const uint8_t instance_aid_length =
          cap_load_session.instanceAidLength();
      const bool selecting_same_applet =
          cap_load_session.installed() &&
          is_select_by_aid(apdu, apdu_length) &&
          apdu[4] == instance_aid_length &&
          std::memcmp(apdu + 5, cap_load_session.instanceAid(),
                      instance_aid_length) == 0;
      const bool selecting_isd = apdu_length == 5 && apdu[2] == 0x04 &&
                                  apdu[3] == 0x00 && apdu[4] == 0x00;
      if (is_select_by_aid(apdu, apdu_length) || selecting_isd) {
        get_status_first_seen = false;
        (void)choupi_vm_deselect_applet(selecting_same_applet ? 1 : 0);
      }
    }
    // Card Recognition Data is used to discover the card and its secure
    // channel before authentication, so expose GET DATA on the open channel.
    if (!secure_channel.active() && apdu_length == 5 && apdu[0] == 0x80 &&
        apdu[1] == 0xca) {
      if (!gp::write_get_data_response(apdu[2], apdu[3], response,
                                       sizeof(response), response_length)) {
        response[0] = 0x6f;
        response[1] = 0x00;
        response_length = 2;
      }
      send_response(rhport, 0x80, response, response_length);
      break;
    }
    if (apdu_length >= 2 && apdu[0] == 0x80 && apdu[1] == 0x50) {
      size_t data_length = 0;
      if (secure_channel.initializeUpdate(apdu, apdu_length, response,
                                          sizeof(response) - 2, data_length)) {
        response[data_length] = 0x90;
        response[data_length + 1] = 0x00;
        response_length = data_length + 2;
      } else {
        response[0] = apdu_length >= 5 && apdu[4] != 0x08 ? 0x67 : 0x6a;
        response[1] = apdu_length >= 5 && apdu[4] != 0x08 ? 0x00 : 0x86;
      }
      send_response(rhport, 0x80, response, response_length);
      break;
    }
    if (apdu_length >= 2 && apdu[0] == 0x84 && apdu[1] == 0x82) {
      if (secure_channel.externalAuthenticate(apdu, apdu_length)) {
        response[0] = 0x90;
        response[1] = 0x00;
      } else {
        response[0] = 0x69;
        response[1] = 0x82;
      }
      send_response(rhport, 0x80, response, response_length);
      break;
    }

    size_t command_data_length = 0;
    const bool authenticated = secure_channel.active() &&
                              secure_channel.verifyCommand(
                                  apdu, apdu_length, command_data_length);
    if (authenticated && apdu_length >= 4) {
      const uint8_t instruction = apdu[1];
      const uint8_t p1 = apdu[2];
      const uint8_t p2 = apdu[3];
      if (instruction == 0xe4) {
        gp::DeleteRequest request;
        const bool valid_request =
            apdu_length >= 5 &&
            command_data_length <= apdu_length - 5 &&
            gp::parse_delete_request(p1, p2, apdu + 5,
                                     command_data_length, request);
        uint16_t status = 0x6a80;
        if (valid_request) {
          (void)choupi_vm_deselect_applet(0);
          get_status_first_seen = false;
          status = cap_load_session.deleteObject(
              request.aid, request.aid_length,
              request.delete_related_objects);
        }
        response[0] = static_cast<uint8_t>(status >> 8);
        response[1] = static_cast<uint8_t>(status);
      } else if (instruction == 0xe6 && p1 == 0x02 && p2 == 0x00 &&
          cap_load_session.beginInstallForLoad(apdu + 5,
                                               command_data_length)) {
        response[0] = 0x90;
        response[1] = 0x00;
      } else if (instruction == 0xe8) {
        const bool accepted = apdu_length >= 5 &&
                              command_data_length <= apdu_length - 5 &&
                              cap_load_session.addLoadBlock(
                                  p1, p2, apdu + 5, command_data_length);
        response[0] = accepted ? 0x90 : 0x6a;
        response[1] = accepted ? 0x00 : 0x80;
      } else if (instruction == 0xe6 && p1 == 0x0c && p2 == 0x00) {
        if (cap_load_session.installAndMakeSelectable(
                apdu + 5, command_data_length)) {
          response[0] = 0x90;
          response[1] = 0x00;
        } else {
          response[0] = cap_load_session.installFailureStage();
          response[1] = static_cast<uint8_t>(
              cap_load_session.installVmStatus() >> 8);
          response[2] = static_cast<uint8_t>(
              cap_load_session.installVmStatus());
          response[3] = cap_load_session.installVmExceptionId();
          response[4] = cap_load_session.installVmExceptionOpcode();
          response[5] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionPc() >> 24);
          response[6] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionPc() >> 16);
          response[7] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionPc() >> 8);
          response[8] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionPc());
          response[9] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionPrefix() >> 24);
          response[10] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionPrefix() >> 16);
          response[11] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionPrefix() >> 8);
          response[12] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionPrefix());
          response[13] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionOperand() >> 8);
          response[14] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionOperand());
          response[15] = cap_load_session.installVmExceptionStackDepth();
          response[16] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionOpcodeHistory() >> 24);
          response[17] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionOpcodeHistory() >> 16);
          response[18] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionOpcodeHistory() >> 8);
          response[19] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionOpcodeHistory());
          response[20] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionDepthHistory() >> 24);
          response[21] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionDepthHistory() >> 16);
          response[22] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionDepthHistory() >> 8);
          response[23] = static_cast<uint8_t>(
              cap_load_session.installVmExceptionDepthHistory());
          response[24] = cap_load_session.installVmDebugStage();
          response[25] = static_cast<uint8_t>(
              cap_load_session.installVmDebugContext() >> 24);
          response[26] = static_cast<uint8_t>(
              cap_load_session.installVmDebugContext() >> 16);
          response[27] = static_cast<uint8_t>(
              cap_load_session.installVmDebugContext() >> 8);
          response[28] = static_cast<uint8_t>(
              cap_load_session.installVmDebugContext());
          const uint16_t install_status =
              cap_load_session.installFailureStatusWord();
          response[29] = static_cast<uint8_t>(install_status >> 8);
          response[30] = static_cast<uint8_t>(install_status);
          response_length = 31;
        }
      } else if (instruction == 0xf2 && apdu_length >= 5) {
        if (p2 != 0x02 && p2 != 0x03) {
          response[0] = 0x6a;
          response[1] = 0x86;
        } else {
          uint16_t status = 0x9000;
          if (p2 == 0x03) {
            status = gp::validate_get_status_next(
                get_status_first_seen, p1, apdu + 5, command_data_length);
          } else {
            get_status_cursor = 0;
            get_status_first_seen = false;
          }
          bool more_entries = false;
          response_length = 0;
          if (status == 0x9000) {
            status = build_dynamic_get_status_page(
                p1, apdu + 5, command_data_length, response,
                sizeof(response) - 2, response_length, more_entries);
            if (more_entries) status = 0x6310;
          }
          if (status == 0x9000 || status == 0x6310) {
            response[response_length++] = static_cast<uint8_t>(status >> 8);
            response[response_length++] = static_cast<uint8_t>(status);
            get_status_first_seen = true;
          } else {
            response[0] = static_cast<uint8_t>(status >> 8);
            response[1] = static_cast<uint8_t>(status);
            response_length = 2;
          }
        }
      } else if (instruction == 0xca) {
        if (!gp::write_get_data_response(p1, p2, response,
                                         sizeof(response), response_length)) {
          response[0] = 0x6f;
          response[1] = 0x00;
          response_length = 2;
        }
      } else {
        response[0] = 0x6d;
        response[1] = 0x00;
      }
    } else if (secure_channel.active()) {
      response[0] = 0x69;
      response[1] = 0x82;
    }
    // GlobalPlatformPro selects the default applet (the ISD) using a
    // case-1-style SELECT with an empty AID before issuing card-manager APDUs.
    // This minimal card-manager response supplies ISD discovery; applet install
    // and Java Card selection/dispatch are separate unfinished paths.
    if (apdu_length == 5 && apdu[0] == 0x00 && apdu[1] == 0xa4 &&
        apdu[2] == 0x04 && apdu[3] == 0x00 && apdu[4] == 0x00) {
      constexpr uint8_t isd_fci[] = {
          0x6f, 0x0a, 0x84, 0x08, 0xa0, 0x00, 0x00,
          0x01, 0x51, 0x00, 0x00, 0x00};
      std::memcpy(response, isd_fci, sizeof(isd_fci));
      response[sizeof(isd_fci)] = 0x90;
      response[sizeof(isd_fci) + 1] = 0x00;
      response_length = sizeof(isd_fci) + 2;
    } else if (is_select_by_aid(apdu, apdu_length)) {
      (void)cap_load_session.restoreInstalledForAid(apdu + 5, apdu[4]);
      const uint8_t installed_aid_length =
          cap_load_session.instanceAidLength();
      const bool select_instance_command =
          cap_load_session.installed() && apdu[4] == installed_aid_length &&
          std::memcmp(apdu + 5, cap_load_session.instanceAid(),
                      installed_aid_length) == 0;
      std::printf("CHOUPI:SELECT_APDU len=%u lc=%u installed=%u aid_len=%u match=%u\r\n",
                  static_cast<unsigned>(apdu_length),
                  static_cast<unsigned>(apdu[4]),
                  cap_load_session.installed() ? 1u : 0u,
                  static_cast<unsigned>(installed_aid_length),
                  select_instance_command ? 1u : 0u);
      std::fflush(stdout);
      if (select_instance_command) {
        uint8_t package_id = 0;
        uint16_t install_method_offset = 0;
        const uint8_t lookup_status = choupi_vm_find_applet(
            cap_load_session.instanceAid(),
            cap_load_session.instanceAidLength(), &package_id,
            &install_method_offset);
        std::printf("CHOUPI:SELECT lookup=%u package=%u install=%04X\r\n",
                    static_cast<unsigned>(lookup_status),
                    static_cast<unsigned>(package_id),
                    static_cast<unsigned>(install_method_offset));
        std::fflush(stdout);
        if (lookup_status != 0 ||
            !choupi_vm_is_dynamic_package(package_id)) {
          response[0] = 0x6a;
          response[1] = 0x82;
          response_length = 2;
        } else {
          uint16_t applet_class_index = 0xffff;
          const bool has_applet_class =
              cap_load_session.appletClassIndex(applet_class_index);
          std::printf("CHOUPI:SELECT class_found=%u class=%04X\r\n",
                      has_applet_class ? 1u : 0u,
                      static_cast<unsigned>(applet_class_index));
          std::fflush(stdout);
        bool select_failed = false;
          if (has_applet_class) {
            const uint8_t restore_status = choupi_vm_restore_applet(
                package_id, applet_class_index, cap_load_session.instanceAid(),
                cap_load_session.instanceAidLength());
            std::printf("CHOUPI:SELECT restore=%u\r\n",
                        static_cast<unsigned>(restore_status));
            std::fflush(stdout);
            if (restore_status != 0) {
              response[0] = 0x6f;
              response[1] = 0x00;
              response_length = 2;
            select_failed = true;
            }
          }
        if (!select_failed && applet_class_index != 0xffff) {
            const uint16_t select_status = choupi_vm_select_applet();
            std::printf("CHOUPI:SELECT select=%04X\r\n",
                        static_cast<unsigned>(select_status));
            std::fflush(stdout);
            if (select_status != 0x9000) {
              response[0] = static_cast<uint8_t>(select_status >> 8);
              response[1] = static_cast<uint8_t>(select_status);
              response_length = 2;
            select_failed = true;
            }
          }
        if (!select_failed) {
            uint16_t applet_response_length = 0;
            // SELECT itself is delivered to the applet, as is each later
            // command. Flash once per incoming applet APDU, not for reader
            // polling or card-manager traffic.
            choupi_reader_led_activity();
            std::printf("CHOUPI:SELECT process=begin\r\n");
            std::fflush(stdout);
            const uint16_t process_status = choupi_vm_process_applet(
                apdu, static_cast<uint16_t>(apdu_length), response,
                static_cast<uint16_t>(sizeof(response)),
                &applet_response_length);
            std::printf("CHOUPI:SELECT process=end sw=%04X len=%u\r\n",
                        static_cast<unsigned>(process_status),
                        static_cast<unsigned>(applet_response_length));
            std::fflush(stdout);
            if (process_status == 0xffff || applet_response_length < 2 ||
                applet_response_length > sizeof(response)) {
              response[0] = 0x6f;
              response[1] = 0x00;
              response_length = 2;
              (void)choupi_vm_deselect_applet(0);
            } else {
              response_length = applet_response_length;
              if (!select_status_keeps_applet_selected(process_status)) {
                (void)choupi_vm_deselect_applet(0);
              } else if (fs_sync() != 0) {
                response[0] = 0x6f;
                response[1] = 0x00;
                response_length = 2;
                (void)choupi_vm_deselect_applet(0);
              }
            }
          }
        }
      } else {
        response[0] = 0x6a;
        response[1] = 0x82;
        response_length = 2;
      }
    } else if (apdu_length <= UINT16_MAX) {
      uint16_t applet_response_length = 0;
      choupi_reader_led_activity();
      const uint16_t status = choupi_vm_process_applet(
          apdu, static_cast<uint16_t>(apdu_length), response,
          static_cast<uint16_t>(sizeof(response)), &applet_response_length);
      if (status != 0xffff) {
        response_length = applet_response_length;
        if (fs_sync() != 0) {
          response[0] = 0x6f;
          response[1] = 0x00;
          response_length = 2;
        }
      }
    }
    send_response(rhport, 0x80, response, response_length);
    break;
  }
  case 0x72: // PC_to_RDR_Abort
    send_response(rhport, 0x81, nullptr, 0, slot_state);
    break;
  default:
    ++debug_unknown_message_count;
    debug_last_unknown_message = command;
    send_response(rhport, 0x81, nullptr, 0, slot_state);
    break;
  }
}

void driver_init() {
  debug_stage = 1;
  rx_length = 0;
  rx_expected = 0;
  message_pending = false;
  slot_state = 1;
  ep_out = 0;
  ep_in = 0;
  secure_channel.reset();
  cap_load_session.reset();
  choupi_reader_led_set_reader_mode(false);
}

bool driver_deinit() {
  choupi_reader_led_set_reader_mode(false);
  return true;
}

void driver_reset(uint8_t) {
  choupi_vm_clear_selected_applet();
  rx_length = 0;
  rx_expected = 0;
  message_pending = false;
  slot_state = 1;
  get_status_first_seen = false;
  ep_out = 0;
  ep_in = 0;
  secure_channel.reset();
  cap_load_session.abortTransfer();
  choupi_reader_led_set_reader_mode(false);
}

uint16_t driver_open(uint8_t rhport,
                     tusb_desc_interface_t const *interface_desc,
                     uint16_t max_length) {
  if (interface_desc->bInterfaceClass != 0x0b ||
      interface_desc->bInterfaceNumber != kCcidInterface) {
    return 0;
  }
  uint16_t consumed = interface_desc->bLength;
  const uint8_t *descriptor = reinterpret_cast<const uint8_t *>(interface_desc) +
                              interface_desc->bLength;
  while (consumed + 2 <= max_length && descriptor[1] != TUSB_DESC_INTERFACE) {
    if (descriptor[1] == TUSB_DESC_ENDPOINT) {
      auto endpoint = reinterpret_cast<const tusb_desc_endpoint_t *>(descriptor);
      if (tu_edpt_dir(endpoint->bEndpointAddress) == TUSB_DIR_OUT) {
        ep_out = endpoint->bEndpointAddress;
      } else {
        ep_in = endpoint->bEndpointAddress;
      }
      if (!usbd_edpt_open(rhport, endpoint)) {
        return 0;
      }
    }
    if (descriptor[0] < 2) {
      return 0;
    }
    consumed = static_cast<uint16_t>(consumed + descriptor[0]);
    descriptor += descriptor[0];
  }
  if (ep_out == 0 || ep_in == 0) {
    return 0;
  }
  debug_stage = 2;
  choupi_reader_led_set_reader_mode(true);
  queue_out(rhport);
  return consumed;
}

bool driver_control(uint8_t rhport, uint8_t stage,
                    tusb_control_request_t const *request) {
  if (stage != CONTROL_STAGE_SETUP || request->bmRequestType_bit.type !=
                                          TUSB_REQ_TYPE_CLASS ||
      request->wIndex != kCcidInterface) {
    return false;
  }
  if (request->bRequest == 0x03 && request->bmRequestType_bit.direction ==
                                       TUSB_DIR_IN) { // GetDataRates
    static uint8_t rate[4] = {0x00, 0xc2, 0x01, 0x00}; // 115200 bit/s
    return tud_control_xfer(rhport, request, rate, sizeof(rate));
  }
  return false;
}

bool driver_xfer(uint8_t rhport, uint8_t endpoint, xfer_result_t result,
                 uint32_t transferred) {
  if (result != XFER_RESULT_SUCCESS) {
    debug_last_xfer_result = static_cast<uint8_t>(result);
    if (endpoint == ep_out) {
      ++debug_out_xfer_errors;
    } else if (endpoint == ep_in) {
      ++debug_in_xfer_errors;
    }
    // Drop a partial command or a response that could not be delivered. The
    // host owns APDU retry semantics; keep the reader endpoint armed so a
    // subsequent CCID command can recover without a USB re-enumeration.
    rx_length = 0;
    rx_expected = 0;
    message_pending = false;
    queue_out(rhport);
    return true;
  }
  if (endpoint == ep_in) {
    debug_stage = 7;
    queue_out(rhport);
    return true;
  }
  if (endpoint != ep_out || rx_length + transferred > sizeof(rx_buffer)) {
    ++debug_invalid_messages;
    rx_length = 0;
    rx_expected = 0;
    queue_out(rhport);
    return false;
  }
  debug_endpoint = endpoint;
  ++debug_rx_events;
  debug_stage = 3;
  rx_length += transferred;
  if (rx_expected == 0 && rx_length >= 10) {
    const uint32_t payload_length = read_le32(rx_buffer + 1);
    if (payload_length + 10 > kMaxCcidMessage) {
      ++debug_invalid_messages;
      rx_length = 0;
      rx_expected = 0;
      queue_out(rhport);
      return false;
    }
    rx_expected = payload_length + 10;
  }
  if (rx_expected != 0 && rx_length >= rx_expected) {
    // TinyUSB invokes this class callback from its low-priority USB worker
    // IRQ. Defer APDU parsing and especially VM INSTALL execution to the main
    // loop so the IRQ never performs flash, allocation, or Java bytecode work.
    pending_rhport = rhport;
    message_pending = true;
    debug_stage = 4;
  } else {
    queue_out(rhport);
  }
  return true;
}

const usbd_class_driver_t kDriver = {
    .name = "CCID",
    .init = driver_init,
    .deinit = driver_deinit,
    .reset = driver_reset,
    .open = driver_open,
    .control_xfer_cb = driver_control,
    .xfer_cb = driver_xfer,
    .sof = nullptr,
};
} // namespace

extern "C" void choupi_ccid_task(void) {
  if (!message_pending) {
    return;
  }
  const uint8_t rhport = pending_rhport;
  message_pending = false;
  ++debug_task_count;
  debug_stage = 5;
  process_message(rhport);
}

extern "C" void choupi_ccid_debug_state(uint8_t *stage, uint8_t *endpoint,
                                         uint16_t *rx_events,
                                         uint16_t *task_count,
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
                                         uint8_t *last_xfer_result) {
  if (stage != nullptr) *stage = debug_stage;
  if (endpoint != nullptr) *endpoint = debug_endpoint;
  if (rx_events != nullptr) *rx_events = debug_rx_events;
  if (task_count != nullptr) *task_count = debug_task_count;
  if (last_apdu_length != nullptr) {
    *last_apdu_length = debug_last_apdu_length;
  }
  if (last_apdu_header != nullptr) {
    for (uint8_t index = 0; index < sizeof(debug_last_apdu_header); ++index) {
      last_apdu_header[index] = debug_last_apdu_header[index];
    }
  }
  if (last_message_type != nullptr) {
    *last_message_type = debug_last_message_type;
  }
  if (last_payload_length != nullptr) {
    *last_payload_length = debug_last_payload_length;
  }
  if (xfr_block_count != nullptr) {
    *xfr_block_count = debug_xfr_block_count;
  }
  if (out_xfer_errors != nullptr) {
    *out_xfer_errors = debug_out_xfer_errors;
  }
  if (in_xfer_errors != nullptr) {
    *in_xfer_errors = debug_in_xfer_errors;
  }
  if (out_rearm_errors != nullptr) {
    *out_rearm_errors = debug_out_rearm_errors;
  }
  if (in_submit_errors != nullptr) {
    *in_submit_errors = debug_in_submit_errors;
  }
  if (invalid_messages != nullptr) {
    *invalid_messages = debug_invalid_messages;
  }
  if (last_xfer_result != nullptr) {
    *last_xfer_result = debug_last_xfer_result;
  }
}

extern "C" void choupi_ccid_debug_unknown(uint8_t *last_message,
                                            uint16_t *message_count) {
  if (last_message != nullptr) *last_message = debug_last_unknown_message;
  if (message_count != nullptr) {
    *message_count = debug_unknown_message_count;
  }
}

extern "C" void choupi_ccid_debug_recent(uint8_t *commands,
                                           uint8_t command_capacity,
                                           uint16_t *command_count) {
  if (command_count != nullptr) *command_count = debug_command_count;
  if (commands == nullptr) return;
  const uint8_t count = command_capacity < sizeof(debug_recent_commands)
                            ? command_capacity
                            : sizeof(debug_recent_commands);
  const uint8_t oldest = debug_recent_command_index;
  for (uint8_t i = 0; i < count; ++i) {
    commands[i] = debug_recent_commands[(oldest + i) %
                                        sizeof(debug_recent_commands)];
  }
}

extern "C" usbd_class_driver_t const *usbd_app_driver_get_cb(
    uint8_t *driver_count) {
  *driver_count = 1;
  return &kDriver;
}

extern "C" const uint8_t *tud_descriptor_device_cb(void) {
  static const tusb_desc_device_t descriptor = {
      .bLength = sizeof(tusb_desc_device_t),
      .bDescriptorType = TUSB_DESC_DEVICE,
      .bcdUSB = 0x0200,
      .bDeviceClass = TUSB_CLASS_MISC,
      .bDeviceSubClass = MISC_SUBCLASS_COMMON,
      .bDeviceProtocol = MISC_PROTOCOL_IAD,
      .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
      .idVendor = 0x2e8a,
      .idProduct = 0x0009,
      .bcdDevice = 0x0100,
      .iManufacturer = 1,
      .iProduct = 2,
      .iSerialNumber = 3,
      .bNumConfigurations = 1,
  };
  return reinterpret_cast<const uint8_t *>(&descriptor);
}

extern "C" const uint8_t *tud_descriptor_configuration_cb(uint8_t) {
  return kConfiguration;
}

extern "C" const uint16_t *tud_descriptor_string_cb(uint8_t index,
                                                       uint16_t) {
  static uint16_t descriptor[32];
  static char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1]{};
  const char *value = nullptr;
  if (index == 0) {
    descriptor[0] = (TUSB_DESC_STRING << 8) | 4;
    descriptor[1] = 0x0409;
    descriptor[2] = 0x0409;
    return descriptor;
  }
  if (index == 1) value = "Pico2";
  if (index == 2) value = "Pico2 JCVM CCID";
  if (index == 3) {
    if (serial[0] == '\0') {
      pico_get_unique_board_id_string(serial, sizeof(serial));
    }
    value = serial;
  }
  if (index == 4) value = "CHOUPI CDC";
  if (index == 5) value = "JCVM CCID";
  if (value == nullptr) return nullptr;
  size_t length = std::strlen(value);
  if (length > 31) length = 31;
  descriptor[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 + 2 * length));
  for (size_t i = 0; i < length; ++i) descriptor[i + 1] = value[i];
  return descriptor;
}
