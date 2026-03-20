#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tm1651 {

enum TM1651Brightness : uint8_t {
  TM1651_DARKEST = 1,
  TM1651_TYPICAL = 2,
  TM1651_BRIGHTEST = 3,
};

class TM1651Display : public Component {
 public:
  void set_clk_pin(InternalGPIOPin *pin) { clk_pin_ = pin; }
  void set_dio_pin(InternalGPIOPin *pin) { dio_pin_ = pin; }

  void setup() override;
  void dump_config() override;

  void set_brightness(uint8_t new_brightness);
  void set_brightness(TM1651Brightness new_brightness) { this->set_brightness(static_cast<uint8_t>(new_brightness)); }

  void set_level(uint8_t new_level);
  void set_level_percent(uint8_t percentage);

  void turn_off();
  void turn_on();

 protected:
  uint8_t calculate_level_(uint8_t percentage);
  void display_level_();

  uint8_t remap_brightness_(uint8_t new_brightness);
  void update_brightness_(uint8_t on_off_control);

  // low level functions
  bool write_byte_(uint8_t data);

  void half_cycle_clock_low_(bool data_bit);
  void half_cycle_clock_high_();
  bool half_cycle_clock_high_ack_();

  void start_();
  void stop_();

  void delineate_transmission_(bool dio_state);

  InternalGPIOPin *clk_pin_;
  InternalGPIOPin *dio_pin_;

  bool display_on_{true};
  uint8_t brightness_{};
  uint8_t level_{0};
};

template<typename... Ts> class SetBrightnessAction : public Action<Ts...>, public Parented<TM1651Display> {
 public:
  TEMPLATABLE_VALUE(uint8_t, brightness)

  void play(const Ts &...x) override {
    auto brightness = this->brightness_.value(x...);
    this->parent_->set_brightness(brightness);
  }
};

template<typename... Ts> class SetLevelAction : public Action<Ts...>, public Parented<TM1651Display> {
 public:
  TEMPLATABLE_VALUE(uint8_t, level)

  void play(const Ts &...x) override {
    auto level = this->level_.value(x...);
    this->parent_->set_level(level);
  }
};

template<typename... Ts> class SetLevelPercentAction : public Action<Ts...>, public Parented<TM1651Display> {
 public:
  TEMPLATABLE_VALUE(uint8_t, level_percent)

  void play(const Ts &...x) override {
    auto level_percent = this->level_percent_.value(x...);
    this->parent_->set_level_percent(level_percent);
  }
};

template<typename... Ts> class TurnOnAction : public Action<Ts...>, public Parented<TM1651Display> {
 public:
  void play(const Ts &...x) override { this->parent_->turn_on(); }
};

template<typename... Ts> class TurnOffAction : public Action<Ts...>, public Parented<TM1651Display> {
 public:
  void play(const Ts &...x) override { this->parent_->turn_off(); }
};

}  // namespace tm1651
}  // namespace esphome
