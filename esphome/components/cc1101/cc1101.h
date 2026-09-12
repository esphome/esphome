#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "cc1101defs.h"
#include <functional>
#include <vector>

namespace esphome::cc1101 {

enum class CC1101Error { NONE = 0, TIMEOUT, PARAMS, CRC_ERROR, FIFO_OVERFLOW, PLL_LOCK };
using packet_length_func_t = std::function<int32_t(const std::vector<uint8_t> &)>;

class CC1101Listener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) = 0;
};

class CC1101Component final : public Component,
                              public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                    spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  CC1101Component();

  void setup() override;
  void loop() override;
  void dump_config() override;
  void configure();

  // Actions
  void begin_tx();
  void begin_rx();
  void reset();
  void set_idle();

  // GDO Pin Configuration
  void set_gdo0_pin(InternalGPIOPin *pin) { this->gdo0_pin_ = pin; }

  // Configuration Setters
  void set_output_power(float value);
  void set_rx_attenuation(RxAttenuation value);
  void set_dc_blocking_filter(bool value);

  // Tuner settings
  void set_frequency(float value);
  void set_if_frequency(float value);
  void set_filter_bandwidth(float value);
  void set_channel(uint8_t value);
  void set_channel_spacing(float value);
  void set_fsk_deviation(float value);
  void set_msk_deviation(uint8_t value);
  void set_symbol_rate(float value);
  void set_sync_mode(SyncMode value);
  void set_carrier_sense_above_threshold(bool value);
  void set_modulation_type(Modulation value);
  void set_manchester(bool value);
  void set_num_preamble(uint8_t value);
  void set_sync1(uint8_t value);
  void set_sync0(uint8_t value);

  // AGC settings
  void set_magn_target(MagnTarget value);
  void set_max_lna_gain(MaxLnaGain value);
  void set_max_dvga_gain(MaxDvgaGain value);
  void set_carrier_sense_abs_thr(int8_t value);
  void set_carrier_sense_rel_thr(CarrierSenseRelThr value);
  void set_lna_priority(bool value);
  void set_filter_length_fsk_msk(FilterLengthFskMsk value);
  void set_filter_length_ask_ook(FilterLengthAskOok value);
  void set_freeze(Freeze value);
  void set_wait_time(WaitTime value);
  void set_hyst_level(HystLevel value);

  // Frequency offset compensation and bit synchronization settings
  void set_foc_bs_cs_gate(bool value);
  void set_foc_limit(FocLimit value);
  void set_foc_pre_k(FocPreK value);
  void set_foc_post_k(FocPostK value);
  void set_bs_limit(BsLimit value);
  void set_bs_pre_ki(BsPreKi value);
  void set_bs_pre_kp(BsPreKp value);
  void set_bs_post_ki(BsPostKi value);
  void set_bs_post_kp(BsPostKp value);

  // Packet mode settings
  void set_packet_mode(bool value);
  void set_packet_length(uint16_t value);
  void set_packet_length_lambda(packet_length_func_t func);
  void set_crc_enable(bool value);
  void set_whitening(bool value);

  // Packet mode operations
  CC1101Error transmit_packet(const std::vector<uint8_t> &packet);
  void register_listener(CC1101Listener *listener) { this->listeners_.push_back(listener); }
  Trigger<std::vector<uint8_t>, float, float, uint8_t> *get_packet_trigger() { return &this->packet_trigger_; }

 protected:
  uint16_t chip_id_{0};
  bool initialized_{false};

  float output_power_requested_{10.0f};
  float output_power_effective_{10.0f};
  uint8_t pa_table_[PA_TABLE_SIZE]{};

  CC1101State state_;

  // GDO pin for packet reception
  InternalGPIOPin *gdo0_pin_{nullptr};
  static void IRAM_ATTR gpio_intr(CC1101Component *arg);

  // Packet handling
  void call_listeners_(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi);
  void configure_packet_mode_();
  bool is_long_packet_mode_() const;
  void reset_long_packet_rx_();
  void restart_long_packet_rx_();
  void handle_long_packet_(uint8_t rx_bytes);
  void finish_long_packet_();
  void maybe_switch_long_packet_to_fixed_();
  uint32_t packet_idle_timeout_ms_() const;
  Trigger<std::vector<uint8_t>, float, float, uint8_t> packet_trigger_;
  std::vector<uint8_t> packet_;
  std::vector<CC1101Listener *> listeners_;
  packet_length_func_t packet_length_func_{};
  uint16_t packet_length_{0};
  uint16_t packet_expected_length_{0};
  uint32_t packet_last_byte_ms_{0};
  bool packet_mode_{false};
  bool packet_fixed_mode_armed_{false};

  // Low-level Helpers
  uint8_t strobe_(Command cmd);
  void write_(Register reg);
  void write_(Register reg, uint8_t value);
  void write_(Register reg, const uint8_t *buffer, size_t length);
  void read_(Register reg);
  void read_(Register reg, uint8_t *buffer, size_t length);

  // State Management
  bool wait_for_state_(State target_state, uint32_t timeout_ms = 100);
  bool enter_calibrated_(State target_state, Command cmd);
  void enter_idle_();
  bool enter_rx_();
  bool enter_tx_();
};

