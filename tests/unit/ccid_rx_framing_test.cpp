#include "ccid_rx_framing.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {
constexpr size_t kBufferCapacity = 320;
constexpr size_t kMaxMessage = 300;
constexpr size_t kEndpointSize = 64;

void set_payload(std::array<uint8_t, kBufferCapacity> &frame,
                 uint32_t payload) {
  frame[1] = static_cast<uint8_t>(payload);
  frame[2] = static_cast<uint8_t>(payload >> 8);
  frame[3] = static_cast<uint8_t>(payload >> 16);
  frame[4] = static_cast<uint8_t>(payload >> 24);
}

void framing_rejects_wrapped_lengths() {
  std::array<uint8_t, kBufferCapacity> frame{};
  frame[0] = 0x6f;
  size_t expected = 123;
  for (uint64_t length = 0xfffffff6ull; length <= 0xffffffffull; ++length) {
    set_payload(frame, static_cast<uint32_t>(length));
    const auto result = ccid_rx::inspect(frame.data(), 10, frame.size(),
                                         kMaxMessage, 0, expected);
    assert(result == ccid_rx::FrameStatus::Invalid);
    assert(expected == 0);
  }
}

void framing_accepts_exact_maximum_and_fragmentation() {
  std::array<uint8_t, kBufferCapacity> frame{};
  frame[0] = 0x6f;
  set_payload(frame, static_cast<uint32_t>(kMaxMessage - 10));
  size_t expected = 0;
  assert(ccid_rx::inspect(frame.data(), 5, frame.size(), kMaxMessage, 0,
                          expected) == ccid_rx::FrameStatus::NeedMore);
  assert(expected == 0);
  assert(ccid_rx::inspect(frame.data(), 10, frame.size(), kMaxMessage, 0,
                          expected) == ccid_rx::FrameStatus::NeedMore);
  assert(expected == kMaxMessage);
  assert(ccid_rx::inspect(frame.data(), kMaxMessage - 1, frame.size(),
                          kMaxMessage, 0, expected) ==
         ccid_rx::FrameStatus::NeedMore);
  assert(ccid_rx::inspect(frame.data(), kMaxMessage, frame.size(),
                          kMaxMessage, 0, expected) ==
         ccid_rx::FrameStatus::Complete);
}

void framing_simulates_fragmented_usb_transfers() {
  std::array<uint8_t, kBufferCapacity> frame{};
  frame[0] = 0x6f;
  set_payload(frame, 256);

  size_t received = 0;
  size_t expected = 0;
  size_t transfer_count = 0;
  auto status = ccid_rx::FrameStatus::NeedMore;
  while (status == ccid_rx::FrameStatus::NeedMore) {
    const size_t armed = ccid_rx::next_transfer_size(
        frame.size(), kEndpointSize, received, expected);
    const size_t transferred = std::min(armed, 266 - received);
    assert(armed != 0);
    assert(ccid_rx::transfer_fits(transferred, armed, received, frame.size()));
    received += transferred;
    status = ccid_rx::inspect(frame.data(), received, frame.size(),
                              kMaxMessage, 0, expected);
    ++transfer_count;
    assert(transfer_count <= 8);
  }
  assert(status == ccid_rx::FrameStatus::Complete);
  assert(received == 266);
  assert(expected == 266);
  assert(transfer_count == 5); // 64 + 64 + 64 + 64 + 10

  // A short first packet can split the CCID header; subsequent receive sizes
  // must still fit the buffer and eventually validate the exact full frame.
  received = 5;
  expected = 0;
  status = ccid_rx::inspect(frame.data(), received, frame.size(), kMaxMessage,
                            0, expected);
  assert(status == ccid_rx::FrameStatus::NeedMore);
  while (status == ccid_rx::FrameStatus::NeedMore) {
    const size_t armed = ccid_rx::next_transfer_size(
        frame.size(), kEndpointSize, received, expected);
    const size_t transferred = std::min(armed, 266 - received);
    assert(armed != 0);
    assert(ccid_rx::transfer_fits(transferred, armed, received, frame.size()));
    received += transferred;
    status = ccid_rx::inspect(frame.data(), received, frame.size(),
                              kMaxMessage, 0, expected);
  }
  assert(status == ccid_rx::FrameStatus::Complete);
  assert(received == 266);
}

void framing_rejects_oversize_slot_and_bad_state() {
  std::array<uint8_t, kBufferCapacity> frame{};
  frame[0] = 0x6f;
  size_t expected = 0;

  set_payload(frame, static_cast<uint32_t>(kMaxMessage - 9));
  assert(ccid_rx::inspect(frame.data(), 10, frame.size(), kMaxMessage, 0,
                          expected) == ccid_rx::FrameStatus::Invalid);

  set_payload(frame, 0);
  frame[5] = 1;
  assert(ccid_rx::inspect(frame.data(), 10, frame.size(), kMaxMessage, 0,
                          expected) == ccid_rx::FrameStatus::Invalid);

  frame[5] = 0;
  assert(ccid_rx::inspect(frame.data(), 10, frame.size(), kMaxMessage, 1,
                          expected) == ccid_rx::FrameStatus::Invalid);

  frame[0] = 0x65; // GetSlotStatus is valid while the card is inactive.
  assert(ccid_rx::inspect(frame.data(), 10, frame.size(), kMaxMessage, 1,
                          expected) == ccid_rx::FrameStatus::Complete);
}

void framing_rejects_extra_bytes_and_bounds_every_transfer() {
  std::array<uint8_t, kBufferCapacity> frame{};
  frame[0] = 0x65;
  set_payload(frame, 0);
  size_t expected = 0;
  assert(ccid_rx::inspect(frame.data(), 11, frame.size(), kMaxMessage, 1,
                          expected) == ccid_rx::FrameStatus::Invalid);
  assert(ccid_rx::transfer_fits(64, 64, 0, frame.size()));
  assert(!ccid_rx::transfer_fits(65, 64, 0, frame.size()));
  assert(!ccid_rx::transfer_fits(2, 2, 319, frame.size()));

  assert(ccid_rx::next_transfer_size(frame.size(), kEndpointSize, 0, 0) == 64);
  assert(ccid_rx::next_transfer_size(frame.size(), kEndpointSize, 290, 300) ==
         10);
  assert(ccid_rx::next_transfer_size(frame.size(), kEndpointSize, 300, 300) ==
         0);
  assert(ccid_rx::next_transfer_size(frame.size(), kEndpointSize, 319, 0) ==
         1);
  assert(ccid_rx::next_transfer_size(frame.size(), kEndpointSize, 320, 0) ==
         0);
}
} // namespace

int main() {
  framing_rejects_wrapped_lengths();
  framing_accepts_exact_maximum_and_fragmentation();
  framing_simulates_fragmented_usb_transfers();
  framing_rejects_oversize_slot_and_bad_state();
  framing_rejects_extra_bytes_and_bounds_every_transfer();
  std::puts("CCID_RX_FRAMING:PASS wrapped lengths, boundaries, slots, state, fragmentation");
}
