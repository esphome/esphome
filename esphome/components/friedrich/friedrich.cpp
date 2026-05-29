#include "friedrich.h"

#include <array>

#include "esphome/components/remote_base/aeha_protocol.h"

namespace esphome::friedrich {
// clang-format off
/*
* MW12Y3H - Currently the only supported model. Model field is retained to allow future per-model code differentiation.
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
const uint8_t BYTE3_LVR_MOVE = 0x36;  // hardware feature, not yet exposed
const uint8_t BYTE3_POWER_OFF = 0x40;
const uint8_t BYTE4_FIXED = 0x90;
const uint8_t BYTE5_FIXED = 0x0C;
struct TempEncoding {
  uint8_t fahrenheit;
  uint8_t encoded;
};
static const TempEncoding TEMP_ENCODINGS[] = {
    {88, 0x07}, {86, 0x0B}, {84, 0x03}, {82, 0x0D}, {80, 0x05}, {78, 0x09}, {76, 0x01}, {74, 0x0E},
    {72, 0x06}, {70, 0x0A}, {68, 0x02}, {66, 0x0C}, {64, 0x04}, {62, 0x08}, {60, 0x00},
};
const uint8_t BYTE6_POWER_ON = 0x80;
const uint8_t BYTE7_MODE_AUTO = 0x00;
const uint8_t BYTE7_MODE_COOL = 0x80;
const uint8_t BYTE7_MODE_DRY = 0x40;
const uint8_t BYTE7_MODE_FAN = 0xC0;
const uint8_t BYTE7_MODE_HEAT = 0x20;
const uint8_t BYTE7_MODE_MIN_HEAT = 0xD0;  // hardware mode, not yet exposed
const uint8_t BYTE7_MODE_COIL_DRY = 0x10;  // hardware mode, not yet exposed
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
const uint8_t BYTE12_ECO_ON = 0x00;  // hardware feature, not yet exposed

void FriedrichClimate::dump_config() {
  ClimateIR::dump_config();
  const char *model_str;
  switch (this->model_) {
    case MODEL_MW12Y3H:
    default:
      model_str = "MW12Y3H";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Model: %s", model_str);
  ESP_LOGCONFIG(TAG, "  Using Fahrenheit: %s", YESNO(this->fahrenheit_));
}

void FriedrichClimate::transmit_state() {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_off_();
    return;
  }

  ESP_LOGV(TAG, "Transmit state");

  // Non-heat modes cannot go below 64°F; clamp before building state so publish_state()
  // is never called mid-transmission.
  float effective_min_c = fahrenheit_to_celsius(this->mode == climate::CLIMATE_MODE_HEAT ? TEMP_MIN : 64);
  if (this->target_temperature < effective_min_c) {
    this->target_temperature = effective_min_c;
    this->publish_state();
  }

  std::array<uint8_t, STATE_MESSAGE_LENGTH> remote_state{};

  remote_state[0] = BYTE0_FIXED;
  remote_state[1] = BYTE1_FIXED;
  remote_state[2] = BYTE2_FIXED;
  remote_state[3] = BYTE3_FIXED;
  remote_state[4] = BYTE4_FIXED;
  remote_state[5] = BYTE5_FIXED;

  // Set Temp (F, step to even)
  uint8_t temperature_clamped =
      (uint8_t) roundf(clamp<float>(celsius_to_fahrenheit(this->target_temperature), TEMP_MIN, TEMP_MAX));
  if (temperature_clamped % 2 == 1) {
    temperature_clamped++;
  }
  for (const auto &entry : TEMP_ENCODINGS) {
    if (entry.fahrenheit == temperature_clamped) {
      remote_state[6] = entry.encoded;
      break;
    }
  }

  // Set power on
  remote_state[6] |= BYTE6_POWER_ON;

  // Set mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      remote_state[7] = BYTE7_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[7] = BYTE7_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[7] = BYTE7_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[7] = BYTE7_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      remote_state[7] = BYTE7_MODE_AUTO;
      break;
  }

  // Set fan
  if (this->fan_mode.has_value()) {
    switch (this->fan_mode.value()) {
      case climate::CLIMATE_FAN_HIGH:
        remote_state[8] = BYTE8_FAN_HIGH;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        remote_state[8] = BYTE8_FAN_MED;
        break;
      case climate::CLIMATE_FAN_LOW:
        remote_state[8] = BYTE8_FAN_LOW;
        break;
      case climate::CLIMATE_FAN_QUIET:
        remote_state[8] = BYTE8_FAN_QUIET;
        break;
      case climate::CLIMATE_FAN_AUTO:
      default:
        remote_state[8] = BYTE8_FAN_AUTO;
        break;
    }
  } else {
    remote_state[8] = BYTE8_FAN_AUTO;
  }

  // Set swing
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
    case climate::CLIMATE_SWING_HORIZONTAL:
    case climate::CLIMATE_SWING_BOTH:
      remote_state[8] |= BYTE8_FAN_SWING;
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      break;
  }

  remote_state[9] = BYTE9_FIXED;
  remote_state[10] = BYTE10_FIXED;
  remote_state[11] = BYTE11_FIXED;
  remote_state[12] = BYTE12_ECO_OFF;

  remote_state[STATE_MESSAGE_LENGTH - 1] = this->checksum_state_(remote_state.data());

  this->transmit_(remote_state.data(), remote_state.size());

  this->power_ = true;
}

void FriedrichClimate::transmit_off_() {
  ESP_LOGV(TAG, "Transmit off");

  if (this->power_) {
    std::array<uint8_t, STATE_MESSAGE_LENGTH> remote_state{};
    remote_state[0] = BYTE0_FIXED;
    remote_state[1] = BYTE1_FIXED;
    remote_state[2] = BYTE2_FIXED;
    remote_state[3] = BYTE3_POWER_OFF;
    remote_state[4] = this->checksum_util_(remote_state.data());

    this->transmit_(remote_state.data(), remote_state.size());
    this->power_ = false;
  }
}

void FriedrichClimate::transmit_(const uint8_t *data, uint8_t len) {
  ESP_LOGV(TAG, "Transmit message");

  auto transmit = this->transmitter_->transmit();
  auto *dst = transmit.get_data();
  dst->set_carrier_frequency(CARRIER_FREQUENCY);

  remote_base::AEHAData aeha_data;
  aeha_data.address = CARRIER_ADDRESS;
  aeha_data.data.assign(data, data + len);
  remote_base::AEHAProtocol protocol;
  protocol.dump(aeha_data);
  protocol.encode(dst, aeha_data);
  transmit.perform();
}
/*
 * https://gist.github.com/GeorgeDewar/11171561
 * 1. Reverse (flip) bytes 6 - 13)
 * 2. Sum those bytes
 * 3. (208 - sum) % 256
 * 4. Reverse (flip) bytes of result
 */
uint8_t FriedrichClimate::checksum_state_(const uint8_t *data) {
  uint8_t chksm = 0;
  for (uint8_t i = 6; i < STATE_MESSAGE_LENGTH - 1; ++i) {
    chksm += reverse_bits(data[i]);
  }
  chksm = (208 - chksm) % 256;
  chksm = reverse_bits(chksm);
  return chksm;
}

uint8_t FriedrichClimate::checksum_util_(const uint8_t *data) { return 255 - data[3]; }

bool FriedrichClimate::on_receive(remote_base::RemoteReceiveData src) {
  ESP_LOGV(TAG, "Received message");
  bool received = false;
  optional<remote_base::AEHAData> odata = remote_base::AEHAProtocol().decode(src);
  if (odata.has_value()) {
    const remote_base::AEHAData &data = odata.value();
    if (data.data.size() == STATE_MESSAGE_LENGTH_UTIL &&
        data.data.at(STATE_MESSAGE_LENGTH_UTIL - 1) == checksum_util_(data.data.data())) {
      // Not looking for other types of messages
      if (data.data.at(3) == BYTE3_POWER_OFF) {
        ESP_LOGV(TAG, "Received off message");
        this->mode = climate::CLIMATE_MODE_OFF;
        this->power_ = false;
        received = true;
      }
    } else if (data.data.size() == STATE_MESSAGE_LENGTH &&
               data.data.at(STATE_MESSAGE_LENGTH - 1) == checksum_state_(data.data.data())) {
      received = true;
      // Strip the power-on flag (bit 7) to isolate the temperature encoding.
      uint8_t byte6 = data.data.at(6) & ~BYTE6_POWER_ON;
      // Only Fahrenheit is supported; use_fahrenheit: false is rejected at config validation.
      for (const auto &entry : TEMP_ENCODINGS) {
        if (entry.encoded == byte6) {
          this->target_temperature = fahrenheit_to_celsius(entry.fahrenheit);
          break;
        }
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
      if (byte8 & BYTE8_FAN_SWING) {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        byte8 &= ~BYTE8_FAN_SWING;
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
  if (received) {
    this->publish_state();
  }
  return received;
}

}  // namespace esphome::friedrich
