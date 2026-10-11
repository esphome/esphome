#include "climate_ir_siemens_ira211.h"
#include "esphome/core/helpers.h"

namespace esphome::climate_ir_siemens_ira211 {

using namespace remote_base;

struct PresetMapping {
  climate::ClimatePreset preset;
  IRA211Mode mode;
};

static constexpr PresetMapping PRESETS[] = {
    {climate::CLIMATE_PRESET_COMFORT, IRA211Mode::IRA211_MODE_COMFORT},
    {climate::CLIMATE_PRESET_ECO, IRA211Mode::IRA211_MODE_TIMER},
};

struct FanMapping {
  climate::ClimateFanMode fan_mode;
  IRA211Fan fan;
};

static constexpr FanMapping FANS[] = {
    {climate::CLIMATE_FAN_AUTO, IRA211Fan::IRA211_FAN_AUTO},
    {climate::CLIMATE_FAN_LOW, IRA211Fan::IRA211_FAN_LOW},
    {climate::CLIMATE_FAN_MEDIUM, IRA211Fan::IRA211_FAN_MEDIUM},
    {climate::CLIMATE_FAN_HIGH, IRA211Fan::IRA211_FAN_HIGH},
};

// The first entry of each table is the fallback for values the other side does not know
static IRA211Mode to_ira211_mode(climate::ClimatePreset preset) {
  for (const auto &entry : PRESETS) {
    if (entry.preset == preset)
      return entry.mode;
  }
  return PRESETS[0].mode;
}

static climate::ClimatePreset to_climate_preset(IRA211Mode mode) {
  for (const auto &entry : PRESETS) {
    if (entry.mode == mode)
      return entry.preset;
  }
  return PRESETS[0].preset;
}

static IRA211Fan to_ira211_fan(climate::ClimateFanMode fan_mode) {
  for (const auto &entry : FANS) {
    if (entry.fan_mode == fan_mode)
      return entry.fan;
  }
  return FANS[0].fan;
}

static climate::ClimateFanMode to_climate_fan_mode(IRA211Fan fan) {
  for (const auto &entry : FANS) {
    if (entry.fan == fan)
      return entry.fan_mode;
  }
  return FANS[0].fan_mode;
}

void SiemensIRA211Climate::setup() {
  // The thermostat decides between heating and cooling itself, so one mode stands for both
  if (this->modes_.count(climate::CLIMATE_MODE_HEAT_COOL)) {
    this->modes_.erase(climate::CLIMATE_MODE_HEAT);
    this->modes_.erase(climate::CLIMATE_MODE_COOL);
  }
  climate_ir::ClimateIR::setup();
}

climate::ClimateMode SiemensIRA211Climate::active_mode_() const {
  if (this->modes_.count(climate::CLIMATE_MODE_HEAT_COOL))
    return climate::CLIMATE_MODE_HEAT_COOL;
  if (this->modes_.count(climate::CLIMATE_MODE_HEAT))
    return climate::CLIMATE_MODE_HEAT;
  return climate::CLIMATE_MODE_COOL;
}

void SiemensIRA211Climate::transmit_frame_(IRA211Command command, IRA211Mode mode, IRA211Fan fan) {
  IRA211Data data{};
  data.command = command;
  data.set_temperature(clamp(this->target_temperature, this->minimum_temperature_, this->maximum_temperature_));
  data.mode = mode;
  data.fan = fan;

  auto transmit = this->transmitter_->transmit();
  IRA211Protocol().encode(transmit.get_data(), data);
  transmit.perform();
}

void SiemensIRA211Climate::transmit_state() {
  const IRA211Mode mode = to_ira211_mode(this->preset.value_or(climate::CLIMATE_PRESET_COMFORT));
  const IRA211Fan fan = to_ira211_fan(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_frame_(IRA211Command::IRA211_COMMAND_POWER, IRA211Mode::IRA211_MODE_PROTECTION, fan);
    this->is_on_ = false;
    return;
  }
  if (!this->is_on_) {
    // Turning on takes a POWER frame before the state is sent
    this->transmit_frame_(IRA211Command::IRA211_COMMAND_POWER, mode, fan);
    this->is_on_ = true;
  }
  this->transmit_frame_(IRA211Command::IRA211_COMMAND_SYNC, mode, fan);
}

void SiemensIRA211Climate::apply_mode_(IRA211Mode mode) {
  // The thermostat reports "off" as the PROTECTION mode
  if (mode == IRA211Mode::IRA211_MODE_PROTECTION) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->is_on_ = false;
    return;
  }
  if (!this->is_on_) {
    this->mode = this->active_mode_();
    this->is_on_ = true;
  }
  this->preset = to_climate_preset(mode);
}

bool SiemensIRA211Climate::on_receive(RemoteReceiveData data) {
  auto decoded = IRA211Protocol().decode(data);
  if (!decoded.has_value())
    return false;

  const IRA211Data &frame = *decoded;
  switch (frame.command) {
    case IRA211Command::IRA211_COMMAND_SYNC:
    case IRA211Command::IRA211_COMMAND_POWER:
      this->apply_mode_(frame.mode);
      this->target_temperature = frame.get_temperature();
      this->fan_mode = to_climate_fan_mode(frame.fan);
      break;
    case IRA211Command::IRA211_COMMAND_TEMP_UP:
    case IRA211Command::IRA211_COMMAND_TEMP_DOWN:
      this->target_temperature = frame.get_temperature();
      break;
    case IRA211Command::IRA211_COMMAND_MODE:
      this->apply_mode_(frame.mode);
      break;
    case IRA211Command::IRA211_COMMAND_FAN:
      this->fan_mode = to_climate_fan_mode(frame.fan);
      break;
    default:
      return false;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::climate_ir_siemens_ira211
