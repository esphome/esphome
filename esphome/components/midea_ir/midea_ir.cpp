#include "midea_ir.h"
#include "midea_data.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace midea_ir {

static const char *const TAG = "midea_ir.climate";

void ControlData::set_temp(float temp) {
  this->set_value_(2, this->get_fahrenheit() ? (lroundf(celsius_to_fahrenheit(temp)) - 62) : (lroundf(temp) - 17), 31);
}

float ControlData::get_temp() const {
  const uint8_t temp = this->get_value_(2, 31);
  if (this->get_fahrenheit())
    return fahrenheit_to_celsius(static_cast<float>(temp + 62));
  return static_cast<float>(temp + 17);
}

void ControlData::set_mode(ClimateMode mode) {
  switch (mode) {
  case ClimateMode::CLIMATE_MODE_OFF:
    this->set_power_(false);
    return;
  case ClimateMode::CLIMATE_MODE_COOL:
    this->set_mode_(MODE_COOL);
    break;
  case ClimateMode::CLIMATE_MODE_DRY:
    this->set_mode_(MODE_DRY);
    break;
  case ClimateMode::CLIMATE_MODE_FAN_ONLY:
    this->set_mode_(MODE_FAN_ONLY);
    break;
  case ClimateMode::CLIMATE_MODE_HEAT:
    this->set_mode_(MODE_HEAT);
    break;
  default:
    this->set_mode_(MODE_AUTO);
    break;
  }
  this->set_power_(true);
}

ClimateMode ControlData::get_mode() const {
  if (!this->get_power_())
    return ClimateMode::CLIMATE_MODE_OFF;
  switch (this->get_mode_()) {
  case MODE_COOL:
    return ClimateMode::CLIMATE_MODE_COOL;
  case MODE_DRY:
    return ClimateMode::CLIMATE_MODE_DRY;
  case MODE_FAN_ONLY:
    return ClimateMode::CLIMATE_MODE_FAN_ONLY;
  case MODE_HEAT:
    return ClimateMode::CLIMATE_MODE_HEAT;
  default:
    return ClimateMode::CLIMATE_MODE_HEAT_COOL;
  }
}

void ControlData::set_fan_mode(ClimateFanMode mode) {
  switch (mode) {
  case ClimateFanMode::CLIMATE_FAN_LOW:
    this->set_fan_mode_(FAN_LOW);
    break;
  case ClimateFanMode::CLIMATE_FAN_MEDIUM:
    this->set_fan_mode_(FAN_MEDIUM);
    break;
  case ClimateFanMode::CLIMATE_FAN_HIGH:
    this->set_fan_mode_(FAN_HIGH);
    break;
  default:
    this->set_fan_mode_(FAN_AUTO);
    break;
  }
}

ClimateFanMode ControlData::get_fan_mode() const {
  switch (this->get_fan_mode_()) {
  case FAN_LOW:
    return ClimateFanMode::CLIMATE_FAN_LOW;
  case FAN_MEDIUM:
    return ClimateFanMode::CLIMATE_FAN_MEDIUM;
  case FAN_HIGH:
    return ClimateFanMode::CLIMATE_FAN_HIGH;
  default:
    return ClimateFanMode::CLIMATE_FAN_AUTO;
  }
}

void MideaIR::control(const climate::ClimateCall &call) {
  // swing resets after unit powered off
  if (call.get_mode() == climate::CLIMATE_MODE_OFF) {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->preset = climate::CLIMATE_PRESET_NONE;
  }
  climate_ir::ClimateIR::control(call);
}

void MideaIR::transmit_(remote_base::MideaData &data) {
  data.finalize();
  auto transmit = this->transmitter_->transmit();
  remote_base::MideaProtocol().encode(transmit.get_data(), data);
  transmit.perform();
}

void MideaIR::transmit_state() {
  ControlData data;
  data.set_fahrenheit(this->fahrenheit_);
  data.set_temp(this->target_temperature);
  data.set_mode(this->mode);
  data.set_fan_mode(this->fan_mode.value_or(ClimateFanMode::CLIMATE_FAN_AUTO));
  data.set_sleep_preset(this->preset == climate::CLIMATE_PRESET_SLEEP);
  this->transmit_(data);
}

template<typename T> void update_property(T &property, const T &value, bool &flag) {
  if (property != value) {
    property = value;
    flag = true;
  }
}

bool MideaIR::on_receive(remote_base::RemoteReceiveData data) {
#if 0
  auto opt = remote_base::MideaProtocol().decode(data);

  if (!opt.has_value())
    return false;

  const remote_base::MideaData &code = opt.value();

  if (code.type() != remote_base::MideaData::MIDEA_TYPE_COMMAND)
    return false;

  ControlData status(code);

  remote_base::MideaProtocol().dump(code);

  //ESP_LOGV(TAG, "Decoded Midea IR data: %s", code.to_string().c_str());

  bool need_publish = false;
  update_property(this->target_temperature, status.get_temp(), need_publish);
  update_property(this->mode, status.get_mode(), need_publish);
  if (!this->fan_mode.has_value() || this->fan_mode.value() != status.get_fan_mode()) {
    this->fan_mode = status.get_fan_mode();
    need_publish = true;
  }

  if (need_publish)
    this->publish_state();
  return true;
#endif
  return false;
}

}  // namespace midea_ir
}  // namespace esphome
