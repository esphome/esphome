#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include <string>
#include <deque>
#include "cc1101defs.h"
#include "esphome/core/automation.h"
#include "esphome/components/remote_base/rc_switch_protocol.h"

namespace esphome {
namespace cc1101 {

class CC1101Component : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  CC1101Component();

  void set_config_gdo0_pin(InternalGPIOPin *pin) { gdo0_ = pin; }

  void setup() override;
  void dump_config() override;
  void loop() override;

  void begin_tx();
  void end_tx();
  void reset();

  void set_output_power(float value);
  void set_rx_attenuation(RxAttenuation value);
  void set_dc_blocking_filter(bool value);
  void set_tuner_frequency(float value);
  void set_tuner_if_frequency(float value);
  void set_tuner_filter_bandwidth(float value);
  void set_tuner_channel(uint8_t value);
  void set_tuner_channel_spacing(float value);
  void set_tuner_fsk_deviation(float value);
  void set_tuner_msk_deviation(uint8_t value);
  void set_tuner_symbol_rate(float value);
  void set_tuner_sync_mode(SyncMode value);
  void set_tuner_carrier_sense_above_threshold(bool value);
  void set_tuner_modulation_type(Modulation value);
  void set_tuner_manchester(bool value);
  void set_tuner_num_preamble(uint8_t value);
  void set_tuner_sync1(uint8_t value);
  void set_tuner_sync0(uint8_t value);
  void set_tuner_pktlen(uint8_t value);
  void set_agc_magn_target(MagnTarget value);
  void set_agc_max_lna_gain(MaxLnaGain value);
  void set_agc_max_dvga_gain(MaxDvgaGain value);
  void set_agc_carrier_sense_abs_thr(int8_t value);
  void set_agc_carrier_sense_rel_thr(CarrierSenseRelThr value);
  void set_agc_lna_priority(bool value);
  void set_agc_filter_length_fsk_msk(FilterLengthFskMsk value);
  void set_agc_filter_length_ask_ook(FilterLengthAskOok value);
  void set_agc_freeze(Freeze value);
  void set_agc_wait_time(WaitTime value);
  void set_agc_hyst_level(HystLevel value);

 protected:
  enum class ComponentState {
    IDLE,
    SETUP_START,
    SETUP_WAIT_RESET,
    SETUP_WRITE_REGS,
    SETUP_WAIT_RX,
    SET_FREQ_START,
    SET_FREQ_WAIT_IDLE,
    SET_FREQ_WRITE_REGS,
    SET_FREQ_WAIT_RX,
  };

  void process_cmd_queue_();
  std::deque<Command> cmd_queue_{};
  bool is_waiting_{false};
  uint32_t wait_start_time_{0};
  ComponentState component_state_{ComponentState::SETUP_START};

  float requested_freq_{433920000.0f};
  bool freq_request_pending_{false};
  bool first_update_done_{false};

  InternalGPIOPin *gdo0_;
  std::string chip_id_;
  bool reset_;
  float output_power_requested_{0.0f};
  float output_power_effective_{0.0f};
  uint8_t pa_table_[8];
  union {
    struct CC1101State state_;
    uint8_t regs_[sizeof(struct CC1101State) / sizeof(uint8_t)];
  };

  uint8_t strobe_(Command cmd);
  void write_(Register reg);
  void write_(Register reg, uint8_t value);
  void write_(Register reg, uint8_t *buffer, size_t length);
  bool read_(Register reg);
  bool read_(Register reg, uint8_t *buffer, size_t length);
  void send_(Command cmd);
};

template<typename... Ts> class BeginTxAction : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(Ts... x) override { this->parent_->begin_tx(); }
};

template<typename... Ts> class EndTxAction : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(Ts... x) override { this->parent_->end_tx(); }
};

template<typename... Ts> class ResetAction : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(Ts... x) override { this->parent_->reset(); }
};

}  // namespace cc1101
}  // namespace esphome
