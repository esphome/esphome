#pragma once

#ifdef USE_ESP32
#include <soc/soc_caps.h>
#endif

// Active on ESP8266 and on ESP32 variants without the RMT peripheral. RMT capable
// ESP32 variants use opentherm_rmt.h instead.
#if defined(ESP8266) || (defined(USE_ESP32) && !SOC_RMT_SUPPORTED)

#include <string>
#include "opentherm_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <driver/gptimer.h>
#include <esp_err.h>
#endif

namespace esphome::opentherm {

// Timer-based OpenTherm driver. Samples/drives the line from a periodic timer
// interrupt (5 kHz while reading, 2 kHz while writing) and decodes/encodes the
// Manchester frame in software. The protocol state machine is shared across
// platforms; only the timer backend differs (ESP8266 timer1, ESP32 gptimer).
//
// Used for ESP8266 and for ESP32 variants that lack the RMT peripheral. RMT
// capable ESP32 variants use the RMT driver in opentherm_rmt.h instead.
class OpenTherm : public OpenThermBase {
 public:
  OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin);

  bool initialize() override;

  void listen() override;

  void send(OpenthermData &data) override;

  void stop() override;

  void log_protocol_state() const override;

#ifdef USE_ESP32
  static bool timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
#endif
  static bool timer_isr(OpenTherm *arg);
#ifdef ESP8266
  static void esp8266_timer_isr();
#endif

 private:
  ISRInternalGPIOPin isr_in_pin_;
  ISRInternalGPIOPin isr_out_pin_;

  uint32_t capture_{};
  uint8_t clock_{};
  uint8_t bit_pos_{};
  int32_t timeout_counter_ = -1;

#ifdef USE_ESP32
  gptimer_handle_t timer_handle_{nullptr};
  gptimer_alarm_config_t alarm_config_{
      .alarm_count = 0,
      .reload_count = 0,
      .flags = {.auto_reload_on_alarm = true},
  };

  bool init_esp32_timer_();
  void start_esp32_timer_(uint64_t alarm_value);
#endif

  void stop_timer_();

  void read_();               // data detected start reading
  void start_read_timer_();   // reading timer_ to sample at 1/5 of manchester code bit length (at 5kHz)
  void start_write_timer_();  // writing timer_ to send manchester code (at 2kHz)

  void bit_read_(uint8_t value);
  ProtocolErrorType verify_stop_bit_(uint8_t value);
  void write_bit_(uint8_t high, uint8_t clock);

#ifdef ESP8266
  // ESP8266 timer accepts a callback with no parameters, so we keep a static instance pointer for the trampoline.
  static OpenTherm *instance;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif
};

}  // namespace esphome::opentherm

#endif
