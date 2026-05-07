#include "friedrich.h"

namespace esphome {
namespace friedrich {
// clang-format off
/*
* M12YH - Currently all code for this model, model_ is not used.
* AEHA
* Byte0  Fixed = 0x00
* Byte1  Fixed = 0x08
* Byte2  Fixed = 0x08
* Byte3  Command: Move Louver = 0x36 Off = 0x40, else Fixed = 0x7F
* Byte4  Checksum when command, else Fixed = 0x90
* Byte5  N/A when command, else Fixed = 0x0C
* Byte6  N/A when command, else Temp(F), add 0x80 for On
* Byte7  N/A when command, else Mode
* Byte8  N/A when command, else Fan, add 0x08 for Vertical Swing
* Byte9 N/A when command, else Fixed = 0x00 // not interested
* Byte10 N/A when command, else Fixed = 0x00 // not interested
* Byte11 N/A when command, else Fixed = 0x00 // not interested
* Byte12 N/A when command, else Economy
* Byte13 N/A when command, else CheckSum
*/
// clang-format on
static const char *const TAG = "friedrich";

const uint8_t STATE_MESSAGE_LENGTH_UTIL = 5;
const uint16_t CARRIER_ADDRESS = 0x28C6;
const uint32_t CARRIER_FREQUENCY = 38000;

const uint8_t BYTE0_FIXED = 0x00;
const uint8_t BYTE1_FIXED = 0x08;
const uint8_t BYTE2_FIXED = 0x08;
const uint8_t BYTE3_FIXED = 0x7F;
const uint8_t BYTE3_LVR_MOVE = 0x36;
const uint8_t BYTE3_POWER_OFF = 0x40;
const uint8_t BYTE4_FIXED = 0x90;
const uint8_t BYTE5_FIXED = 0x0C;
const uint8_t BYTE6_TEMP_88 = 0x07;
const uint8_t BYTE6_TEMP_86 = 0x0B;
const uint8_t BYTE6_TEMP_84 = 0x03;
const uint8_t BYTE6_TEMP_82 = 0x0D;
const uint8_t BYTE6_TEMP_80 = 0x05;
const uint8_t BYTE6_TEMP_78 = 0x09;
const uint8_t BYTE6_TEMP_76 = 0x01;
const uint8_t BYTE6_TEMP_74 = 0x0E;
const uint8_t BYTE6_TEMP_72 = 0x06;
const uint8_t BYTE6_TEMP_70 = 0x0A;
const uint8_t BYTE6_TEMP_68 = 0x02;
const uint8_t BYTE6_TEMP_66 = 0x0C;
const uint8_t BYTE6_TEMP_64 = 0x04;
const uint8_t BYTE6_TEMP_62 = 0x08;
const uint8_t BYTE6_TEMP_60 = 0x00;
const uint8_t BYTE6_POWER_ON = 0x80;
const uint8_t BYTE7_MODE_AUTO = 0x00;
const uint8_t BYTE7_MODE_COOL = 0x80;
const uint8_t BYTE7_MODE_DRY = 0x40;
const uint8_t BYTE7_MODE_FAN = 0xC0;
const uint8_t BYTE7_MODE_HEAT = 0x20;
const uint8_t BYTE7_MODE_MIN_HEAT = 0xD0;
const uint8_t BYTE7_MODE_COIL_DRY = 0x10;
const uint8_t BYTE8_FAN_AUTO = 0x00;
const uint8_t BYTE8_FAN_HIGH = 0x80;
const uint8_t BYTE8_FAN_MED = 0x40;
const uint8_t BYTE8_FAN_LOW = 0xC0;
const uint8_t BYTE8_FAN_QUIET = 0x20;
const uint8_t BYTE8_FAN_SWING = 0x08;
const uint8_t BYTE9_FIXED = 0x00;
const uint8_t BYTE10_FIXED = 0x00;
const uint8_t BYTE11_FIXED = 0x00;
const uint8_t BYTE12_ECO_OFF = 0x04;
const uint8_t BYTE12_ECO_ON = 0x00;

void FriedrichClimate::transmit_state() {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_off_();
    return;
  }

