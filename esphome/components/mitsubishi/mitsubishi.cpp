#include "mitsubishi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mitsubishi {

static const char *const TAG = "mitsubishi.climate";

const uint32_t MITSUBISHI_OFF = 0x00;

const uint8_t MITSUBISHI_HEAT = 0x08;
const uint8_t MITSUBISHI_DRY = 0x10;
const uint8_t MITSUBISHI_COOL = 0x18;
const uint8_t MITSUBISHI_AUTO = 0x20;

const uint8_t MITSUBISHI_FAN_1 = 0x01;
const uint8_t MITSUBISHI_FAN_2 = 0x02;
const uint8_t MITSUBISHI_FAN_3 = 0x03;
const uint8_t MITSUBISHI_FAN_4 = 0x04;

// Pulse parameters in usec
// See: https://www.analysir.com/blog/wp-content/uploads/2014/12/Mitsubishi_AC_IR_Signal_Structure.jpg?x69441
const uint16_t MITSUBISHI_BIT_MARK = 430;
const uint16_t MITSUBISHI_ONE_SPACE = 1250;
const uint16_t MITSUBISHI_ZERO_SPACE = 390;
const uint16_t MITSUBISHI_HEADER_MARK = 3500;
const uint16_t MITSUBISHI_HEADER_SPACE = 1700;
const uint16_t MITSUBISHI_MIN_GAP = 17500;

void MitsubishiClimate::transmit_state() {
  // See https://www.analysir.com/blog/wp-content/uploads/2014/12/Mitsubishi_AC_IR_Signal_Fields.jpg?x29451
  // Byte 0-4: Constant: 0x23, 0xCB, 0x26, 0x01, 0x00
  // Byte 5: On=0x20, Off: 0x00
  // Byte 6: HVAC Mode (See constants above (Heat/Dry/Cool/Auto)
  // Byte 7: Temp (Lower 4 bit) Example: 0x00 = 0°C+MITSUBISHI_TEMP_MIN = 16°C; 0x07 = 7°C+MITSUBISHI_TEMP_MIN = 23°C
  // Byte 8: Sends also the state similar to Byte 6, but in a strange way. Values taken from IR-Remote
  // Byte 9: Fan/Vane Default: 0x58 = 0101 1000 --> Fan Auto, Vane Pos. 3
  //         The Remote doesn't behave constant here. The code differs depending on what you changed, even if the result
  //         is the same:
  //         0x58: Fan was set to Auto previously, and remote cycled through Vanne position until "Pos 3" was reached
  //         0x98: Vane was already on "Pos 3" and remote cycled through Fan until "Auto" was selected.
  //         Both cases show the identical state on the Remote, but the code differs
  // Byte 10: Current time as configured on remote
  // Byte 11: Stop time of HVAC (0x00 for no setting)
  // Byte 12: Start time of HVAC (0x00 for no setting)
  // Byte 13: Enable/Disable timer (No Timer: 0x00; 0x05: StartTimer; 0x03: EndTimer; 0x07: Start+EndTimer)
  // Byte 14-16: Constant: 0x00, 0x00, 0x00
  // Byte 17: Checksum: SUM[Byte0...Byte16]
  uint32_t remote_state[18] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x20, 0x08, 0x00, 0x30,
                               0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      remote_state[6] = MITSUBISHI_COOL;
      remote_state[8] = 0x36;  // Value taken from remote control, difference between docu and reality
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[6] = MITSUBISHI_HEAT;
      remote_state[8] = 0x30;  // Value taken from remote control, difference between docu and reality
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state[6] = MITSUBISHI_AUTO;
      remote_state[8] = 0x36;  // Value taken from remote control, difference between docu and reality
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[6] = MITSUBISHI_DRY;
      remote_state[8] = 0x32;  // Value taken from remote control, difference between docu and reality
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state[5] = MITSUBISHI_OFF;
      break;
  }

  // Temp
  if (this->mode == climate::CLIMATE_MODE_DRY) {
    remote_state[7] = 24 - MITSUBISHI_TEMP_MIN;  // Remote sends always 24°C if "Dry" mode is selected
  } else {
    remote_state[7] = (uint8_t) roundf(
        clamp<float>(this->target_temperature, MITSUBISHI_TEMP_MIN, MITSUBISHI_TEMP_MAX) - MITSUBISHI_TEMP_MIN);
  }

  // Fan Speed & Vanne
  remote_state[9] = 0x00;  // reset
  // Fan First
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      // used fan mode 2 as lowest since CLIMATE_FAN offers only 3 states
      remote_state[9] = remote_state[9] | MITSUBISHI_FAN_2;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[9] = remote_state[9] | MITSUBISHI_FAN_3;
      break;
    case climate::CLIMATE_FAN_HIGH:
      remote_state[9] = remote_state[9] | MITSUBISHI_FAN_4;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      remote_state[9] = remote_state[9] | 0x80;
      break;
  }
  // Vanne
  if (this->swing_mode == climate::CLIMATE_SWING_OFF) {
    remote_state[9] = remote_state[9] | 0x40;  // Off--> Auto position (High if cooling, low if heating)
  } else if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    remote_state[9] = remote_state[9] | 0x78;  // Vanne move
  }

  // ESP_LOGV(TAG, "Sending Mitsubishi target temp: %.1f state: %02X mode: %02X temp: %02X Fan+Vane: %02X",
  // this->target_temperature,
  //          remote_state[5], remote_state[6], remote_state[7], remote_state[9]);

  // Checksum
  for (int i = 0; i < 17; i++) {
    remote_state[17] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);
  // repeat twice
  for (uint16_t r = 0; r < 2; r++) {
    // Header
    data->mark(MITSUBISHI_HEADER_MARK);
    data->space(MITSUBISHI_HEADER_SPACE);
    // Data
    for (uint8_t i : remote_state) {
      for (uint8_t j = 0; j < 8; j++) {
        data->mark(MITSUBISHI_BIT_MARK);
        bool bit = i & (1 << j);
        data->space(bit ? MITSUBISHI_ONE_SPACE : MITSUBISHI_ZERO_SPACE);
      }
    }
    // Footer
    if (r == 0) {
      data->mark(MITSUBISHI_BIT_MARK);
      data->space(MITSUBISHI_MIN_GAP);  // Pause before repeating
    }
  }
  data->mark(MITSUBISHI_BIT_MARK);

  transmit.perform();
}

