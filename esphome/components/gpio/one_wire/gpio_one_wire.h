#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/one_wire/one_wire.h"

#ifdef USE_ONE_WIRE_RMT
#include <driver/rmt_tx.h>
#include <driver/rmt_rx.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif  // USE_ONE_WIRE_RMT

namespace esphome::gpio {

class GPIOOneWireBus final : public one_wire::OneWireBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_pin(InternalGPIOPin *pin) {
    this->t_pin_ = pin;
#ifndef USE_ONE_WIRE_RMT
    this->pin_ = pin->to_isr();
#endif
  }

  void write8(uint8_t val) override;
  void write64(uint64_t val) override;
  uint8_t read8() override;
  uint64_t read64() override;

 protected:
  InternalGPIOPin *t_pin_{};

  // ROM search state (shared by both RMT and bit-bang implementations).
  // Upstream gpio_one_wire initialises last_discrepancy_ and last_device_flag_
  // but leaves address_ without a default.  All three fields are reset
  // together in reset_search(), so they are initialised consistently here.
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t address_{0};

  int reset_int() override;
  void reset_search() override;
  uint64_t search_int() override;
  bool read_bit_();
  void write_bit_(bool bit);

#ifdef USE_ONE_WIRE_RMT
  rmt_channel_handle_t tx_channel_{nullptr};
  rmt_channel_handle_t rx_channel_{nullptr};
  rmt_encoder_handle_t tx_bytes_encoder_{nullptr};
  rmt_encoder_handle_t tx_copy_encoder_{nullptr};
  rmt_symbol_word_t *rx_symbols_buf_{nullptr};
  QueueHandle_t receive_queue_{nullptr};

  void destroy_();
#else
  ISRInternalGPIOPin pin_;
#endif
};

}  // namespace esphome::gpio
