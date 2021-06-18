#include "esphome/core/log.h"
#include "midea_climate.h"

namespace esphome {
namespace midea_ac {

static const char *TAG = "midea_ac";

static void set_sensor(sensor::Sensor *sensor, float value) {
  if (sensor != nullptr && (!sensor->has_state() || sensor->get_raw_state() != value))
    sensor->publish_state(value);
}

template<typename T> void set_property(T &property, T value, bool &flag) {
  if (property != value) {
    property = value;
    flag = true;
  }
}

void MideaAC::on_frame(const midea_dongle::Frame &frame) {
  const auto p = frame.as<PropertiesFrame>();
  if (p.has_power_info()) {
    set_sensor(this->power_sensor_, p.get_power_usage());
    return;
  } else if (!p.has_properties()) {
    ESP_LOGW(TAG, "RX: frame has unknown type");
    return;
  }
  if (p.get_type() == midea_dongle::MideaMessageType::DEVICE_CONTROL) {
    ESP_LOGD(TAG, "RX: control frame");
    this->ctrl_request_ = false;
  } else {
    ESP_LOGD(TAG, "RX: query frame");
  }
  if (this->ctrl_request_)
    return;
  this->cmd_frame_.set_properties(p);  // copy properties from response
  bool need_publish = false;
  set_property(this->mode, p.get_mode(), need_publish);
  set_property(this->target_temperature, p.get_target_temp(), need_publish);
  set_property(this->current_temperature, p.get_indoor_temp(), need_publish);
  if (p.is_custom_fan_mode()) {
    this->fan_mode.reset();
    optional<std::string> mode = p.get_custom_fan_mode();
    set_property(this->custom_fan_mode, mode, need_publish);
  } else {
    this->custom_fan_mode.reset();
    optional<climate::ClimateFanMode> mode = p.get_fan_mode();
    set_property(this->fan_mode, mode, need_publish);
  }
  set_property(this->swing_mode, p.get_swing_mode(), need_publish);
  if (p.is_custom_preset()) {
    this->preset.reset();
    optional<std::string> preset = p.get_custom_preset();
    set_property(this->custom_preset, preset, need_publish);
  } else {
    this->custom_preset.reset();
    set_property(this->preset, p.get_preset(), need_publish);
  }
  if (need_publish)
    this->publish_state();
  set_sensor(this->outdoor_sensor_, p.get_outdoor_temp());
  set_sensor(this->humidity_sensor_, p.get_humidity_setpoint());
}

void MideaAC::on_update() {
  if (this->ctrl_request_) {
    ESP_LOGD(TAG, "TX: control");
    this->parent_->write_frame(this->cmd_frame_);
  } else {
    ESP_LOGD(TAG, "TX: query");
    if (this->power_sensor_ == nullptr || this->request_num_++ % 32)
      this->parent_->write_frame(this->query_frame_);
    else
      this->parent_->write_frame(this->power_frame_);
  }
}

bool MideaAC::allow_preset(climate::ClimatePreset preset) const {
  switch (preset) {
    case climate::CLIMATE_PRESET_ECO:
      if (this->mode == climate::CLIMATE_MODE_COOL) {
        return true;
      } else {
        ESP_LOGD(TAG, "ECO preset is only available in COOL mode");
      }
      break;
    case climate::CLIMATE_PRESET_SLEEP:
      if (this->mode == climate::CLIMATE_MODE_FAN_ONLY || this->mode == climate::CLIMATE_MODE_DRY) {
        ESP_LOGD(TAG, "SLEEP preset is not available in FAN_ONLY or DRY mode");
      } else {
        return true;
      }
      break;
    case climate::CLIMATE_PRESET_BOOST:
      if (this->mode == climate::CLIMATE_MODE_HEAT || this->mode == climate::CLIMATE_MODE_COOL) {
        return true;
      } else {
        ESP_LOGD(TAG, "BOOST preset is only available in HEAT or COOL mode");
      }
      break;
    case climate::CLIMATE_PRESET_HOME:
      return true;
    default:
      break;
  }
  return false;
}

bool MideaAC::allow_custom_preset(const std::string &custom_preset) const {
  if (custom_preset == MIDEA_FREEZE_PROTECTION_PRESET) {
    if (this->mode == climate::CLIMATE_MODE_HEAT) {
      return true;
    } else {
      ESP_LOGD(TAG, "%s is only available in HEAT mode", MIDEA_FREEZE_PROTECTION_PRESET.c_str());
    }
  }
  return false;
}

void MideaAC::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value() && call.get_mode().value() != this->mode) {
    this->cmd_frame_.set_mode(call.get_mode().value());
    this->ctrl_request_ = true;
  }
  if (call.get_target_temperature().has_value() && call.get_target_temperature().value() != this->target_temperature) {
    this->cmd_frame_.set_target_temp(call.get_target_temperature().value());
    this->ctrl_request_ = true;
  }
  if (call.get_fan_mode().has_value() &&
      (!this->fan_mode.has_value() || this->fan_mode.value() != call.get_fan_mode().value())) {
    this->custom_fan_mode.reset();
    this->cmd_frame_.set_fan_mode(call.get_fan_mode().value());
    this->ctrl_request_ = true;
  }
  if (call.get_custom_fan_mode().has_value() &&
      (!this->custom_fan_mode.has_value() || this->custom_fan_mode.value() != call.get_custom_fan_mode().value())) {
    this->fan_mode.reset();
    this->cmd_frame_.set_custom_fan_mode(call.get_custom_fan_mode().value());
    this->ctrl_request_ = true;
  }
  if (call.get_swing_mode().has_value() && call.get_swing_mode().value() != this->swing_mode) {
    this->cmd_frame_.set_swing_mode(call.get_swing_mode().value());
    this->ctrl_request_ = true;
  }
  if (call.get_preset().has_value() && this->allow_preset(call.get_preset().value()) &&
      (!this->preset.has_value() || this->preset.value() != call.get_preset().value())) {
    this->custom_preset.reset();
    this->cmd_frame_.set_preset(call.get_preset().value());
    this->ctrl_request_ = true;
  }
  if (call.get_custom_preset().has_value() && this->allow_custom_preset(call.get_custom_preset().value()) &&
      (!this->custom_preset.has_value() || this->custom_preset.value() != call.get_custom_preset().value())) {
    this->preset.reset();
    this->cmd_frame_.set_custom_preset(call.get_custom_preset().value());
    this->ctrl_request_ = true;
  }
  if (this->ctrl_request_) {
    this->cmd_frame_.set_beeper_feedback(this->beeper_feedback_);
    this->cmd_frame_.finalize();
  }
}