bool MitsubishiClimate::parse_state_frame_(const uint8_t frame[]) { return false; }

bool MitsubishiClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[18] = {};

  if (!data.expect_item(MITSUBISHI_HEADER_MARK, MITSUBISHI_HEADER_SPACE)) {
    return false;
  }

  for (uint8_t pos = 0; pos < 18; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(MITSUBISHI_BIT_MARK, MITSUBISHI_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(MITSUBISHI_BIT_MARK, MITSUBISHI_ZERO_SPACE)) {
        return false;
      }
    }
    state_frame[pos] = byte;

    // Check Header && Footer
    if ((pos == 0 && byte != 0x23) || (pos == 1 && byte != 0xCB) || (pos == 2 && byte != 0x26) ||
        (pos == 3 && byte != 0x01) || (pos == 4 && byte != 0x00) ||
        ((pos == 14 || pos == 15 || pos == 16) && byte != 0x00)) {
      return false;
    }
  }

  // On/Off and Mode
  if (state_frame[5] == MITSUBISHI_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (state_frame[6]) {
      case MITSUBISHI_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case MITSUBISHI_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case MITSUBISHI_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case MITSUBISHI_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
    }
  }

  // Temp
  this->target_temperature = state_frame[7] + MITSUBISHI_TEMP_MIN;

  // Fan
  uint8_t fan = state_frame[9] & 0x87;  //(Bit 8 = Auto, Bit 1,2,3 = Speed)
  switch (fan) {
    case 0x00:  // Fan = Auto but Vane(Swing) in specific mode
    case 0x80:  // Fan & Vane = Auto
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case MITSUBISHI_FAN_1:  // Lowest modes mapped together, CLIMATE_FAN offers only 3 states
    case MITSUBISHI_FAN_2:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case MITSUBISHI_FAN_3:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case MITSUBISHI_FAN_4:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
  }

  // Vane (Swing Vertical) is active if
  //  - Byte is 01111000 (Fan (Bit8,3,2,1) in invalid state since not auto (Bit8) but also no speed set (Bit1,2,3))
  //  - Byte is 10111000 (Fan (Bit8) in Auto Mode)
  if ((state_frame[9] & 0x78) == 0x78 || state_frame[9] == 0xB8) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  // ESP_LOGD(TAG, "Data: %02X", state_frame[9]);

  this->publish_state();
  return true;
}

}  // namespace mitsubishi
}  // namespace esphome
