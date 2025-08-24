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

  /// Get the index of this effect in the parent light's effect list.
  /// Returns 0 if not found or not initialized.
  uint32_t get_index() const;

  /// Check if this effect is currently active.
  bool is_active() const;

  /// Get a reference to the parent light state.
  /// Returns nullptr if not initialized.
  LightState *get_light_state() const { return this->state_; }

 protected:
  LightState *state_{nullptr};
  std::string name_;

  /// Internal method to find this effect's index in the parent light's effect list.
  uint32_t get_index_in_parent_() const;
};

}  // namespace light
}  // namespace esphome