climate::ClimateTraits MideaAC::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_visual_min_temperature(17);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(0.5);
  traits.set_supports_heat_cool_mode(true);
  traits.set_supports_cool_mode(true);
  traits.set_supports_dry_mode(true);
  traits.set_supports_heat_mode(true);
  traits.set_supports_fan_only_mode(true);
  traits.set_supports_fan_mode_auto(true);
  traits.set_supports_fan_mode_low(true);
  traits.set_supports_fan_mode_medium(true);
  traits.set_supports_fan_mode_high(true);
  traits.set_supported_custom_fan_modes(this->traits_custom_fan_modes_);
  traits.set_supports_swing_mode_off(true);
  traits.set_supports_swing_mode_vertical(true);
  traits.set_supports_swing_mode_horizontal(this->traits_swing_horizontal_);
  traits.set_supports_swing_mode_both(this->traits_swing_both_);
  traits.set_supports_preset_home(true);
  traits.set_supports_preset_eco(this->traits_preset_eco_);
  traits.set_supports_preset_sleep(this->traits_preset_sleep_);
  traits.set_supports_preset_boost(this->traits_preset_boost_);
  traits.set_supported_custom_presets(this->traits_custom_presets_);
  traits.set_supports_current_temperature(true);
  return traits;
}

}  // namespace midea_ac
}  // namespace esphome
