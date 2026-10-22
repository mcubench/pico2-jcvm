#ifndef CHOUPI_VPCD_SESSION_HPP
#define CHOUPI_VPCD_SESSION_HPP

#ifdef PC_VERSION

#include <cstddef>
#include <cstdint>
#include <vector>

namespace choupi::vpcd {

struct CardCallbacks {
  void *context = nullptr;
  void (*clear)(void *context) = nullptr;
  uint8_t (*restore)(void *context, const uint8_t *aid, uint8_t aid_length) =
      nullptr;
  uint16_t (*select)(void *context) = nullptr;
  uint16_t (*deselect)(void *context, bool selecting_same_applet) = nullptr;
  uint16_t (*process)(void *context, const uint8_t *command,
                      uint16_t command_length, uint8_t *response,
                      uint16_t response_capacity, uint16_t *response_length) =
      nullptr;
  uint16_t (*process_card_manager)(void *context, const uint8_t *command,
                                   uint16_t command_length, uint8_t *response,
                                   uint16_t response_capacity,
                                   uint16_t *response_length) = nullptr;
  void (*reset_card_manager)(void *context) = nullptr;
};

enum class SessionResult { NoResponse, Response, ProtocolError };

class Session {
public:
  Session(std::vector<uint8_t> applet_aid, CardCallbacks callbacks);

  SessionResult handle_message(const uint8_t *message, size_t message_length,
                               std::vector<uint8_t> &response);
  void disconnect();

  bool powered() const noexcept { return powered_; }
  bool applet_selected() const noexcept { return selected_; }

private:
  void clear_or_deselect(bool selecting_same_applet);
  bool select_by_aid(const uint8_t *aid, size_t aid_length,
                     const uint8_t *command, size_t command_length) const;

  std::vector<uint8_t> applet_aid_;
  CardCallbacks callbacks_;
  bool powered_ = false;
  bool selected_ = false;
  bool card_manager_selected_ = false;
};

} // namespace choupi::vpcd

#endif // PC_VERSION

#endif // CHOUPI_VPCD_SESSION_HPP
