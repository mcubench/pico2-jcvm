#include "vpcd_protocol.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {
unsigned int checks = 0;

void check(const bool condition, const char *name) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL %s\n", name);
    std::exit(EXIT_FAILURE);
  }
}
} // namespace

int main() {
  const std::array<uint8_t, 5> apdu{0x00, 0xa4, 0x04, 0x00, 0x00};
  std::vector<uint8_t> frame;
  check(choupi::vpcd::encode_frame(apdu.data(), apdu.size(), frame),
        "encode APDU frame");
  check(frame.size() == apdu.size() + 2 && frame[0] == 0 &&
            frame[1] == apdu.size(),
        "network-order length prefix");

  std::vector<uint8_t> message;
  check(choupi::vpcd::decode_frame(frame.data(), frame.size(), message) ==
            choupi::vpcd::ReadResult::Message,
        "decode APDU frame");
  check(message.size() == apdu.size() && message[1] == 0xa4 &&
            message[4] == 0x00,
        "APDU payload round trip");

  const uint8_t control = 0x04;
  check(choupi::vpcd::encode_frame(&control, 1, frame),
        "encode control frame");
  check(frame.size() == 3 && frame[0] == 0 && frame[1] == 1 &&
            frame[2] == control,
        "control frame encoding");
  check(choupi::vpcd::decode_frame(frame.data(), frame.size(), message) ==
            choupi::vpcd::ReadResult::Message &&
            message.size() == 1 && message[0] == control,
        "control frame round trip");

  const uint8_t empty_header[] = {0x00, 0x00};
  check(choupi::vpcd::decode_frame(empty_header, sizeof(empty_header), message) ==
            choupi::vpcd::ReadResult::InvalidFrame && message.empty(),
        "reject empty frame");
  const uint8_t short_header[] = {0x00};
  check(choupi::vpcd::decode_frame(short_header, sizeof(short_header), message) ==
            choupi::vpcd::ReadResult::IoError,
        "reject truncated header");
  const uint8_t truncated_payload[] = {0x00, 0x03, 0xaa};
  check(choupi::vpcd::decode_frame(truncated_payload,
                                  sizeof(truncated_payload), message) ==
            choupi::vpcd::ReadResult::IoError && message.empty(),
        "reject truncated payload");
  const uint8_t extra_payload[] = {0x00, 0x01, 0xaa, 0xbb};
  check(choupi::vpcd::decode_frame(extra_payload, sizeof(extra_payload),
                                  message) ==
            choupi::vpcd::ReadResult::IoError,
        "reject trailing payload bytes");
  check(!choupi::vpcd::encode_frame(nullptr, 1, frame),
        "reject null payload");
  check(!choupi::vpcd::encode_frame(&control, 0, frame),
        "reject empty outgoing frame");
  check(choupi::vpcd::decode_frame(nullptr, 0, message) ==
            choupi::vpcd::ReadResult::IoError,
        "reject null frame");

  int sockets[2]{};
  check(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0,
        "create socket pair for idle read");
  check(choupi::vpcd::read_message(sockets[0], message, 5) ==
            choupi::vpcd::ReadResult::Timeout && message.empty(),
        "report idle frame timeout");
  const uint8_t partial_header = 0;
  const ssize_t partial_header_sent =
      ::send(sockets[1], &partial_header, 1, 0);
  if (partial_header_sent != 1) {
    std::fprintf(stderr,
                 "TEST:DIAGNOSTIC partial header send=%zd errno=%d\n",
                 partial_header_sent, errno);
  }
  check(partial_header_sent == 1, "send partial frame header");
  check(choupi::vpcd::read_message(sockets[0], message, 5) ==
            choupi::vpcd::ReadResult::IoError && message.empty(),
        "reject timeout after partial frame");
  ::close(sockets[0]);
  ::close(sockets[1]);

  std::printf("TEST:SUMMARY pass=%u fail=0\n", checks);
  return EXIT_SUCCESS;
}
