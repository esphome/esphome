#pragma once

#include <utility>

#include "esphome/core/component.h"

namespace esphome {
namespace light {

class LightState;

class LightEffect {
 public:
  explicit LightEffect(std::string name) : name_(std::move(name)) {}

  /// Initialize this LightEffect. Will be called once after creation.
  virtual void start() {}

  virtual void start_internal() { this->start(); }

  /// Called when this effect is about to be removed
  virtual void stop() {}

  /// Apply this effect. Use the provided state for starting transitions, ...
  virtual void apply() = 0;

  const std::string &get_name() { return this->name_; }

  /// Internal method called by the LightState when this light effect is registered in it.
  virtual void init() {}

  void init_internal(LightState *state) {
    this->state_ = state;
    this->init();
  }

 protected:
  LightState *state_{nullptr};
  std::string name_;
};

}  // namespace light
}  // namespace esphome
