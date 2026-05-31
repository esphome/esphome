#include "comp_hardware.h"

#include <cstdio>
#include <string>

#include "comp_reconnect_button.h"
#include "comp_vane_horizontal.h"
#include "comp_vane_vertical.h"

namespace esphome::sharp_ac {

void ESPHomeStateCallback::on_state_update() {
  if (this->sharp_ac_ != nullptr) {
    this->sharp_ac_->publish_update();
  }
}

void ESPHomeStateCallback::on_ion_state_update(bool state) {}

void ESPHomeStateCallback::on_vane_horizontal_update(SwingHorizontal val) {}

void ESPHomeStateCallback::on_vane_vertical_update(SwingVertical val) {}

void ESPHomeStateCallback::on_connection_status_update(int status) {
  if (this->sharp_ac_ != nullptr) {
    this->sharp_ac_->update_connection_status(status);
  }
}

SharpAc::SharpAc() {
  this->hardware_interface_ = std::make_unique<ESPHomeHardwareInterface>(this);
  this->state_callback_ = std::make_unique<ESPHomeStateCallback>(this);
  this->core_ = std::make_unique<SharpAcCore>(this->hardware_interface_.get(), this->state_callback_.get());
}

ClimateTraits SharpAc::traits() {
  auto traits = esphome::climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
  traits.set_visual_min_temperature(16);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(1.0);

  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_AUTO);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_LOW);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_MEDIUM);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_HIGH);

  traits.add_supported_mode(ClimateMode::CLIMATE_MODE_OFF);
  traits.add_supported_mode(ClimateMode::CLIMATE_MODE_COOL);
  traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT);
  traits.add_supported_mode(ClimateMode::CLIMATE_MODE_DRY);
  traits.add_supported_mode(ClimateMode::CLIMATE_MODE_FAN_ONLY);

  traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_ECO);
  traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_BOOST);
  traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_NONE);

  traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_OFF);
  traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_BOTH);
  traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_HORIZONTAL);
  traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_VERTICAL);

  return traits;
}

void SharpAc::dump_config() {
  LOG_CLIMATE("", "Sharp AC Climate", this);
  this->check_uart_settings(9600, 1, uart::UART_CONFIG_PARITY_EVEN, 8);

  ESP_LOGCONFIG(TAG, "  Connection status sensor: %s", this->connection_status_sensor_ != nullptr ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Ion switch: %s", this->ion_switch_ != nullptr ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Horizontal vane select: %s", this->vane_horizontal_ != nullptr ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Vertical vane select: %s", this->vane_vertical_ != nullptr ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Reconnect button: %s", this->reconnect_button_ != nullptr ? "yes" : "no");
}