  ESP_LOGV(TAG, "Transmit state");

  std::fill(this->remote_state_.begin(), this->remote_state_.end(), 0);

  this->remote_state_[0] = BYTE0_FIXED;
  this->remote_state_[1] = BYTE1_FIXED;
  this->remote_state_[2] = BYTE2_FIXED;
  this->remote_state_[3] = BYTE3_FIXED;
  this->remote_state_[4] = BYTE4_FIXED;
  this->remote_state_[5] = BYTE5_FIXED;

  if (this->fahrenheit_) {
    // Set Temp (F, step to even)
    uint8_t temperature_clamped =
        (uint8_t) roundf(clamp<float>(celsius_to_fahrenheit(this->target_temperature), TEMP_MIN, TEMP_MAX));
    if (temperature_clamped % 2 == 1) {
      temperature_clamped++;
    }
    switch (temperature_clamped) {
      case 88:
        this->remote_state_[6] = BYTE6_TEMP_88;
        break;
      case 86:
        this->remote_state_[6] = BYTE6_TEMP_86;
        break;
      case 84:
        this->remote_state_[6] = BYTE6_TEMP_84;
        break;
      case 82:
        this->remote_state_[6] = BYTE6_TEMP_82;
        break;
      case 80:
        this->remote_state_[6] = BYTE6_TEMP_80;
        break;
      case 78:
        this->remote_state_[6] = BYTE6_TEMP_78;
        break;
      case 76:
        this->remote_state_[6] = BYTE6_TEMP_76;
        break;
      case 74:
        this->remote_state_[6] = BYTE6_TEMP_74;
        break;
      case 72:
        this->remote_state_[6] = BYTE6_TEMP_72;
        break;
      case 70:
        this->remote_state_[6] = BYTE6_TEMP_70;
        break;
      case 68:
        this->remote_state_[6] = BYTE6_TEMP_68;
        break;
      case 66:
        this->remote_state_[6] = BYTE6_TEMP_66;
        break;
      case 64:
        this->remote_state_[6] = BYTE6_TEMP_64;
        break;
      case 62:
        if (this->mode == climate::CLIMATE_MODE_HEAT) {
          this->remote_state_[6] = BYTE6_TEMP_62;
        } else {
          this->remote_state_[6] = BYTE6_TEMP_64;
          this->target_temperature = fahrenheit_to_celsius(64);
          this->publish_state();
        }
        break;
      case 60:
      default:
        if (this->mode == climate::CLIMATE_MODE_HEAT) {
          this->remote_state_[6] = BYTE6_TEMP_60;
          this->target_temperature = fahrenheit_to_celsius(60);
          this->publish_state();
        } else {
          this->remote_state_[6] = BYTE6_TEMP_64;
          this->target_temperature = fahrenheit_to_celsius(64);
          this->publish_state();
        }
        break;
    }
  } else {
    ESP_LOGI(TAG, "use_fahrenheit: False is not a valid setting at present, code needs modification.");
  }

  // Set power on
  this->remote_state_[6] = this->remote_state_[6] + BYTE6_POWER_ON;

  // Set mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      this->remote_state_[7] = BYTE7_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      this->remote_state_[7] = BYTE7_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_DRY:
      this->remote_state_[7] = BYTE7_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      this->remote_state_[7] = BYTE7_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      this->remote_state_[7] = BYTE7_MODE_AUTO;
      break;
  }

  // Set fan
  if (fan_mode.has_value()) {
    switch (this->fan_mode.value()) {
      case climate::CLIMATE_FAN_HIGH:
        this->remote_state_[8] = BYTE8_FAN_HIGH;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        this->remote_state_[8] = BYTE8_FAN_MED;
        break;
      case climate::CLIMATE_FAN_LOW:
        this->remote_state_[8] = BYTE8_FAN_LOW;
        break;
      case climate::CLIMATE_FAN_QUIET:
        this->remote_state_[8] = BYTE8_FAN_QUIET;
        break;
      case climate::CLIMATE_FAN_AUTO:
      default:
        this->remote_state_[8] = BYTE8_FAN_AUTO;
        break;
    }
  } else {
    this->remote_state_[8] = BYTE8_FAN_AUTO;
  }

