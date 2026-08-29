#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/time/real_time_clock.h"

#include "../ds3231.h"

namespace esphome::ds3231 {

class DS3231Time : public time::RealTimeClock, public Parented<DS3231Component> {
 public:
  void update() override;
  void dump_config() override;

  void read_time();
  void write_time();
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<DS3231Time> {
 public:
  void play(const Ts &...x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<DS3231Time> {
 public:
  void play(const Ts &...x) override { this->parent_->read_time(); }
};

}  // namespace esphome::ds3231