// Action Wrappers
template<typename... Ts> class BeginTxAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(const Ts &...x) override { this->parent_->begin_tx(); }
};

template<typename... Ts> class BeginRxAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(const Ts &...x) override { this->parent_->begin_rx(); }
};

template<typename... Ts> class ResetAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(const Ts &...x) override { this->parent_->reset(); }
};

template<typename... Ts> class SetIdleAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(const Ts &...x) override { this->parent_->set_idle(); }
};

template<typename... Ts> class SendPacketAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) { this->data_func_ = func; }
  void set_data_static(const uint8_t *data, size_t len) {
    this->data_static_ = data;
    this->data_static_len_ = len;
  }

  void play(const Ts &...x) override {
    if (this->data_func_) {
      auto data = this->data_func_(x...);
      this->parent_->transmit_packet(data);
    } else if (this->data_static_ != nullptr) {
      std::vector<uint8_t> data(this->data_static_, this->data_static_ + this->data_static_len_);
      this->parent_->transmit_packet(data);
    }
  }

 protected:
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  const uint8_t *data_static_{nullptr};
  size_t data_static_len_{0};
};

template<typename... Ts> class SetSymbolRateAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(float, symbol_rate)
  void play(const Ts &...x) override { this->parent_->set_symbol_rate(this->symbol_rate_.value(x...)); }
};

template<typename... Ts> class SetFrequencyAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(float, frequency)
  void play(const Ts &...x) override { this->parent_->set_frequency(this->frequency_.value(x...)); }
};

template<typename... Ts> class SetOutputPowerAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(float, output_power)
  void play(const Ts &...x) override { this->parent_->set_output_power(this->output_power_.value(x...)); }
};

template<typename... Ts> class SetModulationTypeAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(Modulation, modulation_type)
  void play(const Ts &...x) override { this->parent_->set_modulation_type(this->modulation_type_.value(x...)); }
};

template<typename... Ts> class SetRxAttenuationAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(RxAttenuation, rx_attenuation)
  void play(const Ts &...x) override { this->parent_->set_rx_attenuation(this->rx_attenuation_.value(x...)); }
};

template<typename... Ts>
class SetDcBlockingFilterAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(bool, dc_blocking_filter)
  void play(const Ts &...x) override { this->parent_->set_dc_blocking_filter(this->dc_blocking_filter_.value(x...)); }
};

template<typename... Ts> class SetManchesterAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(bool, manchester)
  void play(const Ts &...x) override { this->parent_->set_manchester(this->manchester_.value(x...)); }
};

template<typename... Ts> class SetFilterBandwidthAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(float, filter_bandwidth)
  void play(const Ts &...x) override { this->parent_->set_filter_bandwidth(this->filter_bandwidth_.value(x...)); }
};

template<typename... Ts> class SetFskDeviationAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(float, fsk_deviation)
  void play(const Ts &...x) override { this->parent_->set_fsk_deviation(this->fsk_deviation_.value(x...)); }
};

template<typename... Ts> class SetMskDeviationAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(uint8_t, msk_deviation)
  void play(const Ts &...x) override { this->parent_->set_msk_deviation(this->msk_deviation_.value(x...)); }
};

template<typename... Ts> class SetChannelAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(uint8_t, channel)
  void play(const Ts &...x) override { this->parent_->set_channel(this->channel_.value(x...)); }
};

template<typename... Ts> class SetChannelSpacingAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(float, channel_spacing)
  void play(const Ts &...x) override { this->parent_->set_channel_spacing(this->channel_spacing_.value(x...)); }
};

template<typename... Ts> class SetIfFrequencyAction final : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  TEMPLATABLE_VALUE(float, if_frequency)
  void play(const Ts &...x) override { this->parent_->set_if_frequency(this->if_frequency_.value(x...)); }
};

}  // namespace esphome::cc1101
