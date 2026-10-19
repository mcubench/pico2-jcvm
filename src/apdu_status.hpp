#ifndef PICO2_JCVM_APDU_STATUS_HPP
#define PICO2_JCVM_APDU_STATUS_HPP

#include <cstdint>

// A SELECT response with SW1=61 means the applet is selected and has more
// response data available through GET RESPONSE.
constexpr bool select_status_keeps_applet_selected(uint16_t status) noexcept {
  return status == 0x9000 || (status & 0xff00) == 0x6100;
}

#endif
