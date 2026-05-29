#pragma once

#include "mitsubishi_cn105.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"

namespace esphome::mitsubishi_cn105 {

class MitsubishiCN105VerticalVaneDirectionSelect;

/// Public vertical vane direction enum for ESPHome-facing APIs.
/// Mirrors MitsubishiCN105::VaneMode to avoid exposing the internal protocol enum
/// in actions, triggers and lambda expressions.
enum VerticalVaneMode : uint8_t {
  VERTICAL_VANE_MODE_AUTO = static_cast<uint8_t>(MitsubishiCN105::VaneMode::AUTO),
  VERTICAL_VANE_MODE_POSITION_1 = static_cast<uint8_t>(MitsubishiCN105::VaneMode::POSITION_1),
  VERTICAL_VANE_MODE_POSITION_2 = static_cast<uint8_t>(MitsubishiCN105::VaneMode::POSITION_2),
  VERTICAL_VANE_MODE_POSITION_3 = static_cast<uint8_t>(MitsubishiCN105::VaneMode::POSITION_3),
  VERTICAL_VANE_MODE_POSITION_4 = static_cast<uint8_t>(MitsubishiCN105::VaneMode::POSITION_4),
  VERTICAL_VANE_MODE_POSITION_5 = static_cast<uint8_t>(MitsubishiCN105::VaneMode::POSITION_5),
  VERTICAL_VANE_MODE_SWING = static_cast<uint8_t>(MitsubishiCN105::VaneMode::SWING),
  VERTICAL_VANE_MODE_UNKNOWN = static_cast<uint8_t>(MitsubishiCN105::VaneMode::UNKNOWN),
};

/// Vane state snapshot used by vane.on_state triggers.
/// The payload contains vane-specific state as well as a reference to the
/// associated climate entity, allowing automations to inspect additional
/// climate properties such as mode, fan mode and swing mode.
struct VaneState {
  struct Vertical {
    VerticalVaneMode direction;
  };

  climate::Climate &climate;
  Vertical vertical;
};

class MitsubishiCN105Climate : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  explicit MitsubishiCN105Climate() : hp_(*this) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void set_update_interval(uint32_t ms) { this->hp_.set_update_interval(ms); }
  void set_current_temperature_min_interval(uint32_t ms) { this->hp_.set_room_temperature_min_interval(ms); }

  void set_remote_temperature(float temperature) { this->hp_.set_remote_temperature(temperature); }
  void clear_remote_temperature() { this->hp_.clear_remote_temperature(); }

  void set_supported_swing_mode(climate::ClimateSwingMode mode);

  void set_vertical_vane_direction_select(MitsubishiCN105VerticalVaneDirectionSelect *select) {
    this->vertical_vane_direction_select_ = select;
  }

  void set_vertical_vane_direction(VerticalVaneMode vane_mode);

  template<typename F> void add_on_vane_state_callback(F &&callback) {
    this->vane_state_callback_.add(std::forward<F>(callback));
  }

 protected:
  void apply_values_();

  void apply_values_if_initialized_() {
    if (this->hp_.is_status_initialized()) {
      this->apply_values_();
    }
  }

  MitsubishiCN105 hp_;
  climate::ClimateSwingModeMask supported_swing_modes_{};
  MitsubishiCN105::VaneMode last_non_swing_vane_mode_{MitsubishiCN105::VaneMode::AUTO};
  MitsubishiCN105::WideVaneMode last_non_swing_wide_vane_mode_{MitsubishiCN105::WideVaneMode::CENTER};
  MitsubishiCN105VerticalVaneDirectionSelect *vertical_vane_direction_select_{nullptr};
  LazyCallbackManager<void(const VaneState &)> vane_state_callback_;
};

template<typename... Ts>
class SetRemoteTemperatureAction : public Action<Ts...>, public Parented<MitsubishiCN105Climate> {
 public:
  TEMPLATABLE_VALUE(float, temperature)

  void play(const Ts &...x) override { this->parent_->set_remote_temperature(this->temperature_.value(x...)); }
};

template<typename... Ts>
class ClearRemoteTemperatureAction : public Action<Ts...>, public Parented<MitsubishiCN105Climate> {
 public:
  void play(const Ts &...x) override { this->parent_->clear_remote_temperature(); }
};

}  // namespace esphome::mitsubishi_cn105
