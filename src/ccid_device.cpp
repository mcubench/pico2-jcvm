#include <cstdint>
#include <cstring>

#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "pico/unique_id.h"
#include "tusb.h"
#include "scp03.hpp"
#include "cap_load.hpp"

extern "C" uint8_t choupi_vm_find_applet(const uint8_t *aid,
                                           uint8_t aid_length,
                                           uint8_t *package_id,
                                           uint16_t *install_offset);
extern "C" uint8_t choupi_vm_restore_applet(uint8_t package_id,
                                            uint16_t class_index,
                                            const uint8_t *aid,
                                            uint8_t aid_length);
extern "C" void choupi_vm_clear_selected_applet(void);
extern "C" uint16_t choupi_vm_select_applet(void);
extern "C" uint16_t choupi_vm_deselect_applet(void);
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
constexpr uint8_t kAlgTestPackageAid[] = {0x4a, 0x43, 0x41, 0x6c, 0x67,
                                          0x54, 0x65, 0x73, 0x74};
constexpr uint8_t kAtr[] = {0x3b, 0x80, 0x01, 0x81};

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
Scp03Session secure_channel;
CapLoadSession cap_load_session;

uint32_t read_le32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

void queue_out(uint8_t rhport) {
  if (ep_out != 0 && !usbd_edpt_busy(rhport, ep_out)) {
    (void)usbd_edpt_xfer(rhport, ep_out, rx_buffer + rx_length,
                         static_cast<uint16_t>(kEndpointSize));
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
  (void)usbd_edpt_xfer(rhport, ep_in, tx_buffer,
                       static_cast<uint16_t>(payload_length + 10));
}

void process_message(uint8_t rhport) {
  const uint8_t command = rx_buffer[0];
  switch (command) {
  case 0x62: // PC_to_RDR_IccPowerOn
    slot_state = 0;
    cap_load_session.restoreInstalled();
    send_response(rhport, 0x80, kAtr, sizeof(kAtr));
    break;
  case 0x63: // PC_to_RDR_IccPowerOff
    (void)choupi_vm_deselect_applet();
    slot_state = 1;
    secure_channel.reset();
    cap_load_session.abortTransfer();
    send_response(rhport, 0x81, nullptr, 0, slot_state);
    break;
  case 0x65: // PC_to_RDR_GetSlotStatus
    send_response(rhport, 0x81, nullptr, 0, slot_state);
    break;
  case 0x6f: { // PC_to_RDR_XfrBlock
    const size_t apdu_length = rx_expected - 10;
    const uint8_t *apdu = rx_buffer + 10;
    uint8_t response[96] = {0x6d, 0x00}; // instruction not supported
    size_t response_length = 2;
    if (apdu_length >= 2 && apdu[0] == 0x00 && apdu[1] == 0xa4) {
      (void)choupi_vm_deselect_applet();
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
      if (instruction == 0xe6 && p1 == 0x02 && p2 == 0x00 &&
          cap_load_session.beginInstallForLoad(apdu + 5,
                                               command_data_length)) {
        response[0] = 0x90;
        response[1] = 0x00;
      } else if (instruction == 0xe8 &&
                 cap_load_session.addLoadBlock(p1, p2, apdu + 5,
                                               command_data_length)) {
        response[0] = 0x90;
        response[1] = 0x00;
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
      } else if (instruction == 0xf2 && p1 == 0x80 &&
                 (p2 & 0xfe) == 0x02) {
        constexpr uint8_t isd_registry_entry[] = {
            0xe3, 0x11, 0x4f, 0x08, 0xa0, 0x00, 0x00, 0x01, 0x51, 0x00,
            0x00, 0x00, 0x9f, 0x70, 0x01, 0x01, 0xc5, 0x01, 0x00};
        std::memcpy(response, isd_registry_entry,
                    sizeof(isd_registry_entry));
        size_t registry_length = sizeof(isd_registry_entry);
        if (cap_load_session.installed()) {
          const uint8_t aid_length = cap_load_session.instanceAidLength();
          uint8_t *entry = response + registry_length;
          const size_t entry_length = 22 + aid_length;
          entry[0] = 0xe3;
          entry[1] = static_cast<uint8_t>(entry_length - 2);
          entry[2] = 0x4f;
          entry[3] = aid_length;
          std::memcpy(entry + 4, cap_load_session.instanceAid(), aid_length);
          size_t pos = 4 + aid_length;
          entry[pos++] = 0xcc;
          entry[pos++] = sizeof(kAlgTestPackageAid);
          std::memcpy(entry + pos, kAlgTestPackageAid,
                      sizeof(kAlgTestPackageAid));
          pos += sizeof(kAlgTestPackageAid);
          entry[pos++] = 0x9f;
          entry[pos++] = 0x70;
          entry[pos++] = 0x01;
          entry[pos++] = 0x07; // selectable
          entry[pos++] = 0xc5;
          entry[pos++] = 0x01;
          entry[pos++] = 0x00; // no privileges
          registry_length += entry_length;
        }
        response[registry_length++] = 0x90;
        response[registry_length++] = 0x00;
        response_length = registry_length;
      } else if (instruction == 0xf2 || instruction == 0xca) {
        response[0] = 0x6a;
        response[1] = 0x88;
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
    } else if (cap_load_session.installed() && apdu_length >= 5 &&
               apdu[0] == 0x00 && apdu[1] == 0xa4 && apdu[2] == 0x04 &&
               apdu[3] == 0x00 &&
               apdu[4] == cap_load_session.instanceAidLength() &&
               (apdu_length == 5u + static_cast<size_t>(apdu[4]) ||
                (apdu_length == 6u + static_cast<size_t>(apdu[4]) &&
                 apdu[apdu_length - 1] == 0x00)) &&
               std::memcmp(apdu + 5, cap_load_session.instanceAid(),
                           cap_load_session.instanceAidLength()) == 0) {
      uint8_t package_id = 0;
      uint16_t install_method_offset = 0;
      if (choupi_vm_find_applet(cap_load_session.instanceAid(),
                                cap_load_session.instanceAidLength(),
                                &package_id, &install_method_offset) != 0 ||
          package_id != 63) {
        send_response(rhport, 0x80, response, response_length);
        break;
      }
      uint16_t applet_class_index = 0xffff;
      if (cap_load_session.appletClassIndex(applet_class_index) &&
          choupi_vm_restore_applet(package_id, applet_class_index,
                                   cap_load_session.instanceAid(),
                                   cap_load_session.instanceAidLength()) != 0) {
        response[0] = 0x6f;
        response[1] = 0x00;
        response_length = 2;
        send_response(rhport, 0x80, response, response_length);
        break;
      }
      if (applet_class_index != 0xffff) {
        const uint16_t select_status = choupi_vm_select_applet();
        if (select_status != 0x9000) {
          response[0] = static_cast<uint8_t>(select_status >> 8);
          response[1] = static_cast<uint8_t>(select_status);
          response_length = 2;
          send_response(rhport, 0x80, response, response_length);
          break;
        }
      }
      const uint8_t aid_length = cap_load_session.instanceAidLength();
      response[0] = 0x6f;
      response[1] = static_cast<uint8_t>(aid_length + 2);
      response[2] = 0x84;
      response[3] = aid_length;
      std::memcpy(response + 4, cap_load_session.instanceAid(), aid_length);
      response[4 + aid_length] = 0x90;
      response[5 + aid_length] = 0x00;
      response_length = 6 + aid_length;
    } else if (apdu_length <= UINT16_MAX) {
      uint16_t applet_response_length = 0;
      const uint16_t status = choupi_vm_process_applet(
          apdu, static_cast<uint16_t>(apdu_length), response,
          static_cast<uint16_t>(sizeof(response)), &applet_response_length);
      if (status != 0xffff) {
        response_length = applet_response_length;
      }
    }
    send_response(rhport, 0x80, response, response_length);
    break;
  }
  case 0x72: // PC_to_RDR_Abort
    send_response(rhport, 0x81, nullptr, 0, slot_state);
    break;
  default:
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
}

bool driver_deinit() { return true; }

void driver_reset(uint8_t) {
  rx_length = 0;
  rx_expected = 0;
  message_pending = false;
  slot_state = 1;
  ep_out = 0;
  ep_in = 0;
  secure_channel.reset();
  cap_load_session.abortTransfer();
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
        if (fs_sync() != 0) {
          response[0] = 0x6f;
          response[1] = 0x00;
          response_length = 2;
        }
    descriptor += descriptor[0];
  }
  if (ep_out == 0 || ep_in == 0) {
    return 0;
  }
  debug_stage = 2;
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
    return false;
  }
  if (endpoint == ep_in) {
    debug_stage = 7;
    queue_out(rhport);
    return true;
  }
  if (endpoint != ep_out || rx_length + transferred > sizeof(rx_buffer)) {
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
      rx_length = 0;
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
                                         uint16_t *task_count) {
  if (stage != nullptr) *stage = debug_stage;
  if (endpoint != nullptr) *endpoint = debug_endpoint;
  if (rx_events != nullptr) *rx_events = debug_rx_events;
  if (task_count != nullptr) *task_count = debug_task_count;
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
  if (index == 1) value = "Raspberry Pi";
  if (index == 2) value = "Pico 2 CHOUPI CCID";
  if (index == 3) {
    if (serial[0] == '\0') {
      pico_get_unique_board_id_string(serial, sizeof(serial));
    }
    value = serial;
  }
  if (index == 4) value = "CHOUPI CDC";
  if (index == 5) value = "JCAlgTest CCID";
  if (value == nullptr) return nullptr;
  size_t length = std::strlen(value);
  if (length > 31) length = 31;
  descriptor[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 + 2 * length));
  for (size_t i = 0; i < length; ++i) descriptor[i + 1] = value[i];
  return descriptor;
}
