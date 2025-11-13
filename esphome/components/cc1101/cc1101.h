#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/components/remote_base/rc_switch_protocol.h"
#include "cc1101defs.h"
#include <string>

namespace esphome {
namespace cc1101 {

// A simple fixed-size ring buffer to avoid heap allocation (std::deque) in the loop
template<typename T, size_t MAX_SIZE> class FixedQueue {
 public:
  void push_back(T val) {
    if (size_ >= MAX_SIZE)
      return;  // Drop if full
    buffer_[tail_] = val;
    tail_ = (tail_ + 1) % MAX_SIZE;
    size_++;
  }

  T front() const { return buffer_[head_]; }

  void pop_front() {
    if (size_ == 0)
      return;
    head_ = (head_ + 1) % MAX_SIZE;
    size_--;
  }

  bool empty() const { return size_ == 0; }

  void clear() {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
  }

 private:
  T buffer_[MAX_SIZE];
  size_t head_{0};
  size_t tail_{0};
  size_t size_{0};
};

class CC1101Component : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  CC1101Component();

  // Updated to use generic GPIOPin for better hardware compatibility
  void set_config_gdo0_pin(GPIOPin *pin) { gdo0_ = pin; }

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

  // Replaced std::deque with fixed size buffer (size 16 is sufficient for command chains)
  FixedQueue<Command, 16> cmd_queue_;

  bool is_waiting_{false};
  uint32_t wait_start_time_{0};
  ComponentState component_state_{ComponentState::SETUP_START};

  float requested_freq_{433920000.0f};
  bool freq_request_pending_{false};
  bool first_update_done_{false};

  GPIOPin *gdo0_{nullptr};  // Updated to Generic GPIO pointer
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
