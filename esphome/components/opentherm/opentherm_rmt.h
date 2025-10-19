#pragma once

#ifdef USE_ESP32

#include <string>
#include "opentherm_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <esp_err.h>
#include <driver/gpio.h>

namespace esphome {
namespace opentherm {

class OpenTherm : public OpenThermBase {
 public:
  OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin);

  bool initialize() override;

  void listen() override;

  void send(OpenthermData &data) override;

  void stop() override;

  void log_protocol_state() const override;

 private:
  // RMT resources
  rmt_channel_handle_t rx_channel_{};
  rmt_channel_handle_t tx_channel_{};
  rmt_receive_config_t rx_config_{};
  rmt_encoder_handle_t tx_encoder_{};
  // RX buffer for one OpenTherm frame
  // One OpenTherm frame contains 34 Manchester symbols (start + 32 data + stop) and we
  // allow a little slack for diagnostic captures. Current ESP32 variants have 48 or 64
  // words per buffer which is more than enough.
  static constexpr size_t RMT_SYMBOL_CAPACITY = 40;

  rmt_symbol_word_t rmt_buffer_[RMT_SYMBOL_CAPACITY]{};
  size_t rmt_buffer_symbol_count_{};
  // RMT clock resolution in Hz (1 MHz => 1 tick == 1 us)
  static constexpr uint32_t RMT_RESOLUTION_HZ = 1000000u;

  size_t bit_index_{};

  bool rmt_init_();
  void rmt_read_();
  void rmt_write_();
  static bool rmt_read_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *evt, void *arg);

  void set_protocol_error_(ProtocolErrorType error_type);

  bool decode_rmt_symbols_(size_t num_symbols);
};

}  // namespace opentherm
}  // namespace esphome

#endif
