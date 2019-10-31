#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/core/automation.h"
#include "esphome/components/output/float_output.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ledc {

extern uint8_t next_ledc_channel;

class LEDCOutput : public output::FloatOutput, public Component {
 public:
  explicit LEDCOutput(GPIOPin *pin) : pin_(pin) { this->channel_ = next_ledc_channel++; }

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  /// Dynamically change frequency at runtime
  void apply_frequency(float frequency);

  /// Setup LEDC.
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Override FloatOutput's write_state.
  void write_state(float state) override;

 protected:
  GPIOPin *pin_;
  uint8_t channel_{};
  uint8_t bit_depth_{};
  float frequency_{};
  float duty_{0.0f};
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...> {
 public:
  SetFrequencyAction(LEDCOutput *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, frequency);

  void play(Ts... x) {
    float freq = this->frequency_.value(x...);
    this->parent_->apply_frequency(freq);
  }

 protected:
  LEDCOutput *parent_;
};

struct SongActionItem {
  SongActionItem(float frequency, uint32_t end_at) : frequency(frequency), end_at(end_at) {}

  // Frequency to play, 0 for space
  float frequency;
  uint32_t end_at;
};

template<typename... Ts> class SongAction : public Action<Ts...>, public Component {
 public:
  SongAction(LEDCOutput *parent) : parent_(parent) {}
  void set_active_level(float active_level) {
    active_level_ = active_level;
  }
  void set_items(const std::vector<SongActionItem> &items) {
    items_ = items;
  }

  void play(Ts... x) {
    // empty, complex action
  }

  void play_complex(Ts... x) override {
    if (this->is_running_)
      return;

    // Store loop parameters
    this->var_ = std::make_tuple(x...);
    this->is_running_ = true;
    this->high_freq_loop_.start();
    this->started_at_ = millis();
    this->at_ = 0;
  }

  void loop() override {
    if (!this->is_running_)
      return;
    if (this->at_ >= this->items_.size()) {
      // stop running
      this->is_running_ = false;
      this->high_freq_loop_.stop();
      this->parent_->set_level(0.0f);
      this->play_next_tuple(this->var_);
      return;
    }

    const uint32_t now = millis();
    const uint32_t dt = now - this->started_at_;

    // write: whether to write the frequency value to the output
    //   true at first item and any time we progress to the next item
    bool write = this->at_ == 0;
    for (; this->at_ < this->items_.size(); this->at_++) {
      if (dt < this->items_[this->at_].end_at)
        break;
      write = true;
    }
    if (this->at_ >= this->items_.size() || !write)
      // will handle stop in next loop
      return;

    float freq = this->items_[this->at_].frequency;
    if (freq <= 0) {
      this->parent_->set_level(0.0f);
    } else {
      this->parent_->apply_frequency(freq);
      this->parent_->set_level(this->active_level_);
    }
  }

  void stop() override {
    this->is_running_ = false;
    this->high_freq_loop_.stop();
  }

  bool is_running() override {
    return this->is_running_ || this->is_running_next();
  }

 protected:
  bool is_running_;
  uint32_t started_at_;
  HighFrequencyLoopRequester high_freq_loop_;
  LEDCOutput *parent_;
  float active_level_;
  size_t at_;
  std::vector<SongActionItem> items_;
  std::tuple<Ts...> var_{};
};

}  // namespace ledc
}  // namespace esphome

#endif
