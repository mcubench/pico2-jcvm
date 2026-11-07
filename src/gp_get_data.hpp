#ifndef PICO2_JCVM_GP_GET_DATA_HPP
#define PICO2_JCVM_GP_GET_DATA_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef CHOUPI_CPLC_PERSONALIZER_EQUIPMENT_ID
#define CHOUPI_CPLC_PERSONALIZER_EQUIPMENT_ID 0x00000000UL
#endif

namespace gp {

// Card Recognition Data and synthetic CPLC data for the card-manager
// features exposed by CHOUPI. CPLC identifiers are project-defined markers,
// not factory-programmed RP2350/Pico manufacturing identifiers. Dates use the
// GlobalPlatform YDDD encoding; 2026-11-07 is year digit 6, day 311.
constexpr uint8_t kCardRecognitionData[] = {
    0x66, 0x18, 0x73, 0x16,
    0x06, 0x07, 0x2a, 0x86, 0x48, 0x86, 0xfc, 0x6b, 0x01,
    0x64, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xfc, 0x6b,
    0x04, 0x03, 0x10};

// Feitian's USB vendor ID (096E) is reused as the synthetic CPLC fabricator
// marker; RP2350 is encoded as the synthetic IC type 2350. These are not
// claimed to be GlobalPlatform-assigned CPLC manufacturer/type codes.
// Unavailable per-device serial, batch and pre-personalization equipment IDs
// are zero. The personalization equipment ID is generated for each firmware
// build and identifies its source commit (with an explicit dirty marker).
// Firmware release encoding in OperatingSystemReleaseLevel is four hex
// digits: major version nibble, minor version nibble, then an update byte.
// Thus 0.1.1 is 0x0101. Change these only for an explicitly requested release.
constexpr uint8_t kFirmwareVersionMajor = 0;
constexpr uint8_t kFirmwareVersionMinor = 1;
constexpr uint8_t kFirmwareVersionUpdate = 1;
constexpr uint16_t kFirmwareCplcReleaseLevel =
    static_cast<uint16_t>((kFirmwareVersionMajor << 12) |
                          (kFirmwareVersionMinor << 8) |
                          kFirmwareVersionUpdate);

constexpr uint8_t kCplcData[] = {
    0x9f, 0x7f, 0x2a,
    0x09, 0x6e, // IC fabricator: Feitian marker
    0x23, 0x50, // IC type: RP2350 marker
    0x4a, 0x43, // OS provider: "JC"
    0x63, 0x11, // OS release date: 2026 day 311 (November 7)
    static_cast<uint8_t>(kFirmwareCplcReleaseLevel >> 8),
    static_cast<uint8_t>(kFirmwareCplcReleaseLevel),
    0x63, 0x11, // IC fabrication date
    0x00, 0x00, 0x00, 0x00, // IC serial number unavailable
    0x00, 0x00, // IC batch identifier unavailable
    0x09, 0x6e, // IC module fabricator: Feitian marker
    0x63, 0x11, // IC module packaging date
    0x09, 0x6e, // ICC manufacturer: Feitian marker
    0x63, 0x11, // IC embedding date
    0x09, 0x6e, // IC pre-personalizer: Feitian marker
    0x63, 0x11, // IC pre-personalization date
    0x00, 0x00, 0x00, 0x00, // pre-personalization equipment ID unavailable
    0x09, 0x6e, // IC personalizer: Feitian marker
    0x63, 0x11, // IC personalization date
    static_cast<uint8_t>((CHOUPI_CPLC_PERSONALIZER_EQUIPMENT_ID >> 24) & 0xff),
    static_cast<uint8_t>((CHOUPI_CPLC_PERSONALIZER_EQUIPMENT_ID >> 16) & 0xff),
    static_cast<uint8_t>((CHOUPI_CPLC_PERSONALIZER_EQUIPMENT_ID >> 8) & 0xff),
    static_cast<uint8_t>(CHOUPI_CPLC_PERSONALIZER_EQUIPMENT_ID & 0xff),
};

constexpr uint8_t kIsdAid[] = {
    0xa0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x00};

inline bool valid_get_applications_tag_list(const uint8_t *data,
                                            size_t length) noexcept {
  return data != nullptr && length == 2 && data[0] == 0x5c && data[1] == 0x00;
}

inline bool append_application_template(uint8_t *response, size_t capacity,
                                        size_t &length, const uint8_t *aid,
                                        uint8_t aid_length) noexcept {
  length = 0;
  if (response == nullptr || aid == nullptr || aid_length < 5 ||
      aid_length > 16 || capacity < static_cast<size_t>(aid_length) + 4) {
    return false;
  }
  response[0] = 0x61;
  response[1] = static_cast<uint8_t>(aid_length + 2);
  response[2] = 0x4f;
  response[3] = aid_length;
  std::memcpy(response + 4, aid, aid_length);
  length = static_cast<size_t>(aid_length) + 4;
  return true;
}

// GET DATA for tag 2F00 has a special response form: a series of Application
// Templates, not an outer BER-TLV object. The caller enumerates installed
// applet instance AIDs after the ISD entry.
template <typename EnumerateApplications>
inline bool write_get_applications_response(
    uint8_t *response, size_t response_capacity, size_t &response_length,
    EnumerateApplications enumerate_applications) {
  response_length = 0;
  if (response == nullptr || response_capacity < 2) return false;

  size_t cursor = 0;
  const auto append_aid = [&](const uint8_t *aid, uint8_t aid_length) {
    if (cursor > response_capacity - 2) return false;
    size_t template_length = 0;
    if (!append_application_template(response + cursor,
                                     response_capacity - 2 - cursor,
                                     template_length, aid, aid_length)) {
      return false;
    }
    cursor += template_length;
    return true;
  };

  if (!append_aid(kIsdAid, static_cast<uint8_t>(sizeof(kIsdAid))) ||
      !enumerate_applications(append_aid) || cursor > response_capacity - 2) {
    return false;
  }
  response[cursor++] = 0x90;
  response[cursor++] = 0x00;
  response_length = cursor;
  return true;
}

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
