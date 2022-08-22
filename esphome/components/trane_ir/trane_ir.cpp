#include "trane_ir.h"
#include "esphome/components/remote_base/trane_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace trane_ir {

static const char *const TAG = "trane.climate";

static const uint32_t TRANE_CONSTANT_1 = 0x4A0ULL;

static const uint32_t TRANE_OFF = 0xB27BE0;
static const uint32_t TRANE_SWING = 0xB26BE0;
static const uint32_t TRANE_LED = 0xB5F5A5;
static const uint32_t TRANE_SILENCE_FP = 0xB5F5B6;

static const uint8_t TRANE_MODE_NBITS = 3;
enum TRANE_MODE{
  AUTO,
  COOL,
  DRY,
  FAN,
  HEAT,
};
static const uint8_t TRANE_MODE_AUTO = 0b000;
static const uint8_t TRANE_MODE_COOL = 0b001;
static const uint8_t TRANE_MODE_DRY = 0b010;
static const uint8_t TRANE_MODE_FAN = 0b011;
static const uint8_t TRANE_MODE_HEAT = 0b100;

static const uint8_t TRANE_FAN_NBITS = 3;
static const uint8_t TRANE_FAN_AUTO = 0b00;
static const uint8_t TRANE_FAN_LOW = 0b01;
static const uint8_t TRANE_FAN_MED = 0b10;
static const uint8_t TRANE_FAN_HIGH = 0b11;

// Temperature
static const uint8_t TRANE_TEMP_NBITS = 4;
static const uint8_t TRANE_TEMP_RANGE = TRANE_TEMP_MAX - TRANE_TEMP_MIN + 1;


void TraneClimate::transmit_state() {
  enum TRANE_MODE mode_state = AUTO;
  switch(this->mode){
    case climate::CLIMATE_MODE_COOL:
      mode_state = COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      mode_state = HEAT;
      break;
    case climate::CLIMATE_MODE_DRY:
      mode_state = DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      mode_state = FAN;
      break;
  }

  uint8_t temperature_state = 0;

  if (this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_HEAT){
    auto temp = roundf(clamp<float>(this->target_temperature, TRANE_TEMP_MIN, TRANE_TEMP_MAX));
    temperature_state = (temp-16);
  }

  uint8_t fan_speed_state = 0;

  switch(this->fan_mode.value()){
    case climate::CLIMATE_FAN_AUTO:
      fan_speed_state = TRANE_FAN_AUTO;
      break;
    case climate::CLIMATE_FAN_LOW:
      fan_speed_state = TRANE_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed_state = TRANE_FAN_MED;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed_state = TRANE_FAN_HIGH;
      break;
  }
  // shift 12-bit constant at beginning of first word by 23 bits to achieve a 35 bit word
  //  uint64_t remote_state_1 = 0x250600E49;
  //  remote_state_1 |= (temperature_state << (12 - TRANE_TEMP_NBITS));
  //  //Implicit 0 for sleep
  //  remote_state_1 |= (1 << (6));
  //  remote_state_1 |= (fan_speed_state << (7 - TRANE_FAN_NBITS));
  //  //power boolean
  //  remote_state_1 |= (1 << 3);
  //  remote_state_1 |= (mode_state);

  //  uint32_t remote_state_2 = 0xD002000A;

  // Testing fixed states
  uint64_t remote_state_1 = 0x6D0E00E49;
  uint32_t remote_state_2 = 0xD002000A;
  ESP_LOGD(TAG, "Sending Trane: data1 = 0x%016X", remote_state_1);
  ESP_LOGD(TAG, "Sending Trane: data2 = 0x%08X", remote_state_2);
  //TODO:IMPLEMENT STATE 2 ENCODING


  remote_base::TraneData remote_state;
  remote_state.trane_data_1 = remote_state_1;
  remote_state.trane_data_2 = remote_state_2;
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  remote_base::TraneProtocol().encode(data, remote_state);
  transmit.perform();
}

//bool TraneClimate::on_TRANE(climate::Climate *parent, remote_base::RemoteReceiveData data) {
//  auto decoded = remote_base::TRANEProtocol().decode(data);
//  if (!decoded.has_value())
//    return false;
//  // Decoded remote state y 3 bytes long code.
//  uint32_t remote_state = *decoded;
//  ESP_LOGV(TAG, "Decoded 0x%06X", remote_state);
//  if ((remote_state & 0xFF0000) != 0xB20000)
//    return false;
//
//  if (remote_state == TRANE_OFF) {
//    parent->mode = climate::CLIMATE_MODE_OFF;
//  } else if (remote_state == TRANE_SWING) {
//    parent->swing_mode =
//        parent->swing_mode == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
//  } else {
//    if ((remote_state & TRANE_MODE_MASK) == TRANE_HEAT) {
//      parent->mode = climate::CLIMATE_MODE_HEAT;
//    } else if ((remote_state & TRANE_MODE_MASK) == TRANE_AUTO) {
//      parent->mode = climate::CLIMATE_MODE_HEAT_COOL;
//    } else if ((remote_state & TRANE_MODE_MASK) == TRANE_DRY_FAN) {
//      if ((remote_state & TRANE_FAN_MASK) == TRANE_FAN_MODE_AUTO_DRY) {
//        parent->mode = climate::CLIMATE_MODE_DRY;
//      } else {
//        parent->mode = climate::CLIMATE_MODE_FAN_ONLY;
//      }
//    } else
//      parent->mode = climate::CLIMATE_MODE_COOL;
//
//    // Fan Speed
//    if ((remote_state & TRANE_FAN_AUTO) == TRANE_FAN_AUTO || parent->mode == climate::CLIMATE_MODE_HEAT_COOL ||
//        parent->mode == climate::CLIMATE_MODE_DRY) {
//      parent->fan_mode = climate::CLIMATE_FAN_AUTO;
//    } else if ((remote_state & TRANE_FAN_MIN) == TRANE_FAN_MIN) {
//      parent->fan_mode = climate::CLIMATE_FAN_LOW;
//    } else if ((remote_state & TRANE_FAN_MED) == TRANE_FAN_MED) {
//      parent->fan_mode = climate::CLIMATE_FAN_MEDIUM;
//    } else if ((remote_state & TRANE_FAN_MAX) == TRANE_FAN_MAX) {
//      parent->fan_mode = climate::CLIMATE_FAN_HIGH;
//    }
//
//    // Temperature
//    uint8_t temperature_code = remote_state & TRANE_TEMP_MASK;
//    for (uint8_t i = 0; i < TRANE_TEMP_RANGE; i++) {
//      if (TRANE_TEMP_MAP[i] == temperature_code)
//        parent->target_temperature = i + TRANE_TEMP_MIN;
//    }
//  }
//  parent->publish_state();
//
//  return true;
//}

}  // namespace TRANE
}  // namespace esphome
