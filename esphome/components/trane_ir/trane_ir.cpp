#include "trane_ir.h"
#include "esphome/components/remote_base/trane_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace trane_ir {

static const char *const TAG = "trane.climate";

static const uint32_t TRANE_CONSTANT_1 = 0x4A0C0000;
static const uint32_t TRANE_CONSTANT_2 = 0x4000;

static const uint32_t TRANE_OFF = 0xB27BE0;
static const uint32_t TRANE_SWING = 0xB26BE0;
static const uint32_t TRANE_LED = 0xB5F5A5;
static const uint32_t TRANE_SILENCE_FP = 0xB5F5B6;

static const uint8_t TRANE_MODE_NBITS = 3;
enum TRANE_MODE{
  MODE_AUTO,
  MODE_COOL,
  MODE_DRY,
  MODE_FAN,
  MODE_HEAT,
};

static const uint8_t TRANE_FAN_SPEED_NBITS = 3;
enum TRANE_FAN_SPEED{
  SPEED_AUTO,
  SPEED_LOW,
  SPEED_MED,
  SPEED_HIGH,
};

enum TRANE_FAN_VERTICAL_SWING{
  SWING_OFF,
  SWING_FULL,
  SWING_POSITION1,
  SWING_POSITION2,
  SWING_POSITION3,
  SWING_POSITION4,
  SWING_POSITION5,
  SWING_UNKNOWN1,
  SWING_BOTTOM,
  SWING_MIDDLE,
  SWING_UNKNOWN,
  SWING_TOP,
};

// Temperature
static const uint8_t TRANE_TEMP_NBITS = 4;
static const uint8_t TRANE_TEMP_RANGE = TRANE_TEMP_MAX - TRANE_TEMP_MIN + 1;

static const uint8_t CHECKSUM_NBITS = 4;


void TraneClimate::transmit_state() {
  enum TRANE_MODE mode_state = MODE_AUTO;
  switch(this->mode){
    case climate::CLIMATE_MODE_COOL:
      mode_state = MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      mode_state = MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_DRY:
      mode_state = MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      mode_state = MODE_FAN;
      break;
  }

  uint8_t temperature_state = 0;

  if (this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_HEAT){
    auto temp = roundf(clamp<float>(this->target_temperature, TRANE_TEMP_MIN, TRANE_TEMP_MAX));
    temperature_state = (temp-16);
  }

  TRANE_FAN_SPEED fan_speed_state = SPEED_AUTO;
  switch(this->fan_mode.value()){
    case climate::CLIMATE_FAN_AUTO:
      fan_speed_state = SPEED_AUTO;
      break;
    case climate::CLIMATE_FAN_LOW:
      fan_speed_state = SPEED_LOW;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed_state = SPEED_MED;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed_state = SPEED_HIGH;
      break;
  }

  TRANE_FAN_VERTICAL_SWING vertical_swing_state = SWING_OFF;
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      vertical_swing_state = SWING_FULL;

  }

  //constant that has to do with unit light, power, and something else I haven't figured out in the protocol yet
  uint32_t remote_state_1 = TRANE_CONSTANT_1;
  remote_state_1 |= (temperature_state << (9 - TRANE_TEMP_NBITS));
  //Implicit 0 for sleep
  //I believe there must be a 1 here for a boolean on whether swing mode is on or off
  remote_state_1 |= (1 << 3);
  remote_state_1 |= (fan_speed_state << (4 - TRANE_FAN_SPEED_NBITS));
  //power boolean
  remote_state_1 |= 1;


  uint8_t checksum_a = mode_state | (1 << 3) | (fan_speed_state << 4) | (1 << 6);
  uint8_t checksum_b = temperature_state;
  uint8_t checksum_c = 0;
  uint8_t checksum = (14 + checksum_a + checksum_b + checksum_c) & 15;

  uint32_t remote_state_2 = TRANE_CONSTANT_2;
  remote_state_2 |= (checksum << (32 - CHECKSUM_NBITS));
  // TEMP DISPLAY
  remote_state_2 |= (2 << 8);
  remote_state_2 |= vertical_swing_state;

  if(this->mode == climate::CLIMATE_MODE_OFF){
    mode_state = MODE_COOL;
    remote_state_1 = 0x4A0634EE;
    remote_state_2 = 0x70004301;
  }

  remote_base::TraneData remote_state;
  remote_state.mode = mode_state;
  remote_state.trane_data_1 = remote_state_1;
  remote_state.trane_data_2 = remote_state_2;

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  remote_base::TraneProtocol().encode(data, remote_state);
  transmit.perform();
}

}  // namespace TRANE
}  // namespace esphome