  // Set swing
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
    case climate::CLIMATE_SWING_HORIZONTAL:
    case climate::CLIMATE_SWING_BOTH:
      this->remote_state_[8] = this->remote_state_[8] + BYTE8_FAN_SWING;
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      break;
  }

  this->remote_state_[9] = BYTE9_FIXED;
  this->remote_state_[10] = BYTE10_FIXED;
  this->remote_state_[11] = BYTE11_FIXED;
  this->remote_state_[12] = BYTE12_ECO_OFF;  // investigate

  this->remote_state_[STATE_MESSAGE_LENGTH - 1] = this->checksum_state_(&this->remote_state_);

  this->transmit_(&this->remote_state_);

  this->power_ = true;
}

void FriedrichClimate::transmit_off_() {
  ESP_LOGV(TAG, "Transmit off");

  if (this->power_) {
    std::fill(this->remote_state_.begin(), this->remote_state_.end(), 0);
    this->remote_state_[0] = BYTE0_FIXED;
    this->remote_state_[1] = BYTE1_FIXED;
    this->remote_state_[2] = BYTE2_FIXED;
    this->remote_state_[3] = BYTE3_POWER_OFF;
    this->remote_state_[4] = this->checksum_util_(&this->remote_state_);

    this->transmit_(&this->remote_state_);
    this->power_ = false;
  }
}

void FriedrichClimate::transmit_(std::vector<uint8_t> *message) {
  ESP_LOGV(TAG, "Transmit message");

  auto transmit = this->transmitter_->transmit();
  auto *dst = transmit.get_data();
  dst->set_carrier_frequency(CARRIER_FREQUENCY);

  data_.address = CARRIER_ADDRESS;
  data_.data = *message;
  protocol_.dump(data_);
  protocol_.encode(dst, data_);
  transmit.perform();
}
/*
 * https://gist.github.com/GeorgeDewar/11171561
 * 1. Reverse (flip) bytes 6 - 13)
 * 2. Sum those bytes
 * 3. (208 - sum) % 256
 * 4. Reverse (flip) bytes of result
 */
uint8_t FriedrichClimate::checksum_state_(std::vector<uint8_t> *message) {
  uint8_t chksm = 0;
  for (uint8_t i = 6; i < STATE_MESSAGE_LENGTH - 1; ++i) {
    chksm += reverse_bits(message->at(i));
  }
  chksm = (208 - chksm) % 256;
  chksm = reverse_bits(chksm);
  return chksm;
}

uint8_t FriedrichClimate::checksum_util_(std::vector<uint8_t> *message) { return 255 - message->at(3); }

