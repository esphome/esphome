#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ONE_WIRE_RMT

#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <sdkconfig.h>

#include "esphome/components/one_wire/one_wire.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "rmt_one_wire_engine.h"

namespace esphome::gpio {

/// 1-Wire over two RMT channels on one open-drain pin. The hardware runs each section of a transaction,
/// the CPU only hands them over. Needs an external pullup: unlike bit-bang, the line is never driven High.
/// So parasite-powered devices are not supported: they need a strong pullup during a conversion or a write
/// to EEPROM, which this bus does not drive.
class GPIORMTOneWireBus final : public one_wire::OneWireBus, public Component, public rmt_one_wire::Port {
 public:
  /// Codegen always passes the timing, with the defaults of the Python schema when rmt_timing is not set.
  explicit GPIORMTOneWireBus(const rmt_one_wire::Timing &timing);

  void setup() override;
  void loop() override;
  void on_shutdown() override;
  void dump_config() override;
  // After LED strips, whose default takes all TX memory: this option must not push out a running strip
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }

  void write8(uint8_t val) override { this->write_(&val, 8); }
  void write64(uint64_t val) override { this->write_(reinterpret_cast<const uint8_t *>(&val), 64); }
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
  int reset_int() override;
  void reset_search() override { this->engine_.reset_search(); }
  uint64_t search_int() override;

  void write_(const uint8_t *data, uint8_t num_bits);
  /// Leaves data as it is when the call is dropped, so the caller sets all bits to 1 first.
  void read_(uint8_t *data, uint8_t num_bits);
  /// False inside an engine call, which only a log handler can reach. The engine would turn such a call away. It is
  /// only counted, because a log at this point would run inside that engine call.
  bool may_call_engine_();
  /// Logs the calls that may_call_engine_() turned away, once no engine call runs.
  void log_dropped_calls_();

  /// True if the call succeeded. Otherwise logs, and the bus is failed from now on.
  bool check_(esp_err_t error, const LogString *step);
  /// Enables the channels that are disabled. False if that failed, check_() has failed the bus then.
  bool enable_channels_();
  /// Disables the enabled channels and deletes the channels and the encoder, after a failed setup().
  void release_();
#ifdef CONFIG_PM_ENABLE
  /// Each enabled channel holds a power management lock, which keeps the CPU at full speed and stops light sleep.
  /// So the channels are only enabled while the bus has work. This enables them before a blocking call or a queued
  /// transaction, and makes loop() run, which disables them again once the bus is idle.
  void wake_channels_();
  /// Disables the enabled channels. False if one could not be disabled yet: loop() tries again on its next pass.
  bool sleep_channels_();
#else
  void wake_channels_() {}
#endif

  static bool tx_done_isr(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *data, void *context);
  static bool rx_done_isr(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *data, void *context);

  InternalGPIOPin *pin_{nullptr};
  rmt_channel_handle_t tx_channel_{nullptr};
  rmt_channel_handle_t rx_channel_{nullptr};
  rmt_encoder_handle_t encoder_{nullptr};
  /// Set by every successful rmt_enable() and cleared by every successful rmt_disable(), so no code path disables
  /// a channel twice.
  bool rx_enabled_{false};
  bool tx_enabled_{false};
  rmt_one_wire::Engine engine_;
  /// The deadline of the poll timeout that loop() set, in millis(). Only valid while poll_timeout_set_.
  uint32_t poll_timeout_at_ms_{0};
  bool poll_timeout_set_{false};
  uint16_t dropped_calls_{0};
};

}  // namespace esphome::gpio

#endif  // USE_ONE_WIRE_RMT
