#include "vpcd_protocol.hpp"

#ifdef PC_VERSION

#include <cerrno>
#include <cstddef>
#include <sys/socket.h>

namespace choupi::vpcd {
namespace {

enum class ExactRead { Complete, Closed, Error };

ExactRead read_exact(int socket_fd, uint8_t *buffer, size_t length) {
  size_t offset = 0;
  while (offset < length) {
    const ssize_t count = ::recv(socket_fd, buffer + offset, length - offset, 0);
    if (count == 0) {
      return offset == 0 ? ExactRead::Closed : ExactRead::Error;
    }
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return ExactRead::Error;
    }
    offset += static_cast<size_t>(count);
  }
  return ExactRead::Complete;
}

bool write_exact(int socket_fd, const uint8_t *buffer, size_t length) {
  size_t offset = 0;
  while (offset < length) {
    const ssize_t count = ::send(socket_fd, buffer + offset, length - offset,
                                 MSG_NOSIGNAL);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (count == 0) {
      return false;
    }
    offset += static_cast<size_t>(count);
  }
  return true;
}

} // namespace

ReadResult decode_frame(const uint8_t *frame, const size_t frame_length,
                        std::vector<uint8_t> &message) {
  message.clear();
  if (frame == nullptr || frame_length < 2) {
    return ReadResult::IoError;
  }
  const uint16_t length = static_cast<uint16_t>(
      (static_cast<uint16_t>(frame[0]) << 8) | frame[1]);
  if (length == 0) {
    return ReadResult::InvalidFrame;
  }
  if (frame_length != static_cast<size_t>(length) + 2) {
    return ReadResult::IoError;
  }
  message.assign(frame + 2, frame + frame_length);
  return ReadResult::Message;
}

bool encode_frame(const uint8_t *message, const uint16_t length,
                  std::vector<uint8_t> &frame) {
  frame.clear();
  if (message == nullptr || length == 0) {
    return false;
  }
  frame.reserve(static_cast<size_t>(length) + 2);
  frame.push_back(static_cast<uint8_t>(length >> 8));
  frame.push_back(static_cast<uint8_t>(length));
  frame.insert(frame.end(), message, message + length);
  return true;
}

ReadResult read_message(const int socket_fd, std::vector<uint8_t> &message) {
  uint8_t header[2]{};
  message.clear();

  const ExactRead header_result = read_exact(socket_fd, header, sizeof(header));
  if (header_result == ExactRead::Closed) {
    return ReadResult::Closed;
  }
  if (header_result != ExactRead::Complete) {
    return ReadResult::IoError;
  }

  const uint16_t length = static_cast<uint16_t>(
      (static_cast<uint16_t>(header[0]) << 8) | header[1]);
  if (length == 0) {
    return ReadResult::InvalidFrame;
  }

  message.resize(length);
  const ExactRead payload_result =
      read_exact(socket_fd, message.data(), message.size());
  if (payload_result != ExactRead::Complete) {
    message.clear();
    return ReadResult::IoError;
  }
  return ReadResult::Message;
}

bool write_message(const int socket_fd, const uint8_t *message,
                   const uint16_t length) {
  std::vector<uint8_t> frame;
  if (!encode_frame(message, length, frame)) {
    return false;
  }
  return write_exact(socket_fd, frame.data(), frame.size());
}

} // namespace choupi::vpcd

#endif // PC_VERSION
