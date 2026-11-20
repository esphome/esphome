#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ONE_WIRE_RMT

#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>

#include "esphome/components/one_wire/one_wire.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "rmt_one_wire_engine.h"

namespace esphome::gpio {

/// 1-Wire over two RMT channels on one open-drain pin. The hardware runs each section of a transaction,
/// the CPU only hands them over. Needs an external pullup: unlike bit-bang, the line is never driven High.
class GPIORMTOneWireBus final : public one_wire::OneWireBus, public Component, public rmt_one_wire::Port {
 public:
  explicit GPIORMTOneWireBus(const rmt_one_wire::Timing &timing = {});

  void setup() override;
  void loop() override;
  void on_shutdown() override;
  void dump_config() override;
  // After LED strips, whose default takes all TX memory: this option must not push out a running strip
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }

  void write8(uint8_t val) override { this->engine_.write_blocking(&val, 8); }
  void write64(uint64_t val) override { this->engine_.write_blocking(reinterpret_cast<const uint8_t *>(&val), 64); }
  uint8_t read8() override;
  uint64_t read64() override;
  bool submit(one_wire::Transaction *transaction) override;

  bool line_is_high() override { return this->pin_->digital_read(); }
  bool arm_rx(rmt_one_wire::Symbol *buffer, size_t capacity, uint32_t idle_us) override;
  bool start_tx(const rmt_one_wire::Symbol *symbols, size_t count) override;
  bool abort() override;
  uint32_t now_ms() override { return millis(); }
  void wait_for_event(uint32_t max_ms) override;

 protected:
  // Not -1, which reads as "bus held low": mark_failed() already named the cause
  int reset_int() override { return this->is_failed() ? 0 : this->engine_.reset_blocking(); }
  void reset_search() override { this->engine_.reset_search(); }
  uint64_t search_int() override { return this->engine_.search_next(); }

  /// True if the call succeeded. Otherwise logs, and the bus is failed from now on.
  bool check_(esp_err_t error, const LogString *step);

  static bool tx_done_isr(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *data, void *context);
  static bool rx_done_isr(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *data, void *context);

  InternalGPIOPin *pin_{nullptr};
  rmt_one_wire::Timing timing_;
  rmt_channel_handle_t tx_channel_{nullptr};
  rmt_channel_handle_t rx_channel_{nullptr};
  rmt_encoder_handle_t encoder_{nullptr};
  rmt_one_wire::Engine engine_;
};

}  // namespace esphome::gpio

#endif  // USE_ONE_WIRE_RMT
