#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/one_wire/one_wire.h"

#ifdef USE_ONE_WIRE_RMT
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace esphome::gpio {

class GPIOOneWireBus final : public one_wire::OneWireBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_pin(InternalGPIOPin *pin) {
    this->t_pin_ = pin;
    this->pin_ = pin->to_isr();
  }
  void set_use_rmt(bool use_rmt) { this->use_rmt_ = use_rmt; }

  void write8(uint8_t val) override;
  void write64(uint64_t val) override;
  uint8_t read8() override;
  uint64_t read64() override;

 protected:
  InternalGPIOPin *t_pin_{};
  ISRInternalGPIOPin pin_;
  bool use_rmt_{false};

  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t address_{0};

  int reset_int() override;
  void reset_search() override;
  uint64_t search_int() override;
  void write_bit_(bool bit);
  bool read_bit_();

#ifdef USE_ONE_WIRE_RMT
  rmt_channel_handle_t tx_channel_{nullptr};
  rmt_channel_handle_t rx_channel_{nullptr};
  rmt_encoder_handle_t tx_bytes_encoder_{nullptr};
  rmt_encoder_handle_t tx_copy_encoder_{nullptr};
  rmt_symbol_word_t *rx_symbols_buf_{nullptr};
  QueueHandle_t receive_queue_{nullptr};

  void setup_rmt_();
  void destroy_rmt_();
  int reset_rmt_();
  void write8_rmt_(uint8_t val);
  void write64_rmt_(uint64_t val);
  uint8_t read8_rmt_();
  uint64_t read64_rmt_();
  void write_bit_rmt_(bool bit);
  bool read_bit_rmt_();
  uint64_t search_rmt_();
#endif
};

}  // namespace esphome::gpio
