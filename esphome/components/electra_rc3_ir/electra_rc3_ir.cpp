#include "electra_rc3_ir.h"
#include "esphome/components/remote_base/electra_rc3_protocol.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace electra_rc3_ir {

using climate::ClimateMode;
using climate::ClimateFanMode;
using remote_base::ElectraRC3Data;

static const char *const TAG = "electra_rc3_ir.climate";

typedef enum ElectraRC3Mode {
  ElectraRC3ModeCool = 0b001,
  ElectraRC3ModeHeat = 0b010,
  ElectraRC3ModeAuto = 0b011,
  ElectraRC3ModeDry = 0b100,
  ElectraRC3ModeFan = 0b101,
  ElectraRC3ModeOff = 0b111
} ElectraRC3Mode;

typedef enum ElectraRC3Fan {
  ElectraRC3FanLow = 0b00,
  ElectraRC3FanMedium = 0b01,
  ElectraRC3FanHigh = 0b10,
  ElectraRC3FanAuto = 0b11
} ElectraRC3Fan;

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
      data.fan = ElectraRC3Fan::ElectraRC3FanLow;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      data.fan = ElectraRC3Fan::ElectraRC3FanMedium;
      break;
    case climate::CLIMATE_FAN_HIGH:
      data.fan = ElectraRC3Fan::ElectraRC3FanHigh;
      break;
    default:
      data.fan = ElectraRC3Fan::ElectraRC3FanAuto;
      break;
  }

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      data.mode = ElectraRC3Mode::ElectraRC3ModeCool;
      break;
    case climate::CLIMATE_MODE_HEAT:
      data.mode = ElectraRC3Mode::ElectraRC3ModeHeat;
      break;
    case climate::CLIMATE_MODE_AUTO:
      data.mode = ElectraRC3Mode::ElectraRC3ModeAuto;
      break;
    case climate::CLIMATE_MODE_DRY:
      data.mode = ElectraRC3Mode::ElectraRC3ModeDry;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      data.mode = ElectraRC3Mode::ElectraRC3ModeFan;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      data.mode = ElectraRC3Mode::ElectraRC3ModeOff;
      break;
  }

  if (climate::CLIMATE_MODE_OFF != this->mode) {
    data.power = (climate::CLIMATE_MODE_OFF == this->current_mode) ? 1 : 0;
  }
  this->current_mode = this->mode;

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
           "Electra RC3: power = 0x%X, mode = 0x%X, fan = 0x%X, swing = 0x%X, ifeel = 0x%X, temperature = 0x%X, sleep "
           "= 0x%X",
           data.power, data.mode, data.fan, data.swing, data.ifeel, data.temperature, data.sleep);

  auto transmit = this->transmitter_->transmit();
  remote_base::ElectraRC3Protocol().encode(transmit.get_data(), data);
  transmit.perform();
}

bool ElectraRC3IR::on_receive(remote_base::RemoteReceiveData data) { return false; }

}  // namespace electra_rc3_ir
}  // namespace esphome
