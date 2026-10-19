#ifndef PICO2_JCVM_GP_GET_DATA_HPP
#define PICO2_JCVM_GP_GET_DATA_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gp {

// Card Recognition Data for the card-manager features currently exposed by
// CHOUPI: GlobalPlatform Card Recognition Data and the SCP03 S8 protocol
// implemented by Scp03Session. No CPLC or unique card-identification values
// are advertised because this firmware has no manufacturing data source.
constexpr uint8_t kCardRecognitionData[] = {
    0x66, 0x18, 0x73, 0x16,
    0x06, 0x07, 0x2a, 0x86, 0x48, 0x86, 0xfc, 0x6b, 0x01,
    0x64, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xfc, 0x6b,
    0x04, 0x03, 0x10};

inline bool get_data_object(uint8_t p1, uint8_t p2, const uint8_t *&data,
                            size_t &length) noexcept {
  data = nullptr;
  length = 0;
  if (p1 == 0x00 && p2 == 0x66) {
    data = kCardRecognitionData;
    length = sizeof(kCardRecognitionData);
    return true;
  }
  return false;
}

inline bool write_get_data_response(uint8_t p1, uint8_t p2,
                                    uint8_t *response,
                                    size_t response_capacity,
                                    size_t &response_length) noexcept {
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
  if (data_length + 2 > response_capacity) return false;

  std::memcpy(response, data, data_length);
  response[data_length] = 0x90;
  response[data_length + 1] = 0x00;
  response_length = data_length + 2;
  return true;
}

} // namespace gp

#endif
