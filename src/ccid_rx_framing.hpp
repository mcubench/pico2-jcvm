#ifndef PICO2_JCVM_CCID_RX_FRAMING_HPP
#define PICO2_JCVM_CCID_RX_FRAMING_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace ccid_rx {

constexpr size_t kHeaderSize = 10;
constexpr uint8_t kSlotCount = 1;
constexpr uint8_t kXfrBlock = 0x6f;
constexpr uint8_t kSlotActive = 0;

enum class FrameStatus : uint8_t { NeedMore, Complete, Invalid };

inline uint32_t read_le32(const uint8_t *bytes) noexcept {
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

inline size_t next_transfer_size(size_t buffer_capacity, size_t endpoint_size,
                                 size_t received, size_t expected) noexcept {
  if (received >= buffer_capacity ||
      (expected != 0 && received >= expected)) {
    return 0;
  }
  size_t remaining = buffer_capacity - received;
  if (expected != 0) remaining = std::min(remaining, expected - received);
  return std::min(remaining, endpoint_size);
}

inline FrameStatus inspect(const uint8_t *buffer, size_t received,
                           size_t buffer_capacity, size_t max_message,
                           uint8_t active_slot_state,
                           size_t &expected) noexcept {
  expected = 0;
  if (received > buffer_capacity || max_message < kHeaderSize ||
      (received != 0 && buffer == nullptr)) {
    return FrameStatus::Invalid;
  }
  if (received < kHeaderSize) return FrameStatus::NeedMore;

  const uint32_t payload_length = read_le32(buffer + 1);
  const size_t max_payload = max_message - kHeaderSize;
  // Compare before adding: the wire length is untrusted and 32-bit.
  if (payload_length > max_payload || buffer[5] >= kSlotCount) {
    return FrameStatus::Invalid;
  }
  expected = kHeaderSize + static_cast<size_t>(payload_length);
  if (buffer[0] == kXfrBlock && active_slot_state != kSlotActive) {
    return FrameStatus::Invalid;
  }
  if (received > expected) return FrameStatus::Invalid;
  return received == expected ? FrameStatus::Complete : FrameStatus::NeedMore;
}

inline bool transfer_fits(size_t transferred, size_t armed,
                          size_t received, size_t buffer_capacity) noexcept {
  return transferred <= armed && received <= buffer_capacity &&
         transferred <= buffer_capacity - received;
}

} // namespace ccid_rx

#endif // PICO2_JCVM_CCID_RX_FRAMING_HPP