void SharpAc::publish_update() {
  const auto &state = this->core_->get_state();

  this->target_temperature = state.temperature;
  this->current_temperature = this->core_->get_current_temperature();

  switch (state.fan) {
    case FanMode::FAN_AUTO:
      this->fan_mode = ClimateFanMode::CLIMATE_FAN_AUTO;
      break;
    case FanMode::FAN_LOW:
      this->fan_mode = ClimateFanMode::CLIMATE_FAN_LOW;
      break;
    case FanMode::FAN_MID:
      this->fan_mode = ClimateFanMode::CLIMATE_FAN_MEDIUM;
      break;
    case FanMode::FAN_HIGH:
    case FanMode::FAN_HIGHEST:
      this->fan_mode = ClimateFanMode::CLIMATE_FAN_HIGH;
      break;
    default:
      ESP_LOGD("sharp_ac", "UNKNOWN FAN MODE");
  }

  switch (state.mode) {
    case PowerMode::FAN:
      this->mode = ClimateMode::CLIMATE_MODE_FAN_ONLY;
      break;
    case PowerMode::COOL:
      this->mode = ClimateMode::CLIMATE_MODE_COOL;
      break;
    case PowerMode::HEAT:
      this->mode = ClimateMode::CLIMATE_MODE_HEAT;
      break;
    case PowerMode::DRY:
      this->mode = ClimateMode::CLIMATE_MODE_DRY;
      break;
    default:
      ESP_LOGD("sharp_ac", "UNKNOWN MODE");
  }

  if (!state.state) {
    this->mode = ClimateMode::CLIMATE_MODE_OFF;
    this->action = climate::ClimateAction::CLIMATE_ACTION_OFF;
  } else {
    switch (state.mode) {
      case PowerMode::FAN:
        this->action = climate::ClimateAction::CLIMATE_ACTION_FAN;
        break;
      case PowerMode::COOL:
        this->action = climate::ClimateAction::CLIMATE_ACTION_COOLING;
        break;
      case PowerMode::HEAT:
        this->action = climate::ClimateAction::CLIMATE_ACTION_HEATING;
        break;
      case PowerMode::DRY:
        this->action = climate::ClimateAction::CLIMATE_ACTION_DRYING;
        break;
      default:
        this->action = climate::ClimateAction::CLIMATE_ACTION_IDLE;
        break;
    }
  }

  switch (state.preset) {
    case Preset::ECO:
      this->preset = ClimatePreset::CLIMATE_PRESET_ECO;
      break;
    case Preset::FULLPOWER:
      this->preset = ClimatePreset::CLIMATE_PRESET_BOOST;
      break;
    default:
      this->preset = ClimatePreset::CLIMATE_PRESET_NONE;
      break;
  }

  if (state.swingH == SwingHorizontal::SWING && state.swingV == SwingVertical::SWING) {
    this->swing_mode = ClimateSwingMode::CLIMATE_SWING_BOTH;
  } else if (state.swingH == SwingHorizontal::SWING) {
    this->swing_mode = ClimateSwingMode::CLIMATE_SWING_HORIZONTAL;
  } else if (state.swingV == SwingVertical::SWING) {
    this->swing_mode = ClimateSwingMode::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = ClimateSwingMode::CLIMATE_SWING_OFF;
  }

  if (this->ion_switch_ != nullptr) {
    this->ion_switch_->publish_state(state.ion);
  }

  if (this->vane_horizontal_ != nullptr) {
    this->vane_horizontal_->set_val(state.swingH);
  }

  if (this->vane_vertical_ != nullptr) {
    this->vane_vertical_->set_val(state.swingV);
  }

  this->publish_state();
}

