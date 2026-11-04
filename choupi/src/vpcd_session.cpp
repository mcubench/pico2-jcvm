#include "vpcd_session.hpp"
#include "../../src/apdu_status.hpp"

#ifdef PC_VERSION

#include <algorithm>
#include <iterator>
#include <utility>

namespace choupi::vpcd {
namespace {

constexpr uint8_t kControlOff = 0x00;
constexpr uint8_t kControlOn = 0x01;
constexpr uint8_t kControlReset = 0x02;
constexpr uint8_t kControlAtr = 0x04;
constexpr uint8_t kAtr[] = {0x3b, 0x80, 0x01, 0x81};
constexpr uint8_t kIsdAid[] = {0xa0, 0x00, 0x00, 0x01,
                                0x51, 0x00, 0x00, 0x00};
constexpr size_t kResponseCapacity = 4096;

void append_status(std::vector<uint8_t> &response, uint16_t status) {
  response = {static_cast<uint8_t>(status >> 8),
              static_cast<uint8_t>(status)};
}

void append_select_response(std::vector<uint8_t> &response, const uint8_t *aid,
                            uint8_t aid_length) {
  response.resize(6 + aid_length);
  response[0] = 0x6f;
  response[1] = static_cast<uint8_t>(aid_length + 2);
  response[2] = 0x84;
  response[3] = aid_length;
  std::copy(aid, aid + aid_length, response.begin() + 4);
  response[4 + aid_length] = 0x90;
  response[5 + aid_length] = 0x00;
}

} // namespace

Session::Session(CardCallbacks callbacks) : callbacks_(callbacks) {}

void Session::clear_or_deselect(const bool selecting_same_applet) {
  if (selected_) {
    if (callbacks_.deselect != nullptr) {
      (void)callbacks_.deselect(callbacks_.context,
                                selecting_same_applet);
    }
    selected_ = false;
  }
}

bool Session::select_by_aid(const uint8_t *aid, const size_t aid_length,
                            const uint8_t *command,
                            const size_t command_length) const {
  if (aid == nullptr || command == nullptr || aid_length < 5 ||
      aid_length > 16 || command_length < 5 || command[0] != 0x00 ||
      command[1] != 0xa4 || command[2] != 0x04 ||
      (command[3] & 0xef) != 0 ||
      command[4] != aid_length ||
      (command_length != 5 + aid_length &&
       command_length != 6 + aid_length)) {
    return false;
  }
  return std::equal(aid, aid + aid_length, command + 5);
}

namespace {

bool is_select_by_aid(const uint8_t *command, const size_t command_length) {
  if (command == nullptr || command_length < 5 || command[0] != 0x00 ||
      command[1] != 0xa4 || command[2] != 0x04 ||
      (command[3] & 0xef) != 0 ||
      command[4] < 5 || command[4] > 16) {
    return false;
  }
  const size_t aid_length = command[4];
  return command_length == 5 + aid_length ||
         command_length == 6 + aid_length;
}

} // namespace

SessionResult Session::handle_message(const uint8_t *message,
                                      const size_t message_length,
                                      std::vector<uint8_t> &response) {
  response.clear();
  if (message == nullptr || message_length == 0) {
    return SessionResult::ProtocolError;
  }

  if (message_length == 1) {
    switch (message[0]) {
    case kControlOff:
      selected_ = false;
      card_manager_selected_ = false;
      if (callbacks_.clear != nullptr) {
        callbacks_.clear(callbacks_.context);
      }
      if (callbacks_.reset_card_manager != nullptr) {
        callbacks_.reset_card_manager(callbacks_.context);
      }
      powered_ = false;
      return SessionResult::NoResponse;
    case kControlOn:
      powered_ = true;
      return SessionResult::NoResponse;
    case kControlReset:
      selected_ = false;
      card_manager_selected_ = false;
      if (callbacks_.clear != nullptr) {
        callbacks_.clear(callbacks_.context);
      }
      if (callbacks_.reset_card_manager != nullptr) {
        callbacks_.reset_card_manager(callbacks_.context);
      }
      powered_ = true;
      return SessionResult::NoResponse;
    case kControlAtr:
      // VPCD readers probe card presence by requesting the ATR before they
      // send POWER_ON. Return it in either state so PC/SC can mark the slot
      // as containing a card and allow clients to connect.
      response.assign(std::begin(kAtr), std::end(kAtr));
      return SessionResult::Response;
    default:
      return SessionResult::ProtocolError;
    }
  }

  if (!powered_) {
    append_status(response, 0x6985);
    return SessionResult::Response;
  }

  const bool select_default_isd =
      message_length == 5 && message[0] == 0x00 && message[1] == 0xa4 &&
      message[2] == 0x04 && message[3] == 0x00 && message[4] == 0x00;
  if (select_default_isd ||
      select_by_aid(kIsdAid, sizeof(kIsdAid), message, message_length)) {
    clear_or_deselect(false);
    card_manager_selected_ = true;
    append_select_response(response, kIsdAid, sizeof(kIsdAid));
    return SessionResult::Response;
  }

  if (is_select_by_aid(message, message_length)) {
    card_manager_selected_ = false;
    clear_or_deselect(true);
    const uint8_t aid_length = message[4];
    if (callbacks_.restore == nullptr || callbacks_.restore(
            callbacks_.context, message + 5, aid_length) != 0) {
      append_status(response, 0x6a82);
      return SessionResult::Response;
    }
    const uint16_t status = callbacks_.select == nullptr
                                ? 0x6f00
                                : callbacks_.select(callbacks_.context);
    if (status != 0x9000) {
      append_status(response, status);
      return SessionResult::Response;
    }
    selected_ = true;
    response.resize(kResponseCapacity);
    uint16_t response_length = 0;
    const uint16_t process_status = callbacks_.process == nullptr
                                        ? 0xffff
                                        : callbacks_.process(
                                              callbacks_.context, message,
                                              static_cast<uint16_t>(message_length),
                                              response.data(),
                                              static_cast<uint16_t>(response.size()),
                                              &response_length);
    if (process_status == 0xffff || response_length < 2 ||
        response_length > response.size()) {
      append_status(response, 0x6999);
      clear_or_deselect(false);
      return SessionResult::Response;
    }
    response.resize(response_length);
    const uint16_t select_status = static_cast<uint16_t>(
        (static_cast<uint16_t>(response[response_length - 2]) << 8) |
        response[response_length - 1]);
    if (!select_status_keeps_applet_selected(select_status)) {
      clear_or_deselect(false);
    }
    return SessionResult::Response;
  }

  if (!selected_) {
    if (card_manager_selected_ && callbacks_.process_card_manager != nullptr) {
      response.resize(kResponseCapacity);
      uint16_t response_length = 0;
      const uint16_t status = callbacks_.process_card_manager(
          callbacks_.context, message,
          static_cast<uint16_t>(message_length), response.data(),
          static_cast<uint16_t>(response.size()), &response_length);
      if (status == 0xffff || response_length < 2 ||
          response_length > response.size()) {
        append_status(response, 0x6999);
        return SessionResult::Response;
      }
      response.resize(response_length);
      return SessionResult::Response;
    }
    append_status(response, 0x6999);
    return SessionResult::Response;
  }

  response.resize(kResponseCapacity);
  uint16_t response_length = 0;
  const uint16_t status = callbacks_.process == nullptr
                              ? 0xffff
                              : callbacks_.process(
                                    callbacks_.context, message,
                                    static_cast<uint16_t>(message_length),
                                    response.data(),
                                    static_cast<uint16_t>(response.size()),
                                    &response_length);
  if (status == 0xffff || response_length > response.size()) {
    append_status(response, 0x6999);
    return SessionResult::Response;
  }
  response.resize(response_length);
  return SessionResult::Response;
}

void Session::disconnect() {
  selected_ = false;
  card_manager_selected_ = false;
  if (callbacks_.clear != nullptr) {
    callbacks_.clear(callbacks_.context);
  }
  if (callbacks_.reset_card_manager != nullptr) {
    callbacks_.reset_card_manager(callbacks_.context);
  }
  powered_ = false;
}

} // namespace choupi::vpcd

#endif // PC_VERSION
