#include "vpcd_session.hpp"

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

Session::Session(std::vector<uint8_t> applet_aid, CardCallbacks callbacks)
    : applet_aid_(std::move(applet_aid)), callbacks_(callbacks) {}

void Session::clear_or_deselect() {
  if (selected_) {
    if (callbacks_.deselect != nullptr) {
      (void)callbacks_.deselect(callbacks_.context);
    }
    selected_ = false;
  } else if (callbacks_.clear != nullptr) {
    callbacks_.clear(callbacks_.context);
  }
}

bool Session::select_by_aid(const uint8_t *aid, const size_t aid_length,
                            const uint8_t *command,
                            const size_t command_length) const {
  if (aid == nullptr || command == nullptr || aid_length < 5 ||
      aid_length > 16 || command_length < 5 || command[0] != 0x00 ||
      command[1] != 0xa4 || command[2] != 0x04 || command[3] != 0x00 ||
      command[4] != aid_length ||
      (command_length != 5 + aid_length &&
       command_length != 6 + aid_length)) {
    return false;
  }
  return std::equal(aid, aid + aid_length, command + 5);
}

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
      powered_ = false;
      clear_or_deselect();
      return SessionResult::NoResponse;
    case kControlOn:
      powered_ = true;
      return SessionResult::NoResponse;
    case kControlReset:
      clear_or_deselect();
      powered_ = true;
      return SessionResult::NoResponse;
    case kControlAtr:
      if (powered_) {
        response.assign(std::begin(kAtr), std::end(kAtr));
        return SessionResult::Response;
      }
      return SessionResult::NoResponse;
    default:
      return SessionResult::ProtocolError;
    }
  }

  if (!powered_) {
    append_status(response, 0x6985);
    return SessionResult::Response;
  }

  if (select_by_aid(kIsdAid, sizeof(kIsdAid), message, message_length)) {
    clear_or_deselect();
    append_select_response(response, kIsdAid, sizeof(kIsdAid));
    return SessionResult::Response;
  }

  if (select_by_aid(applet_aid_.data(), applet_aid_.size(), message,
                    message_length)) {
    clear_or_deselect();
    if (callbacks_.clear != nullptr) {
      callbacks_.clear(callbacks_.context);
    }
    if (callbacks_.restore == nullptr ||
        callbacks_.restore(callbacks_.context, applet_aid_.data(),
                           static_cast<uint8_t>(applet_aid_.size())) != 0) {
      append_status(response, 0x6f00);
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
    append_select_response(response, applet_aid_.data(),
                           static_cast<uint8_t>(applet_aid_.size()));
    return SessionResult::Response;
  }

  if (!selected_) {
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
  clear_or_deselect();
  powered_ = false;
}

} // namespace choupi::vpcd

#endif // PC_VERSION
