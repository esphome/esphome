#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "cc1101defs.h"

namespace esphome::cc1101 {

class CC1101Component : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  CC1101Component();

  void setup() override;
  void dump_config() override;

  // Actions
  void begin_tx();
  void begin_rx();
  void reset();
  void set_idle();

  // GDO Pin Configuration
  void set_gdo0_config(uint8_t value);
  void set_gdo2_config(uint8_t value);

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
  void set_pktlen(uint8_t value);

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

 protected:
  uint16_t chip_id_{0};
  bool initialized_{false};

  float output_power_requested_{10.0f};
  float output_power_effective_{10.0f};
  uint8_t pa_table_[PA_TABLE_SIZE]{};

  CC1101State state_;

  // Low-level Helpers
  uint8_t strobe_(Command cmd);
  void write_(Register reg);
  void write_(Register reg, uint8_t value);
  void write_(Register reg, const uint8_t *buffer, size_t length);
  void read_(Register reg);
  void read_(Register reg, uint8_t *buffer, size_t length);

  // State Management
  bool wait_for_state_(State target_state, uint32_t timeout_ms = 100);
  void enter_idle_();
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

}  // namespace esphome::cc1101
