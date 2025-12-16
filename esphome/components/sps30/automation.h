#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "sps30.h"

namespace esphome {
namespace sps30 {

template<typename... Ts> class StartFanAction : public Action<Ts...>, public Parented<SPS30Component> {
 public:
  void play(const Ts &...x) override { this->parent_->start_fan_cleaning(); }
};

template<typename... Ts> class StartMeasurementAction : public Action<Ts...>, public Parented<SPS30Component> {
 public:
  void play(const Ts &...x) override { this->parent_->start_measurement(); }
};

template<typename... Ts> class StopMeasurementAction : public Action<Ts...>, public Parented<SPS30Component> {
 public:
  void play(const Ts &...x) override { this->parent_->stop_measurement(); }
};

}  // namespace sps30
}  // namespace esphome
