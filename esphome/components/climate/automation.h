#pragma once

#include "esphome/core/automation.h"
#include "climate.h"

namespace esphome::climate {

// Unique Empty<Tag> per field so [[no_unique_address]] is guaranteed to coalesce.
namespace climate_control_detail {
template<int Tag> struct Empty {};
}  // namespace climate_control_detail

// X-macro: (type, field_name, call_setter, bit_index). Order and bit values must
// match the FIELDS table in __init__.py. call_setter is the ClimateCall method
// invoked in play() — for custom_fan_mode/custom_preset this dispatches to the
// std::string overload of set_fan_mode/set_preset respectively.
#define CLIMATE_CONTROL_FIELDS(X) \
  X(ClimateMode, mode, set_mode, 0) \
  X(float, target_temperature, set_target_temperature, 1) \
  X(float, target_temperature_low, set_target_temperature_low, 2) \
  X(float, target_temperature_high, set_target_temperature_high, 3) \
  X(float, target_humidity, set_target_humidity, 4) \
  X(ClimateFanMode, fan_mode, set_fan_mode, 5) \
  X(std::string, custom_fan_mode, set_fan_mode, 6) \
  X(ClimatePreset, preset, set_preset, 7) \
  X(std::string, custom_preset, set_preset, 8) \
  X(ClimateSwingMode, swing_mode, set_swing_mode, 9)

template<uint16_t Fields, typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  explicit ControlAction(Climate *climate) : climate_(climate) {}

#define CLIMATE_FIELD_SETTER_(type, name, call_setter, idx) \
  template<typename V> void set_##name(V value) requires((Fields & (1 << (idx))) != 0) { this->name##_ = value; }
#define CLIMATE_FIELD_APPLY_(type, name, call_setter, idx) \
  if constexpr ((Fields & (1 << (idx))) != 0) \
    call.call_setter(this->name##_.value(x...));
#define CLIMATE_FIELD_DECL_(type, name, call_setter, idx) \
  [[no_unique_address]] std::conditional_t<(Fields & (1 << (idx))) != 0, TemplatableStorage<type, Ts...>, \
                                           climate_control_detail::Empty<(idx)>> \
      name##_{};

  CLIMATE_CONTROL_FIELDS(CLIMATE_FIELD_SETTER_)

  void play(const Ts &...x) override {
    auto call = this->climate_->make_call();
    CLIMATE_CONTROL_FIELDS(CLIMATE_FIELD_APPLY_)
    call.perform();
  }

 protected:
  Climate *climate_;
  CLIMATE_CONTROL_FIELDS(CLIMATE_FIELD_DECL_)

#undef CLIMATE_FIELD_DECL_
#undef CLIMATE_FIELD_APPLY_
#undef CLIMATE_FIELD_SETTER_
};
#undef CLIMATE_CONTROL_FIELDS

class ControlTrigger : public Trigger<ClimateCall &> {
 public:
  ControlTrigger(Climate *climate) {
    climate->add_on_control_callback([this](ClimateCall &x) { this->trigger(x); });
  }
};

class StateTrigger : public Trigger<Climate &> {
 public:
  StateTrigger(Climate *climate) {
    climate->add_on_state_callback([this](Climate &x) { this->trigger(x); });
  }
};

}  // namespace esphome::climate
