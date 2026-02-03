#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "cc1101defs.h"
#include <vector>

namespace esphome::cc1101 {

enum class CC1101Error { NONE = 0, TIMEOUT, PARAMS, CRC_ERROR, FIFO_OVERFLOW, PLL_LOCK };

class CC1101Listener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) = 0;
};

class CC1101Component : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  CC1101Component();

  void setup() override;
  void loop() override;
  void dump_config() override;

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

  // Packet mode settings
  void set_packet_mode(bool value);
  void set_packet_length(uint8_t value);
  void set_crc_enable(bool value);
  void set_whitening(bool value);

  // Configuration Getters
  float get_output_power() { return this->output_power_effective_; }
  RxAttenuation get_rx_attenuation() { return static_cast<RxAttenuation>(this->state_.CLOSE_IN_RX); }
  bool get_dc_blocking_filter() { return this->state_.DEM_DCFILT_OFF == 0; }
  float get_frequency();
  float get_if_frequency();
  float get_filter_bandwidth();
  uint8_t get_channel() { return this->state_.CHANNR; }
  float get_channel_spacing();
  float get_fsk_deviation();
  uint8_t get_msk_deviation() { return this->state_.DEVIATION_M + 1; }
  float get_symbol_rate();
  SyncMode get_sync_mode() { return static_cast<SyncMode>(this->state_.SYNC_MODE); }
  bool get_carrier_sense_above_threshold() { return this->state_.CARRIER_SENSE_ABOVE_THRESHOLD; }
  Modulation get_modulation_type() { return static_cast<Modulation>(this->state_.MOD_FORMAT); }
  bool get_manchester() { return this->state_.MANCHESTER_EN; }
  uint8_t get_num_preamble() { return this->state_.NUM_PREAMBLE; }
  uint8_t get_sync1() { return this->state_.SYNC1; }
  uint8_t get_sync0() { return this->state_.SYNC0; }
  MagnTarget get_magn_target() { return static_cast<MagnTarget>(this->state_.MAGN_TARGET); }
  MaxLnaGain get_max_lna_gain() { return static_cast<MaxLnaGain>(this->state_.MAX_LNA_GAIN); }
  MaxDvgaGain get_max_dvga_gain() { return static_cast<MaxDvgaGain>(this->state_.MAX_DVGA_GAIN); }
  int8_t get_carrier_sense_abs_thr();
  CarrierSenseRelThr get_carrier_sense_rel_thr() {
    return static_cast<CarrierSenseRelThr>(this->state_.CARRIER_SENSE_REL_THR);
  }
  bool get_lna_priority() { return this->state_.AGC_LNA_PRIORITY; }
  FilterLengthFskMsk get_filter_length_fsk_msk() {
    return static_cast<FilterLengthFskMsk>(this->state_.FILTER_LENGTH);
  }
  FilterLengthAskOok get_filter_length_ask_ook() {
    return static_cast<FilterLengthAskOok>(this->state_.FILTER_LENGTH);
  }
  Freeze get_freeze() { return static_cast<Freeze>(this->state_.AGC_FREEZE); }
  WaitTime get_wait_time() { return static_cast<WaitTime>(this->state_.WAIT_TIME); }
  HystLevel get_hyst_level() { return static_cast<HystLevel>(this->state_.HYST_LEVEL); }
  bool get_packet_mode() {
    return this->state_.PKT_FORMAT == static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_FIFO);
  }
  uint8_t get_packet_length() { return this->state_.PKTLEN; }
  bool get_crc_enable() { return this->state_.CRC_EN; }
  bool get_whitening() { return this->state_.WHITE_DATA; }

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

  // Packet handling
  void call_listeners_(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi);
  Trigger<std::vector<uint8_t>, float, float, uint8_t> packet_trigger_;
  std::vector<uint8_t> packet_;
  std::vector<CC1101Listener *> listeners_;

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
template<typename... Ts> class BeginTxAction : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(const Ts &...x) override { this->parent_->begin_tx(); }
};

template<typename... Ts> class BeginRxAction : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(const Ts &...x) override { this->parent_->begin_rx(); }
};

template<typename... Ts> class ResetAction : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(const Ts &...x) override { this->parent_->reset(); }
};

template<typename... Ts> class SetIdleAction : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(const Ts &...x) override { this->parent_->set_idle(); }
};

template<typename... Ts> class SendPacketAction : public Action<Ts...>, public Parented<CC1101Component> {
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

}  // namespace esphome::cc1101
