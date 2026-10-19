#ifndef PICO2_JCVM_GP_DELETE_HPP
#define PICO2_JCVM_GP_DELETE_HPP

#include <cstddef>
#include <cstdint>

namespace gp {

struct DeleteRequest {
  const uint8_t *aid = nullptr;
  uint8_t aid_length = 0;
  bool delete_related_objects = false;
};

// Parse the supported single-object GlobalPlatform DELETE command. This card
// supports one AID qualifier (4F) and the P2 related-object flag only.
inline bool parse_delete_request(uint8_t p1, uint8_t p2, const uint8_t *data,
                                 size_t data_length,
                                 DeleteRequest &request) noexcept {
  request = {};
  if (p1 != 0 || (p2 != 0 && p2 != 0x80) || data == nullptr ||
      data_length < 7 || data[0] != 0x4f) {
    return false;
  }

  size_t cursor = 1;
  size_t aid_length = data[cursor++];
  if ((aid_length & 0x80) != 0) {
    const uint8_t length_bytes = static_cast<uint8_t>(aid_length & 0x7f);
    if ((length_bytes != 1 && length_bytes != 2) ||
        length_bytes > data_length - cursor) {
      return false;
    }
    aid_length = 0;
    for (uint8_t index = 0; index < length_bytes; ++index) {
      aid_length = (aid_length << 8) | data[cursor++];
    }
    // BER-TLV requires the shortest length form.
    if (aid_length < 128 || (length_bytes == 2 && aid_length <= 0xff)) {
      return false;
    }
  }

  if (aid_length < 5 || aid_length > 16 ||
      aid_length != data_length - cursor) {
    return false;
  }

  request.aid = data + cursor;
  request.aid_length = static_cast<uint8_t>(aid_length);
  request.delete_related_objects = p2 == 0x80;
  return true;
}

// Apply the persistent part of a DELETE operation through a small backend
// contract. Keeping ordering and rollback here lets host tests exercise the
// same transaction path used by CapLoadSession without emulating RP2350 flash.
// The backend provides begin(), clear_applet(class_index),
// clear_package_static(), mark_deleted(package), sync(), rollback(), and
// commit() methods. It may throw; failures map to NV-memory failure (6581).
template <typename Backend>
uint16_t commit_delete_state(Backend &backend, bool delete_package,
                             bool applet_installed,
                             uint16_t applet_class_index) noexcept {
  bool began = false;
  try {
    began = backend.begin();
  } catch (...) {
    return 0x6581;
  }
  if (!began) return 0x6581;
  bool transaction_active = true;
  const auto rollback = [&]() noexcept {
    if (!transaction_active) return true;
    transaction_active = false;
    bool restored = false;
    try {
      restored = backend.rollback();
    } catch (...) {
      restored = false;
    }
    uint8_t sync_status = 1;
    try {
      sync_status = backend.sync();
    } catch (...) {
      sync_status = 1;
    }
    return sync_status == 0 && restored;
  };

  try {
    if (delete_package) {
      if (applet_installed && !backend.clear_applet(applet_class_index)) {
        (void)rollback();
        return 0x6581;
      }
      if (!backend.clear_package_static()) {
        (void)rollback();
        return 0x6581;
      }
    } else if (!backend.clear_applet(applet_class_index)) {
      (void)rollback();
      return 0x6581;
    }

    backend.mark_deleted(delete_package);
    if (backend.sync() != 0) {
      (void)rollback();
      return 0x6581;
    }
    if (!backend.commit()) {
      transaction_active = false;
      return 0x6581;
    }
    transaction_active = false;
  } catch (...) {
    (void)rollback();
    return 0x6581;
  }
  return 0x9000;
}

} // namespace gp

#endif
