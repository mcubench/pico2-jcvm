#ifndef PICO2_JCVM_GP_GET_DATA_HPP
#define PICO2_JCVM_GP_GET_DATA_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gp {

// Card Recognition Data and synthetic CPLC data for the card-manager
// features exposed by CHOUPI. CPLC identifiers are project-defined markers,
// not factory-programmed RP2350/Pico manufacturing identifiers. Dates use the
// GlobalPlatform YDDD encoding; 2026-10-31 is year digit 6, day 304.
constexpr uint8_t kCardRecognitionData[] = {
    0x66, 0x18, 0x73, 0x16,
    0x06, 0x07, 0x2a, 0x86, 0x48, 0x86, 0xfc, 0x6b, 0x01,
    0x64, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xfc, 0x6b,
    0x04, 0x03, 0x10};

// Feitian's USB vendor ID (096E) is reused as the synthetic CPLC fabricator
// marker; RP2350 is encoded as the synthetic IC type 2350. These are not
// claimed to be GlobalPlatform-assigned CPLC manufacturer/type codes.
// Unavailable per-device serial, batch and equipment identifiers are zero.
constexpr uint8_t kCplcData[] = {
    0x9f, 0x7f, 0x2a,
    0x09, 0x6e, // IC fabricator: Feitian marker
    0x23, 0x50, // IC type: RP2350 marker
    0x4a, 0x43, // OS provider: "JC"
    0x63, 0x04, // OS release date: 2026 day 304 (October 31)
    0x00, 0x01, // OS release level
    0x63, 0x04, // IC fabrication date
    0x00, 0x00, 0x00, 0x00, // IC serial number unavailable
    0x00, 0x00, // IC batch identifier unavailable
    0x09, 0x6e, // IC module fabricator: Feitian marker
    0x63, 0x04, // IC module packaging date
    0x09, 0x6e, // ICC manufacturer: Feitian marker
    0x63, 0x04, // IC embedding date
    0x09, 0x6e, // IC pre-personalizer: Feitian marker
    0x63, 0x04, // IC pre-personalization date
    0x00, 0x00, 0x00, 0x00, // pre-personalization equipment ID unavailable
    0x09, 0x6e, // IC personalizer: Feitian marker
    0x63, 0x04, // IC personalization date
    0x00, 0x00, 0x00, 0x00, // personalization equipment ID unavailable
};

inline bool get_data_object(uint8_t p1, uint8_t p2, const uint8_t *&data,
                            size_t &length) noexcept {
  data = nullptr;
  length = 0;
  if (p1 == 0x00 && p2 == 0x66) {
    data = kCardRecognitionData;
    length = sizeof(kCardRecognitionData);
    return true;
  }
  if (p1 == 0x9f && p2 == 0x7f) {
    data = kCplcData;
    length = sizeof(kCplcData);
    return true;
  }
  return false;
}

inline bool write_get_data_response(uint8_t p1, uint8_t p2,
                                    uint8_t *response,
                                    size_t response_capacity,
                                    size_t &response_length,
                                    bool include_tlv = true) noexcept {
  response_length = 0;
  if (response == nullptr || response_capacity < 2) return false;

  const uint8_t *data = nullptr;
  size_t data_length = 0;
  if (!get_data_object(p1, p2, data, data_length)) {
    response[0] = 0x6a;
    response[1] = 0x88;
    response_length = 2;
    return true;
  }
  if (!include_tlv) {
    // ISO GET DATA (INS=CB) returns the data-object value. GlobalPlatform's
    // proprietary form (INS=CA) returns the complete BER-TLV object.
    const size_t tag_length = p1 == 0x9f && p2 == 0x7f ? 2 : 1;
    if (data_length <= tag_length) return false;
    const size_t length_offset = tag_length;
    const uint8_t value_length = data[length_offset];
    if ((value_length & 0x80) != 0 ||
        static_cast<size_t>(value_length) + length_offset + 1 != data_length) {
      return false;
    }
    data += length_offset + 1;
    data_length = value_length;
  }
  if (data_length + 2 > response_capacity) return false;

  std::memcpy(response, data, data_length);
  response[data_length] = 0x90;
  response[data_length + 1] = 0x00;
  response_length = data_length + 2;
  return true;
}

} // namespace gp

#endif
