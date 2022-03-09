#include "midea_ir.h"
#include "midea_data.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/coolix/coolix.h"

namespace esphome {
namespace midea_ir {

static const char *const TAG = "midea_ir.climate";

void ControlData::set_temp(float temp) {
  uint8_t min;
  if (this->get_fahrenheit()) {
    min = MIDEA_TEMPF_MIN;
    temp = esphome::clamp<float>(celsius_to_fahrenheit(temp), MIDEA_TEMPF_MIN, MIDEA_TEMPF_MAX);
  } else {
    min = MIDEA_TEMPC_MIN;
    temp = esphome::clamp<float>(temp, MIDEA_TEMPC_MIN, MIDEA_TEMPC_MAX);
  }
  this->set_value_(2, lroundf(temp) - min, 31);
}

float ControlData::get_temp() const {
  const uint8_t temp = this->get_value_(2, 31);
  if (this->get_fahrenheit())
    return fahrenheit_to_celsius(static_cast<float>(temp + MIDEA_TEMPF_MIN));
  return static_cast<float>(temp + MIDEA_TEMPC_MIN);
}

void ControlData::fix() {
  // In FAN_AUTO, modes COOL, HEAT and FAN_ONLY bit #5 in byte #1 must be set
  const uint8_t value = this->get_value_(1, 31);
  if (value == 0 || value == 3 || value == 4)
    this->set_mask_(1, true, 32);
  // In FAN_ONLY mode we need to set all temperature bits
  if (this->get_mode_() == MODE_FAN_ONLY)
    this->set_mask_(2, true, 31);
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
  // swing and preset resets after unit powered off
  if (call.get_mode() == climate::CLIMATE_MODE_OFF) {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->preset = climate::CLIMATE_PRESET_NONE;
  } else if (call.get_swing_mode().has_value() && ((*call.get_swing_mode() == climate::CLIMATE_SWING_OFF &&
                                                    this->swing_mode == climate::CLIMATE_SWING_VERTICAL) ||
                                                   (*call.get_swing_mode() == climate::CLIMATE_SWING_VERTICAL &&
                                                    this->swing_mode == climate::CLIMATE_SWING_OFF))) {
    this->swing_ = true;
  } else if (call.get_preset().has_value() &&
             ((*call.get_preset() == climate::CLIMATE_PRESET_NONE && this->preset == climate::CLIMATE_PRESET_BOOST) ||
              (*call.get_preset() == climate::CLIMATE_PRESET_BOOST && this->preset == climate::CLIMATE_PRESET_NONE))) {
    this->boost_ = true;
  }
  climate_ir::ClimateIR::control(call);
}

void MideaIR::transmit_(MideaData &data) {
  data.finalize();
  auto transmit = this->transmitter_->transmit();
  remote_base::MideaProtocol().encode(transmit.get_data(), data);
  transmit.perform();
}

void MideaIR::transmit_state() {
  if (this->swing_) {
    SpecialData data(SpecialData::VSWING_TOGGLE);
    this->transmit_(data);
    this->swing_ = false;
    return;
  }
  if (this->boost_) {
    SpecialData data(SpecialData::TURBO_TOGGLE);
    this->transmit_(data);
    this->boost_ = false;
    return;
  }
  ControlData data;
  data.set_fahrenheit(this->fahrenheit_);
  data.set_temp(this->target_temperature);
  data.set_mode(this->mode);
  data.set_fan_mode(this->fan_mode.value_or(ClimateFanMode::CLIMATE_FAN_AUTO));
  data.set_sleep_preset(this->preset == climate::CLIMATE_PRESET_SLEEP);
  data.fix();
  this->transmit_(data);
}

bool MideaIR::on_receive(remote_base::RemoteReceiveData data) {
  auto midea = remote_base::MideaProtocol().decode(data);
  if (midea.has_value())
    return this->on_midea_(*midea);
  return coolix::CoolixClimate::on_coolix(this, data);
}

bool MideaIR::on_midea_(const MideaData &data) {
  ESP_LOGV(TAG, "Decoded Midea IR data: %s", data.to_string().c_str());
  if (data.type() == MideaData::MIDEA_TYPE_CONTROL) {
    const ControlData status = data;
    if (status.get_mode() != climate::CLIMATE_MODE_FAN_ONLY)
      this->target_temperature = status.get_temp();
    this->mode = status.get_mode();
    this->fan_mode = status.get_fan_mode();
    if (status.get_sleep_preset()) {
      this->preset = climate::CLIMATE_PRESET_SLEEP;
    } else if (this->preset == climate::CLIMATE_PRESET_SLEEP) {
      this->preset = climate::CLIMATE_PRESET_NONE;
    }
    this->publish_state();
    return true;
  }
  if (data.type() == MideaData::MIDEA_TYPE_SPECIAL) {
    switch (data[1]) {
      case SpecialData::VSWING_TOGGLE:
        this->swing_mode = this->swing_mode == climate::CLIMATE_SWING_VERTICAL ? climate::CLIMATE_SWING_OFF
                                                                               : climate::CLIMATE_SWING_VERTICAL;
        break;
      case SpecialData::TURBO_TOGGLE:
        this->preset = this->preset == climate::CLIMATE_PRESET_BOOST ? climate::CLIMATE_PRESET_NONE
                                                                     : climate::CLIMATE_PRESET_BOOST;
        break;
    }
    this->publish_state();
    return true;
  }

  return false;
}

}  // namespace midea_ir
}  // namespace esphome
