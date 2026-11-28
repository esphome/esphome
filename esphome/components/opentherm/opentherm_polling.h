#pragma once

/*
 * OpenTherm protocol implementation. Originally taken from https://github.com/jpraus/arduino-opentherm, but
 * heavily modified to comply with ESPHome coding standards and provide better logging.
 * Original code is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International
 * Public License, which is compatible with GPLv3 license, which covers C++ part of ESPHome project.
 */

#include <cstdint>
#include "esphome/core/hal.h"
#include "opentherm_base.h"

#ifdef USE_ESP32
#include <soc/soc_caps.h>
#include "driver/gptimer.h"
#endif

#if !(defined(USE_ESP32) && SOC_RMT_SUPPORTED)

namespace esphome::opentherm {

/**
 * Timer-driven transport. A general purpose timer samples the line at five times the bit rate, and while sending
 * flips the output at twice the bit rate; the frame is assembled from those samples. Used where there is no RMT
 * peripheral: the ESP8266 and the ESP32 variants without one (C2, C61).
 */
class OpenThermPolling : public OpenThermBase {
 public:
  OpenThermPolling(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t device_timeout = 800)
      : OpenThermBase(in_pin, out_pin, device_timeout) {}

  bool initialize() override;
  void listen() override;

  void report_and_reset_timer_error() override;

  /// Blocks while the transport stays in its current stage, or until the timeout expires. Only usable here: sampling
  /// and bit flipping advance from interrupts, so nothing else has to run for a conversation to complete.
  /// @return true when the stage ended, false on timeout
  bool wait_until_done(uint32_t timeout);

#ifdef USE_ESP32
  static bool timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
#else
  static bool timer_isr(OpenThermPolling *arg);
#endif

#ifdef ESP8266
  static void esp8266_timer_isr();
#endif

 private:
  void write_frame() override;
  void abort() override;

  void read_();  // data detected, start reading
  void bit_read_(uint8_t value);
  void write_half_bit_(uint8_t bit, uint8_t half);

  // 5 kHz timer_ to sample at 1/5 of manchester code bit length
  void start_read_timer_();
  // 2 kHz timer_ to send manchester code
  void start_write_timer_();
  void stop_timer_();

#ifdef USE_ESP32
  bool init_esp32_timer_();
  void start_esp32_timer_(uint64_t alarm_value);

  gptimer_handle_t timer_handle_{nullptr};
  gptimer_alarm_config_t alarm_config_{
      .alarm_count = 0,
      .reload_count = 0,
      .flags = {.auto_reload_on_alarm = true},
  };
  esp_err_t timer_error_ = ESP_OK;
  TimerErrorType timer_error_type_ = TimerErrorType::NO_TIMER_ERROR;
#endif

  // The read timer is running for this conversation's answer.
  bool listening_{false};

#ifdef ESP8266
  // ESP8266 timer can accept callback with no parameters, so we have this hack to save a static instance of
  // OpenThermPolling
  static OpenThermPolling *instance;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif
};

}  // namespace esphome::opentherm

#endif  // !(USE_ESP32 && SOC_RMT_SUPPORTED)