bool FriedrichClimate::on_receive(remote_base::RemoteReceiveData src) {
  ESP_LOGV(TAG, "Received message");
  bool rtrn = false;
  optional<remote_base::AEHAData> odata = protocol_.decode(src);
  if (odata.has_value()) {
    remote_base::AEHAData data = odata.value();
    if (data.data.size() == STATE_MESSAGE_LENGTH_UTIL &&
        data.data.at(STATE_MESSAGE_LENGTH_UTIL - 1) == checksum_util_(&data.data)) {
      // Not looking for other types of messages
      if (data.data.at(3) == BYTE3_POWER_OFF) {
        ESP_LOGV(TAG, "Received off message");
        this->mode = climate::CLIMATE_MODE_OFF;
        this->power_ = false;
        rtrn = true;
      }
    } else if (data.data.size() == STATE_MESSAGE_LENGTH &&
               data.data.at(STATE_MESSAGE_LENGTH - 1) == checksum_state_(&data.data)) {
      rtrn = true;
      // Set temperature (don't bother if off)
      uint8_t byte6 = 0;
      int8_t tbyte6 = data.data.at(6) - BYTE6_POWER_ON;
      if (tbyte6 < 0) {
        byte6 = data.data.at(6);
      } else {
        byte6 = data.data.at(6) - BYTE6_POWER_ON;
      }
      if (this->fahrenheit_) {
        switch (byte6) {
          case BYTE6_TEMP_88:
            this->target_temperature = fahrenheit_to_celsius(88);
            break;
          case BYTE6_TEMP_86:
            this->target_temperature = fahrenheit_to_celsius(86);
            break;
          case BYTE6_TEMP_84:
            this->target_temperature = fahrenheit_to_celsius(84);
            break;
          case BYTE6_TEMP_82:
            this->target_temperature = fahrenheit_to_celsius(82);
            break;
          case BYTE6_TEMP_80:
            this->target_temperature = fahrenheit_to_celsius(80);
            break;
          case BYTE6_TEMP_78:
            this->target_temperature = fahrenheit_to_celsius(78);
            break;
          case BYTE6_TEMP_76:
            this->target_temperature = fahrenheit_to_celsius(76);
            break;
          case BYTE6_TEMP_74:
            this->target_temperature = fahrenheit_to_celsius(74);
            break;
          case BYTE6_TEMP_72:
            this->target_temperature = fahrenheit_to_celsius(72);
            break;
          case BYTE6_TEMP_70:
            this->target_temperature = fahrenheit_to_celsius(70);
            break;
          case BYTE6_TEMP_68:
            this->target_temperature = fahrenheit_to_celsius(68);
            break;
          case BYTE6_TEMP_66:
            this->target_temperature = fahrenheit_to_celsius(66);
            break;
          case BYTE6_TEMP_64:
            this->target_temperature = fahrenheit_to_celsius(64);
            break;
          case BYTE6_TEMP_62:
            this->target_temperature = fahrenheit_to_celsius(62);
            break;
          case BYTE6_TEMP_60:
            this->target_temperature = fahrenheit_to_celsius(60);
            break;
          default:
            break;
        }
      } else {
        ESP_LOGI(TAG, "use_fahrenheit: False is not a valid setting at present, code needs modification.");
      }

      // Set mode
      uint8_t byte7 = data.data.at(7);
      switch (byte7) {
        case BYTE7_MODE_COOL:
          this->mode = climate::CLIMATE_MODE_COOL;
          break;
        case BYTE7_MODE_HEAT:
          this->mode = climate::CLIMATE_MODE_HEAT;
          break;
        case BYTE7_MODE_DRY:
          this->mode = climate::CLIMATE_MODE_DRY;
          break;
        case BYTE7_MODE_FAN:
          this->mode = climate::CLIMATE_MODE_FAN_ONLY;
          break;
        case BYTE7_MODE_AUTO:
        default:
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
          break;
      }

      // Set fan
      uint8_t byte8 = data.data.at(8);
      // Check for Swing
      uint8_t last_nibble = byte8 & 0x0F;
      if (last_nibble == BYTE8_FAN_SWING) {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        byte8 = byte8 - BYTE8_FAN_SWING;
      } else {
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      }
      switch (byte8) {
        case BYTE8_FAN_HIGH:
          this->fan_mode = climate::CLIMATE_FAN_HIGH;
          break;
        case BYTE8_FAN_MED:
          this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
          break;
        case BYTE8_FAN_LOW:
          this->fan_mode = climate::CLIMATE_FAN_LOW;
          break;
        case BYTE8_FAN_QUIET:
          this->fan_mode = climate::CLIMATE_FAN_QUIET;
          break;
        case BYTE8_FAN_AUTO:
        default:
          this->fan_mode = climate::CLIMATE_FAN_AUTO;
          break;
      }
    }
  }
  if (rtrn) {
    this->publish_state();
  }
  return rtrn;
}

}  // namespace friedrich
}  // namespace esphome
