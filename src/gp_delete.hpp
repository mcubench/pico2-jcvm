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

} // namespace gp

#endif
