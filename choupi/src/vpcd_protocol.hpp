#ifndef CHOUPI_VPCD_PROTOCOL_HPP
#define CHOUPI_VPCD_PROTOCOL_HPP

#ifdef PC_VERSION

#include <cstddef>
#include <cstdint>
#include <vector>

namespace choupi::vpcd {

enum class ReadResult { Message, Closed, Timeout, InvalidFrame, IoError };

ReadResult read_message(int socket_fd, std::vector<uint8_t> &message,
                        int timeout_ms = 0);
bool write_message(int socket_fd, const uint8_t *message, uint16_t length);
ReadResult decode_frame(const uint8_t *frame, size_t frame_length,
                        std::vector<uint8_t> &message);
bool encode_frame(const uint8_t *message, uint16_t length,
                  std::vector<uint8_t> &frame);

} // namespace choupi::vpcd

#endif // PC_VERSION

#endif // CHOUPI_VPCD_PROTOCOL_HPP
