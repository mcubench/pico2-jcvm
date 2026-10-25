#include "reader_led.hpp"

#include "pico/stdlib.h"

#if defined(PICO_DEFAULT_LED_PIN)
#include "hardware/pwm.h"
#include "pico/time.h"

namespace {
constexpr uint32_t kHeartbeatPeriodMs = 10000;
constexpr uint32_t kHeartbeatDurationMs = 1600;
constexpr uint32_t kActivityDurationMs = 120;
constexpr uint16_t kPwmTop = 4095;

bool reader_mode = false;
volatile bool activity_active = false;
uint32_t heartbeat_started_ms = 0;
uint32_t activity_started_ms = 0;
uint slice = 0;
volatile alarm_id_t activity_alarm = 0;
void set_brightness(uint16_t brightness);

int64_t activity_timeout(alarm_id_t id, void *) {
  if (id == activity_alarm) {
    activity_alarm = 0;
    activity_active = false;
    set_brightness(0);
  }
  return 0;
}

uint16_t heartbeat_brightness(uint32_t elapsed_ms) {
  const uint32_t phase = elapsed_ms % kHeartbeatPeriodMs;
  if (phase >= kHeartbeatDurationMs) {
    return 0;
  }

  const uint32_t half_duration = kHeartbeatDurationMs / 2;
  const uint32_t ramp = phase < half_duration
                            ? phase
                            : kHeartbeatDurationMs - phase;
  const uint32_t x = (ramp * 65535u) / half_duration;
  // Smoothstep gives the idle pulse a gradual rise and fall.
  const uint64_t x_squared = static_cast<uint64_t>(x) * x / 65535u;
  const uint64_t smooth = x_squared * (3u * 65535u - 2u * x) / 65535u;
  return static_cast<uint16_t>(smooth);
}

void set_brightness(uint16_t brightness) {
#if defined(PICO_DEFAULT_LED_PIN_INVERTED)
  brightness = static_cast<uint16_t>(65535u - brightness);
#endif
  pwm_set_gpio_level(PICO_DEFAULT_LED_PIN,
                     static_cast<uint16_t>(
                         static_cast<uint32_t>(brightness) * kPwmTop / 65535u));
}
} // namespace
#endif

extern "C" void choupi_reader_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
  gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_PWM);
  slice = pwm_gpio_to_slice_num(PICO_DEFAULT_LED_PIN);
  pwm_set_clkdiv(slice, 30.0f);
  pwm_set_wrap(slice, kPwmTop);
  pwm_set_enabled(slice, true);
  set_brightness(0);
#endif
}

extern "C" void choupi_reader_led_set_reader_mode(bool enabled) {
#if defined(PICO_DEFAULT_LED_PIN)
  reader_mode = enabled;
  activity_active = false;
  if (activity_alarm > 0) {
    cancel_alarm(activity_alarm);
  }
  activity_alarm = 0;
  heartbeat_started_ms = to_ms_since_boot(get_absolute_time());
  if (!enabled) {
    set_brightness(0);
  }
#else
  (void)enabled;
#endif
}

extern "C" void choupi_reader_led_activity(void) {
#if defined(PICO_DEFAULT_LED_PIN)
  if (reader_mode) {
    if (activity_alarm > 0) {
      cancel_alarm(activity_alarm);
    }
    activity_active = true;
    activity_started_ms = to_ms_since_boot(get_absolute_time());
    set_brightness(65535);
    activity_alarm = add_alarm_in_ms(
        kActivityDurationMs, activity_timeout, nullptr, true);
  }
#endif
}

extern "C" void choupi_reader_led_task(void) {
#if defined(PICO_DEFAULT_LED_PIN)
  if (!reader_mode) {
    return;
  }

  const uint32_t now = to_ms_since_boot(get_absolute_time());
  if (activity_active) {
    if (now - activity_started_ms < kActivityDurationMs) {
      set_brightness(65535);
      return;
    }
    if (activity_alarm > 0) {
      cancel_alarm(activity_alarm);
    }
    activity_alarm = 0;
    activity_active = false;
  }

  set_brightness(heartbeat_brightness(now - heartbeat_started_ms));
#endif
}