void SharpAc::control(const ClimateCall &call) {
  ESP_LOGD("sharp_ac", "=== Climate Control Called ===");

  if (call.get_mode().has_value()) {
    ClimateMode new_mode = call.get_mode().value();
    ESP_LOGD("sharp_ac", "Setting mode: %d (%s)", (int) new_mode,
             new_mode == ClimateMode::CLIMATE_MODE_OFF        ? "OFF"
             : new_mode == ClimateMode::CLIMATE_MODE_COOL     ? "COOL"
             : new_mode == ClimateMode::CLIMATE_MODE_HEAT     ? "HEAT"
             : new_mode == ClimateMode::CLIMATE_MODE_DRY      ? "DRY"
             : new_mode == ClimateMode::CLIMATE_MODE_FAN_ONLY ? "FAN"
                                                              : "UNKNOWN");

    switch (new_mode) {
      case ClimateMode::CLIMATE_MODE_OFF:
        this->core_->control_mode(PowerMode::COOL, false);
        break;
      case ClimateMode::CLIMATE_MODE_COOL:
        this->core_->control_mode(PowerMode::COOL, true);
        break;
      case ClimateMode::CLIMATE_MODE_HEAT:
        this->core_->control_mode(PowerMode::HEAT, true);
        break;
      case ClimateMode::CLIMATE_MODE_DRY:
        this->core_->control_mode(PowerMode::DRY, true);
        break;
      case ClimateMode::CLIMATE_MODE_FAN_ONLY:
        this->core_->control_mode(PowerMode::FAN, true);
        break;
      default:
        ESP_LOGE("sharp_ac", "Unsupported mode: %d", (int) new_mode);
    }
  }

  if (call.get_target_temperature().has_value()) {
    float temp = call.get_target_temperature().value();
    this->core_->control_temperature((int) temp);
  }

  if (call.get_fan_mode().has_value()) {
    ClimateFanMode fan_mode = call.get_fan_mode().value();
    switch (fan_mode) {
      case ClimateFanMode::CLIMATE_FAN_AUTO:
        this->core_->control_fan(FanMode::FAN_AUTO);
        break;
      case ClimateFanMode::CLIMATE_FAN_LOW:
        this->core_->control_fan(FanMode::FAN_LOW);
        break;
      case ClimateFanMode::CLIMATE_FAN_MEDIUM:
        this->core_->control_fan(FanMode::FAN_MID);
        break;
      case ClimateFanMode::CLIMATE_FAN_HIGH:
        this->core_->control_fan(FanMode::FAN_HIGHEST);
        break;
      default:
        ESP_LOGE("sharp_ac", "Unsupported fan mode: %d", (int) fan_mode);
    }
  }

  if (call.get_preset().has_value()) {
    ClimatePreset preset = call.get_preset().value();
    switch (preset) {
      case ClimatePreset::CLIMATE_PRESET_ECO:
        this->core_->control_preset(Preset::ECO);
        break;
      case ClimatePreset::CLIMATE_PRESET_BOOST:
        this->core_->control_preset(Preset::FULLPOWER);
        break;
      default:
        this->core_->control_preset(Preset::NONE);
        break;
    }
  }

  if (call.get_swing_mode().has_value()) {
    ClimateSwingMode swing_mode = call.get_swing_mode().value();
    switch (swing_mode) {
      case ClimateSwingMode::CLIMATE_SWING_OFF:
        this->core_->control_swing(SwingHorizontal::MIDDLE, SwingVertical::MID);
        break;
      case ClimateSwingMode::CLIMATE_SWING_BOTH:
        this->core_->control_swing(SwingHorizontal::SWING, SwingVertical::SWING);
        break;
      case ClimateSwingMode::CLIMATE_SWING_HORIZONTAL:
        this->core_->control_swing(SwingHorizontal::SWING, SwingVertical::MID);
        break;
      case ClimateSwingMode::CLIMATE_SWING_VERTICAL:
        this->core_->control_swing(SwingHorizontal::MIDDLE, SwingVertical::SWING);
        break;
      default:
        ESP_LOGE("sharp_ac", "Unsupported swing mode: %d", (int) swing_mode);
    }
  }

  this->publish_update();
  ESP_LOGD("sharp_ac", "=== Control Processing Complete ===");
}

void SharpAc::set_ion(bool state) { this->core_->set_ion(state); }

void SharpAc::set_vane_horizontal(SwingHorizontal val) { this->core_->set_vane_horizontal(val); }

void SharpAc::set_vane_vertical(SwingVertical val) { this->core_->set_vane_vertical(val); }

void SharpAc::setup() {
  this->core_->setup();
  if (this->connection_status_sensor_ != nullptr) {
    this->connection_status_sensor_->publish_state("Disconnected");
  }
}

void SharpAc::loop() { this->core_->loop(); }

void SharpAc::update_connection_status(int status) {
  if (this->connection_status_sensor_ == nullptr) {
    return;
  }

  std::string status_text;
  if (status < 8) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Connecting (%d/8)", status);
    status_text = buffer;
  } else if (status == 8) {
    status_text = "Connected";
  } else {
    status_text = "Unknown";
  }

  this->connection_status_sensor_->publish_state(status_text);
}

void SharpAc::trigger_reconnect() {
  ESP_LOGI("sharp_ac", "Triggering connection reset...");
  if (this->core_ != nullptr) {
    this->core_->reset_connection();
  }
}

void ReconnectButton::press_action() {
  ESP_LOGI("sharp_ac", "Reconnect button pressed - resetting connection");
  if (this->parent_ != nullptr) {
    this->parent_->trigger_reconnect();
  }
}

}  // namespace esphome::sharp_ac
