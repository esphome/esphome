#include "siemens_ira211.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::siemens_ira211 {

using namespace remote_base;

// Presets map to IRA211Mode and fan modes to IRA211Fan. The first entry of each table is the fallback for values
// the other side does not know.
static constexpr climate_ir::IRCode PRESETS[] PROGMEM = {
    {climate::CLIMATE_PRESET_COMFORT, static_cast<uint8_t>(IRA211Mode::IRA211_MODE_COMFORT)},
    {climate::CLIMATE_PRESET_ECO, static_cast<uint8_t>(IRA211Mode::IRA211_MODE_TIMER)},
};
static constexpr climate_ir::IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, static_cast<uint8_t>(IRA211Fan::IRA211_FAN_AUTO)},
    {climate::CLIMATE_FAN_LOW, static_cast<uint8_t>(IRA211Fan::IRA211_FAN_LOW)},
    {climate::CLIMATE_FAN_MEDIUM, static_cast<uint8_t>(IRA211Fan::IRA211_FAN_MEDIUM)},
    {climate::CLIMATE_FAN_HIGH, static_cast<uint8_t>(IRA211Fan::IRA211_FAN_HIGH)},
};

static IRA211Mode to_ira211_mode(climate::ClimatePreset preset) {
  return static_cast<IRA211Mode>(climate_ir::to_ir_code(PRESETS, preset));
}
static climate::ClimatePreset to_climate_preset(IRA211Mode mode) {
  return climate_ir::from_ir_code<climate::ClimatePreset>(PRESETS, static_cast<uint8_t>(mode));
}
static IRA211Fan to_ira211_fan(climate::ClimateFanMode fan_mode) {
  return static_cast<IRA211Fan>(climate_ir::to_ir_code(FANS, fan_mode));
}
static climate::ClimateFanMode to_climate_fan_mode(IRA211Fan fan) {
  return climate_ir::from_ir_code<climate::ClimateFanMode>(FANS, static_cast<uint8_t>(fan));
}

void SiemensIRA211Climate::setup() {
  // The thermostat decides between heating and cooling itself, so one mode stands for both
  if (this->modes_.count(climate::CLIMATE_MODE_HEAT_COOL)) {
    this->modes_.erase(climate::CLIMATE_MODE_HEAT);
    this->modes_.erase(climate::CLIMATE_MODE_COOL);
  }
  climate_ir::ClimateIR::setup();
  // Match the restored state, so the first command after a reboot does not send a needless POWER frame
  this->is_on_ = this->mode != climate::CLIMATE_MODE_OFF;
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
  data.set_temperature(this->target_temperature);
  data.mode = mode;
  data.fan = fan;
  this->transmit_<IRA211Protocol>(data);
}

void SiemensIRA211Climate::transmit_state() {
  const IRA211Mode mode = to_ira211_mode(this->preset.value_or(climate::CLIMATE_PRESET_COMFORT));
  const IRA211Fan fan = to_ira211_fan(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));
  // Show the preset that is actually sent, so a fresh entity does not stay on "none"
  this->preset = to_climate_preset(mode);

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

}  // namespace esphome::siemens_ira211
