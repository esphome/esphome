#include "comp_hardware.h"
#include "comp_vane_horizontal.h"
#include "comp_vane_vertical.h"
#include "comp_reconnect_button.h"

namespace esphome {
namespace sharp_ac {
void ESPHomeStateCallback::on_state_update() {
  if (sharp_ac_) {
    sharp_ac_->publishUpdate();
  }
}

void ESPHomeStateCallback::on_ion_state_update(bool state) {}

void ESPHomeStateCallback::on_vane_horizontal_update(SwingHorizontal val) {}

void ESPHomeStateCallback::on_vane_vertical_update(SwingVertical val) {}

void ESPHomeStateCallback::on_connection_status_update(int status) {
  if (sharp_ac_) {
    sharp_ac_->updateConnectionStatus(status);
  }
}

SharpAc::SharpAc() {
  hardware_interface_ = std::make_unique<ESPHomeHardwareInterface>(this);
  state_callback_ = std::make_unique<ESPHomeStateCallback>(this);
  core_ = std::make_unique<SharpAcCore>(hardware_interface_.get(), state_callback_.get());
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

  ESP_LOGCONFIG(TAG, "  Connection status sensor: %s", this->connectionStatusSensor != nullptr ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Ion switch: %s", this->ionSwitch != nullptr ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Horizontal vane select: %s", this->vaneHorizontal != nullptr ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Vertical vane select: %s", this->vaneVertical != nullptr ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Reconnect button: %s", this->reconnectButton != nullptr ? "yes" : "no");
}

void SharpAc::publishUpdate() {
  const auto &state = core_->getState();

  this->target_temperature = state.temperature;
  this->current_temperature = core_->getCurrentTemperature();

  switch (state.fan) {
    case FanMode::auto_fan:
      this->fan_mode = ClimateFanMode::CLIMATE_FAN_AUTO;
      break;
    case FanMode::low:
      this->fan_mode = ClimateFanMode::CLIMATE_FAN_LOW;
      break;
    case FanMode::mid:
      this->fan_mode = ClimateFanMode::CLIMATE_FAN_MEDIUM;
      break;
    case FanMode::high:
      this->fan_mode = ClimateFanMode::CLIMATE_FAN_MEDIUM;
      break;
    case FanMode::highest:
      this->fan_mode = ClimateFanMode::CLIMATE_FAN_HIGH;
      break;
    default:
      ESP_LOGD("sharp_ac", "UNKNOWN FAN MODE");
  }

  switch (state.mode) {
    case PowerMode::fan:
      this->mode = ClimateMode::CLIMATE_MODE_FAN_ONLY;
      break;
    case PowerMode::cool:
      this->mode = ClimateMode::CLIMATE_MODE_COOL;
      break;
    case PowerMode::heat:
      this->mode = ClimateMode::CLIMATE_MODE_HEAT;
      break;
    case PowerMode::dry:
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
      case PowerMode::fan:
        this->action = climate::ClimateAction::CLIMATE_ACTION_FAN;
        break;
      case PowerMode::cool:
        this->action = climate::ClimateAction::CLIMATE_ACTION_COOLING;
        break;
      case PowerMode::heat:
        this->action = climate::ClimateAction::CLIMATE_ACTION_HEATING;
        break;
      case PowerMode::dry:
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

  if (state.swingH == SwingHorizontal::swing && state.swingV == SwingVertical::swing)
    this->swing_mode = ClimateSwingMode::CLIMATE_SWING_BOTH;
  else if (state.swingH == SwingHorizontal::swing)
    this->swing_mode = ClimateSwingMode::CLIMATE_SWING_HORIZONTAL;
  else if (state.swingV == SwingVertical::swing)
    this->swing_mode = ClimateSwingMode::CLIMATE_SWING_VERTICAL;
  else
    this->swing_mode = ClimateSwingMode::CLIMATE_SWING_OFF;

  if (this->ionSwitch != nullptr)
    this->ionSwitch->publish_state(state.ion);

  if (this->vaneHorizontal != nullptr)
    this->vaneHorizontal->setVal(state.swingH);

  if (this->vaneVertical != nullptr)
    this->vaneVertical->setVal(state.swingV);

  this->publish_state();
}

void SharpAc::control(const ClimateCall &call) {
  ESP_LOGD("sharp_ac", "=== Climate Control Called ===");

  if (call.get_mode().has_value()) {
    ClimateMode newMode = call.get_mode().value();
    ESP_LOGD("sharp_ac", "Setting mode: %d (%s)", (int) newMode,
             newMode == ClimateMode::CLIMATE_MODE_OFF        ? "OFF"
             : newMode == ClimateMode::CLIMATE_MODE_COOL     ? "COOL"
             : newMode == ClimateMode::CLIMATE_MODE_HEAT     ? "HEAT"
             : newMode == ClimateMode::CLIMATE_MODE_DRY      ? "DRY"
             : newMode == ClimateMode::CLIMATE_MODE_FAN_ONLY ? "FAN"
                                                             : "UNKNOWN");

    switch (newMode) {
      case ClimateMode::CLIMATE_MODE_OFF:
        ESP_LOGD("sharp_ac", "Sending OFF command to core");
        core_->controlMode(PowerMode::cool, false);
        break;
      case ClimateMode::CLIMATE_MODE_COOL:
        ESP_LOGD("sharp_ac", "Sending COOL ON command to core");
        core_->controlMode(PowerMode::cool, true);
        break;
      case ClimateMode::CLIMATE_MODE_HEAT:
        ESP_LOGD("sharp_ac", "Sending HEAT ON command to core");
        core_->controlMode(PowerMode::heat, true);
        break;
      case ClimateMode::CLIMATE_MODE_DRY:
        ESP_LOGD("sharp_ac", "Sending DRY ON command to core");
        core_->controlMode(PowerMode::dry, true);
        break;
      case ClimateMode::CLIMATE_MODE_FAN_ONLY:
        ESP_LOGD("sharp_ac", "Sending FAN ONLY ON command to core");
        core_->controlMode(PowerMode::fan, true);
        break;
      default:
        ESP_LOGE("sharp_ac", "Unsupported mode: %d", (int) newMode);
    }
  }

  if (call.get_target_temperature().has_value()) {
    float temp = call.get_target_temperature().value();
    ESP_LOGD("sharp_ac", "Setting target temperature: %.1f C", temp);
    ESP_LOGD("sharp_ac", "Sending temperature command to core");
    core_->controlTemperature((int) temp);
  }

  if (call.get_fan_mode().has_value()) {
    ClimateFanMode fanMode = call.get_fan_mode().value();
    ESP_LOGD("sharp_ac", "Setting fan mode: %d (%s)", (int) fanMode,
             fanMode == ClimateFanMode::CLIMATE_FAN_AUTO     ? "AUTO"
             : fanMode == ClimateFanMode::CLIMATE_FAN_LOW    ? "LOW"
             : fanMode == ClimateFanMode::CLIMATE_FAN_MEDIUM ? "MEDIUM"
             : fanMode == ClimateFanMode::CLIMATE_FAN_HIGH   ? "HIGH"
                                                             : "UNKNOWN");

    switch (fanMode) {
      case ClimateFanMode::CLIMATE_FAN_AUTO:
        ESP_LOGD("sharp_ac", "Sending AUTO FAN command to core");
        core_->controlFan(FanMode::auto_fan);
        break;
      case ClimateFanMode::CLIMATE_FAN_LOW:
        ESP_LOGD("sharp_ac", "Sending LOW FAN command to core");
        core_->controlFan(FanMode::low);
        break;
      case ClimateFanMode::CLIMATE_FAN_MEDIUM:
        ESP_LOGD("sharp_ac", "Sending MEDIUM FAN command to core");
        core_->controlFan(FanMode::mid);
        break;
      case ClimateFanMode::CLIMATE_FAN_HIGH:
        ESP_LOGD("sharp_ac", "Sending HIGH FAN command to core");
        core_->controlFan(FanMode::highest);
        break;
      default:
        ESP_LOGE("sharp_ac", "Unsupported fan mode: %d", (int) fanMode);
    }
  }

  if (call.get_preset().has_value()) {
    ClimatePreset preset = call.get_preset().value();
    ESP_LOGD("sharp_ac", "Setting preset: %d (%s)", (int) preset,
             preset == ClimatePreset::CLIMATE_PRESET_ECO     ? "ECO"
             : preset == ClimatePreset::CLIMATE_PRESET_BOOST ? "BOOST"
             : preset == ClimatePreset::CLIMATE_PRESET_NONE  ? "NONE"
                                                             : "UNKNOWN");

    switch (preset) {
      case ClimatePreset::CLIMATE_PRESET_ECO:
        ESP_LOGD("sharp_ac", "Sending ECO preset command to core");
        core_->controlPreset(Preset::ECO);
        break;
      case ClimatePreset::CLIMATE_PRESET_BOOST:
        ESP_LOGD("sharp_ac", "Sending BOOST preset command to core");
        core_->controlPreset(Preset::FULLPOWER);
        break;
      default:
        ESP_LOGD("sharp_ac", "Sending NONE preset command to core");
        core_->controlPreset(Preset::NONE);
        break;
    }
  }

  if (call.get_swing_mode().has_value()) {
    ClimateSwingMode swingMode = call.get_swing_mode().value();
    ESP_LOGD("sharp_ac", "Setting swing mode: %d (%s)", (int) swingMode,
             swingMode == ClimateSwingMode::CLIMATE_SWING_OFF          ? "OFF"
             : swingMode == ClimateSwingMode::CLIMATE_SWING_BOTH       ? "BOTH"
             : swingMode == ClimateSwingMode::CLIMATE_SWING_HORIZONTAL ? "HORIZONTAL"
             : swingMode == ClimateSwingMode::CLIMATE_SWING_VERTICAL   ? "VERTICAL"
                                                                       : "UNKNOWN");

    switch (swingMode) {
      case ClimateSwingMode::CLIMATE_SWING_OFF:
        ESP_LOGD("sharp_ac", "Sending SWING OFF command to core (middle/mid position)");
        core_->controlSwing(SwingHorizontal::middle, SwingVertical::mid);
        break;
      case ClimateSwingMode::CLIMATE_SWING_BOTH:
        ESP_LOGD("sharp_ac", "Sending BOTH SWING command to core");
        core_->controlSwing(SwingHorizontal::swing, SwingVertical::swing);
        break;
      case ClimateSwingMode::CLIMATE_SWING_HORIZONTAL:
        ESP_LOGD("sharp_ac", "Sending HORIZONTAL SWING command to core");
        core_->controlSwing(SwingHorizontal::swing, SwingVertical::mid);
        break;
      case ClimateSwingMode::CLIMATE_SWING_VERTICAL:
        ESP_LOGD("sharp_ac", "Sending VERTICAL SWING command to core");
        core_->controlSwing(SwingHorizontal::middle, SwingVertical::swing);
        break;
      default:
        ESP_LOGE("sharp_ac", "Unsupported swing mode: %d", (int) swingMode);
    }
  }

  // Publish optimistic state immediately after sending command
  // This prevents the UI from showing the old state briefly
  ESP_LOGD("sharp_ac", "Publishing optimistic state update");
  this->publishUpdate();

  ESP_LOGD("sharp_ac", "=== Control Processing Complete ===");
}

void SharpAc::setIon(bool state) { core_->setIon(state); }

void SharpAc::setVaneHorizontal(SwingHorizontal val) { core_->setVaneHorizontal(val); }

void SharpAc::setVaneVertical(SwingVertical val) { core_->setVaneVertical(val); }

void SharpAc::setup() {
  core_->setup();
  if (connectionStatusSensor != nullptr) {
    connectionStatusSensor->publish_state("Disconnected");
  }
}

void SharpAc::loop() { core_->loop(); }

void SharpAc::updateConnectionStatus(int status) {
  if (connectionStatusSensor == nullptr) {
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

  connectionStatusSensor->publish_state(status_text);
}

void SharpAc::triggerReconnect() {
  ESP_LOGI("sharp_ac", "Triggering connection reset...");
  if (core_) {
    core_->resetConnection();
  }
}

void ReconnectButton::press_action() {
  ESP_LOGI("sharp_ac", "Reconnect button pressed - resetting connection");
  if (this->parent_ != nullptr) {
    this->parent_->triggerReconnect();
  }
}

}  // namespace sharp_ac
}  // namespace esphome
