#include "electra_rc3_ir.h"
#include "esphome/components/remote_base/electra_rc3_protocol.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace electra_rc3_ir {

// using climate::ClimateMode;
// using climate::ClimateFanMode;
using remote_base::ElectraRC3Data;

static const char *const TAG = "electra_rc3_ir.climate";

using ElectraRC3Mode = enum ElectraRC3Mode {
  ELECTRA_RC3_MODE_COOL = 0b001,
  ELECTRA_RC3_MODE_HEAT = 0b010,
  ELECTRA_RC3_MODE_AUTO = 0b011,
  ELECTRA_RC3_MODE_DRY = 0b100,
  ELECTRA_RC3_MODE_FAN = 0b101,
  ELECTRA_RC3_MODE_OFF = 0b111
};

using ElectraRC3Fan = enum ElectraRC3Fan {
  ELECTRA_RC3_FAN_LOW = 0b00,
  ELECTRA_RC3_FAN_MEDIUM = 0b01,
  ELECTRA_RC3_FAN_HIGH = 0b10,
  ELECTRA_RC3_FAN_AUTO = 0b11
};

// void ElectraRC3IR::control(const climate::ClimateCall &call) {
//   // swing and preset resets after unit powered off
//   if (call.get_mode().has_value() && (climate::CLIMATE_MODE_OFF == call.get_mode())) {
//     this->current_mode = call.get_mode();
//   }

//   climate_ir::ClimateIR::control(call);
// }

void ElectraRC3IR::transmit_state() {
  ElectraRC3Data data;

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      data.fan = ElectraRC3Fan::ELECTRA_RC3_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      data.fan = ElectraRC3Fan::ELECTRA_RC3_FAN_MEDIUM;
      break;
    case climate::CLIMATE_FAN_HIGH:
      data.fan = ElectraRC3Fan::ELECTRA_RC3_FAN_HIGH;
      break;
    default:
      data.fan = ElectraRC3Fan::ELECTRA_RC3_FAN_AUTO;
      break;
  }

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      data.mode = ElectraRC3Mode::ELECTRA_RC3_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      data.mode = ElectraRC3Mode::ELECTRA_RC3_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_AUTO:
      data.mode = ElectraRC3Mode::ELECTRA_RC3_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_DRY:
      data.mode = ElectraRC3Mode::ELECTRA_RC3_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      data.mode = ElectraRC3Mode::ELECTRA_RC3_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      data.mode = ElectraRC3Mode::ELECTRA_RC3_MODE_OFF;
      break;
  }

  if (climate::CLIMATE_MODE_OFF != this->mode) {
    data.power = (climate::CLIMATE_MODE_OFF == this->current_mode_) ? 1 : 0;
  }
  this->current_mode_ = this->mode;

  auto temp = (uint8_t) roundf(this->target_temperature);

  if (temp < ELECTRA_RC3_TEMP_MIN) {
    temp = ELECTRA_RC3_TEMP_MIN;
  } else if (temp > ELECTRA_RC3_TEMP_MAX) {
    temp = ELECTRA_RC3_TEMP_MAX;
  }
  data.temperature = temp - 15;

  data.sleep = (this->preset == climate::CLIMATE_PRESET_SLEEP) ? 1 : 0;
  ;
  data.swing = (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) ? 1 : 0;

  ESP_LOGD(TAG,
           "Electra RC3: power = 0x%llX, mode = 0x%llX, fan = 0x%llX, swing = 0x%llX, "
           "ifeel = 0x%llX, temperature = 0x%llX, sleep = 0x%llX",
           data.power, data.mode, data.fan, data.swing, data.ifeel, data.temperature, data.sleep);

  auto transmit = this->transmitter_->transmit();
  remote_base::ElectraRC3Protocol().encode(transmit.get_data(), data);
  transmit.perform();
}

bool ElectraRC3IR::on_receive(remote_base::RemoteReceiveData data) { return false; }

}  // namespace electra_rc3_ir
}  // namespace esphome
