#include "toshiba.h"

#include <vector>

namespace esphome {
namespace toshiba {

struct RacPt1411hwruFanSpeed {
  uint8_t code1;
  uint8_t code2;
};

static const char *const TAG = "toshiba.climate";
// Timings for IR bits/data
const uint16_t TOSHIBA_HEADER_MARK = 4380;
const uint16_t TOSHIBA_HEADER_SPACE = 4370;
const uint16_t TOSHIBA_GAP_SPACE = 5480;
const uint16_t TOSHIBA_PACKET_SPACE = 10500;
const uint16_t TOSHIBA_BIT_MARK = 540;
const uint16_t TOSHIBA_ZERO_SPACE = 540;
const uint16_t TOSHIBA_ONE_SPACE = 1620;
const uint16_t TOSHIBA_CARRIER_FREQUENCY = 38000;
const uint8_t TOSHIBA_HEADER_LENGTH = 4;
// Generic Toshiba commands/flags
const uint8_t TOSHIBA_COMMAND_DEFAULT = 0x01;
const uint8_t TOSHIBA_COMMAND_TIMER = 0x02;
const uint8_t TOSHIBA_COMMAND_POWER = 0x08;
const uint8_t TOSHIBA_COMMAND_MOTION = 0x02;

const uint8_t TOSHIBA_MODE_AUTO = 0x00;
const uint8_t TOSHIBA_MODE_COOL = 0x01;
const uint8_t TOSHIBA_MODE_DRY = 0x02;
const uint8_t TOSHIBA_MODE_HEAT = 0x03;
const uint8_t TOSHIBA_MODE_FAN_ONLY = 0x04;
const uint8_t TOSHIBA_MODE_OFF = 0x07;

const uint8_t TOSHIBA_FAN_SPEED_AUTO = 0x00;
const uint8_t TOSHIBA_FAN_SPEED_QUIET = 0x20;
const uint8_t TOSHIBA_FAN_SPEED_1 = 0x40;
const uint8_t TOSHIBA_FAN_SPEED_2 = 0x60;
const uint8_t TOSHIBA_FAN_SPEED_3 = 0x80;
const uint8_t TOSHIBA_FAN_SPEED_4 = 0xa0;
const uint8_t TOSHIBA_FAN_SPEED_5 = 0xc0;

const uint8_t TOSHIBA_POWER_HIGH = 0x01;
const uint8_t TOSHIBA_POWER_ECO = 0x03;

const uint8_t TOSHIBA_MOTION_SWING = 0x04;
const uint8_t TOSHIBA_MOTION_FIX = 0x00;

// RAC-PT1411HWRU temperature code flag bits
const uint8_t RAC_PT1411HWRU_FLAG_FAH = 0x01;
const uint8_t RAC_PT1411HWRU_FLAG_FRAC = 0x20;
const uint8_t RAC_PT1411HWRU_FLAG_NEG = 0x10;
// RAC-PT1411HWRU temperature short code flags mask
const uint8_t RAC_PT1411HWRU_FLAG_MASK = 0x0F;
// RAC-PT1411HWRU Headers, Footers and such
const uint8_t RAC_PT1411HWRU_MESSAGE_HEADER0 = 0xB2;
const uint8_t RAC_PT1411HWRU_MESSAGE_HEADER1 = 0xD5;
const uint8_t RAC_PT1411HWRU_MESSAGE_LENGTH = 6;
// RAC-PT1411HWRU "Comfort Sense" feature bits
const uint8_t RAC_PT1411HWRU_CS_ENABLED = 0x40;
const uint8_t RAC_PT1411HWRU_CS_DATA = 0x80;
const uint8_t RAC_PT1411HWRU_CS_HEADER = 0xBA;
const uint8_t RAC_PT1411HWRU_CS_FOOTER_AUTO = 0x7A;
const uint8_t RAC_PT1411HWRU_CS_FOOTER_COOL = 0x72;
const uint8_t RAC_PT1411HWRU_CS_FOOTER_HEAT = 0x7E;
// RAC-PT1411HWRU Swing
const uint8_t RAC_PT1411HWRU_SWING_HEADER = 0xB9;
const std::vector<uint8_t> RAC_PT1411HWRU_SWING_VERTICAL{0xB9, 0x46, 0xF5, 0x0A, 0x04, 0xFB};
const std::vector<uint8_t> RAC_PT1411HWRU_SWING_OFF{0xB9, 0x46, 0xF5, 0x0A, 0x05, 0xFA};
// RAC-PT1411HWRU Fan speeds
const uint8_t RAC_PT1411HWRU_FAN_OFF = 0x7B;
constexpr RacPt1411hwruFanSpeed RAC_PT1411HWRU_FAN_AUTO{0xBF, 0x66};
constexpr RacPt1411hwruFanSpeed RAC_PT1411HWRU_FAN_LOW{0x9F, 0x28};
constexpr RacPt1411hwruFanSpeed RAC_PT1411HWRU_FAN_MED{0x5F, 0x3C};
constexpr RacPt1411hwruFanSpeed RAC_PT1411HWRU_FAN_HIGH{0x3F, 0x64};
// RAC-PT1411HWRU Fan speed for Auto and Dry climate modes
const RacPt1411hwruFanSpeed RAC_PT1411HWRU_NO_FAN{0x1F, 0x65};
// RAC-PT1411HWRU Modes
const uint8_t RAC_PT1411HWRU_MODE_AUTO = 0x08;
const uint8_t RAC_PT1411HWRU_MODE_COOL = 0x00;
const uint8_t RAC_PT1411HWRU_MODE_DRY = 0x04;
const uint8_t RAC_PT1411HWRU_MODE_FAN = 0x04;
const uint8_t RAC_PT1411HWRU_MODE_HEAT = 0x0C;
const uint8_t RAC_PT1411HWRU_MODE_OFF = 0x00;
// RAC-PT1411HWRU Fan-only "temperature"/system off
const uint8_t RAC_PT1411HWRU_TEMPERATURE_FAN_ONLY = 0x0E;
// RAC-PT1411HWRU temperature codes are not sequential; they instead follow a modified Gray code.
//  Hence these look-up tables. In addition, the upper nibble is used here for additional
//  "negative" and "fractional value" flags as required for some temperatures.
// RAC-PT1411HWRU °C Temperatures (short codes)
const std::vector<uint8_t> RAC_PT1411HWRU_TEMPERATURE_C{0x10, 0x00, 0x01, 0x03, 0x02, 0x06, 0x07, 0x05,
                                                        0x04, 0x0C, 0x0D, 0x09, 0x08, 0x0A, 0x0B};
// RAC-PT1411HWRU °F Temperatures (short codes)
const std::vector<uint8_t> RAC_PT1411HWRU_TEMPERATURE_F{0x10, 0x30, 0x00, 0x20, 0x01, 0x21, 0x03, 0x23, 0x02,
                                                        0x22, 0x06, 0x26, 0x07, 0x05, 0x25, 0x04, 0x24, 0x0C,
                                                        0x2C, 0x0D, 0x2D, 0x09, 0x08, 0x28, 0x0A, 0x2A, 0x0B};

void ToshibaClimate::setup() {
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      this->transmit_rac_pt1411hwru_temp_();
      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else
    this->current_temperature = NAN;
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // restore from defaults
    this->mode = climate::CLIMATE_MODE_OFF;
    // initialize target temperature to some value so that it's not NAN
    this->target_temperature =
        roundf(clamp<float>(this->current_temperature, this->minimum_temperature_, this->maximum_temperature_));
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }
  // Set supported modes & temperatures based on model
  this->minimum_temperature_ = this->temperature_min_();
  this->maximum_temperature_ = this->temperature_max_();
  this->swing_modes_ = this->toshiba_swing_modes_();
  // Never send nan to HA
  if (std::isnan(this->target_temperature))
    this->target_temperature = 24;
}

void ToshibaClimate::transmit_state() {
  if (this->model_ == MODEL_RAC_PT1411HWRU_C || this->model_ == MODEL_RAC_PT1411HWRU_F) {
    transmit_rac_pt1411hwru_();
  } else {
    transmit_generic_();
  }
}

void ToshibaClimate::transmit_generic_() {
  uint8_t message[16] = {0};
  uint8_t message_length = 9;

  // Header
  message[0] = 0xf2;
  message[1] = 0x0d;

  // Message length
  message[2] = message_length - 6;

  // First checksum
  message[3] = message[0] ^ message[1] ^ message[2];

  // Command
  message[4] = TOSHIBA_COMMAND_DEFAULT;

  // Temperature
  uint8_t temperature = static_cast<uint8_t>(
      clamp<float>(this->target_temperature, TOSHIBA_GENERIC_TEMP_C_MIN, TOSHIBA_GENERIC_TEMP_C_MAX));
  message[5] = (temperature - static_cast<uint8_t>(TOSHIBA_GENERIC_TEMP_C_MIN)) << 4;

  // Mode and fan
  uint8_t mode;
  switch (this->mode) {
    case climate::CLIMATE_MODE_OFF:
      mode = TOSHIBA_MODE_OFF;
      break;

    case climate::CLIMATE_MODE_HEAT:
      mode = TOSHIBA_MODE_HEAT;
      break;

    case climate::CLIMATE_MODE_COOL:
      mode = TOSHIBA_MODE_COOL;
      break;

    case climate::CLIMATE_MODE_DRY:
      mode = TOSHIBA_MODE_DRY;
      break;

    case climate::CLIMATE_MODE_FAN_ONLY:
      mode = TOSHIBA_MODE_FAN_ONLY;
      break;

    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      mode = TOSHIBA_MODE_AUTO;
  }

  uint8_t fan;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      fan = TOSHIBA_FAN_SPEED_1;
      break;

    case climate::CLIMATE_FAN_MEDIUM:
      fan = TOSHIBA_FAN_SPEED_3;
      break;

    case climate::CLIMATE_FAN_HIGH:
      fan = TOSHIBA_FAN_SPEED_5;
      break;

    case climate::CLIMATE_FAN_AUTO:
    default:
      fan = TOSHIBA_FAN_SPEED_AUTO;
      break;
  }
  message[6] = fan | mode;

  // Zero
  message[7] = 0x00;

  // If timers bit in the command is set, two extra bytes are added here

  // If power bit is set in the command, one extra byte is added here

  // The last byte is the xor of all bytes from [4]
  for (uint8_t i = 4; i < 8; i++) {
    message[8] ^= message[i];
  }

  // Transmit
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  encode_(data, message, message_length, 1);

  transmit.perform();
}

void ToshibaClimate::transmit_rac_pt1411hwru_() {
  uint8_t code = 0, index = 0, message[RAC_PT1411HWRU_MESSAGE_LENGTH * 2] = {0};
  float temperature =
      clamp<float>(this->target_temperature, TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN, TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX);
  float temp_adjd = temperature - TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN;
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  // Byte 0:  Header upper (0xB2)
  message[0] = RAC_PT1411HWRU_MESSAGE_HEADER0;
  // Byte 1:  Header lower (0x4D)
  message[1] = ~message[0];
  // Byte 2u: Fan speed
  // Byte 2l: 1111 (on) or 1011 (off)
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    message[2] = RAC_PT1411HWRU_FAN_OFF;
  } else if ((this->mode == climate::CLIMATE_MODE_HEAT_COOL) || (this->mode == climate::CLIMATE_MODE_DRY)) {
    message[2] = RAC_PT1411HWRU_NO_FAN.code1;
    message[7] = RAC_PT1411HWRU_NO_FAN.code2;
  } else {
    switch (this->fan_mode.value()) {
      case climate::CLIMATE_FAN_LOW:
        message[2] = RAC_PT1411HWRU_FAN_LOW.code1;
        message[7] = RAC_PT1411HWRU_FAN_LOW.code2;
        break;

      case climate::CLIMATE_FAN_MEDIUM:
        message[2] = RAC_PT1411HWRU_FAN_MED.code1;
        message[7] = RAC_PT1411HWRU_FAN_MED.code2;
        break;

      case climate::CLIMATE_FAN_HIGH:
        message[2] = RAC_PT1411HWRU_FAN_HIGH.code1;
        message[7] = RAC_PT1411HWRU_FAN_HIGH.code2;
        break;

      case climate::CLIMATE_FAN_AUTO:
      default:
        message[2] = RAC_PT1411HWRU_FAN_AUTO.code1;
        message[7] = RAC_PT1411HWRU_FAN_AUTO.code2;
    }
  }
  // Byte 3u: ~Fan speed
  // Byte 3l: 0000 (on) or 0100 (off)
  message[3] = ~message[2];
  // Byte 4u: Temp
  if (this->model_ == MODEL_RAC_PT1411HWRU_F) {
    temperature = (temperature * 1.8) + 32;
    temp_adjd = temperature - TOSHIBA_RAC_PT1411HWRU_TEMP_F_MIN;
  }

  index = static_cast<uint8_t>(roundf(temp_adjd));

  if (this->model_ == MODEL_RAC_PT1411HWRU_F) {
    code = RAC_PT1411HWRU_TEMPERATURE_F[index];
    message[9] |= RAC_PT1411HWRU_FLAG_FAH;
  } else {
    code = RAC_PT1411HWRU_TEMPERATURE_C[index];
  }
  if ((this->mode == climate::CLIMATE_MODE_FAN_ONLY) || (this->mode == climate::CLIMATE_MODE_OFF)) {
    code = RAC_PT1411HWRU_TEMPERATURE_FAN_ONLY;
  }

  if (code & RAC_PT1411HWRU_FLAG_FRAC) {
    message[8] |= RAC_PT1411HWRU_FLAG_FRAC;
  }
  if (code & RAC_PT1411HWRU_FLAG_NEG) {
    message[9] |= RAC_PT1411HWRU_FLAG_NEG;
  }
  message[4] = (code & RAC_PT1411HWRU_FLAG_MASK) << 4;
  // Byte 4l: Mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_OFF:
      // zerooooo
      break;

    case climate::CLIMATE_MODE_HEAT:
      message[4] |= RAC_PT1411HWRU_MODE_HEAT;
      break;

    case climate::CLIMATE_MODE_COOL:
      message[4] |= RAC_PT1411HWRU_MODE_COOL;
      break;

    case climate::CLIMATE_MODE_DRY:
      message[4] |= RAC_PT1411HWRU_MODE_DRY;
      break;

    case climate::CLIMATE_MODE_FAN_ONLY:
      message[4] |= RAC_PT1411HWRU_MODE_FAN;
      break;

    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      message[4] |= RAC_PT1411HWRU_MODE_AUTO;
  }

  // Byte 5u: ~Temp
  // Byte 5l: ~Mode
  message[5] = ~message[4];

  if (this->mode != climate::CLIMATE_MODE_OFF) {
    // Byte 6:  Header (0xD5)
    message[6] = RAC_PT1411HWRU_MESSAGE_HEADER1;
    // Byte 7:  Fan speed part 2 (done above)
    // Byte 8: 0x20 for °F frac, else 0 (done above)
    // Byte 9: 0x10=NEG, 0x01=°F (done above)
    // Byte 10: 0
    // Byte 11: Checksum (bytes 6 through 10)
    for (index = 6; index <= 10; index++) {
      message[11] += message[index];
    }
  }
  ESP_LOGV(TAG, "*** Generated codes: 0x%.2X%.2X%.2X%.2X%.2X%.2X  0x%.2X%.2X%.2X%.2X%.2X%.2X", message[0], message[1],
           message[2], message[3], message[4], message[5], message[6], message[7], message[8], message[9], message[10],
           message[11]);

  // load first block of IR code and repeat it once
  encode_(data, &message[0], RAC_PT1411HWRU_MESSAGE_LENGTH, 1);
  // load second block of IR code, if present
  if (message[6] != 0) {
    encode_(data, &message[6], RAC_PT1411HWRU_MESSAGE_LENGTH, 0);
  }

  transmit.perform();

  // Swing Mode
  data->reset();
  data->space(TOSHIBA_PACKET_SPACE);
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      encode_(data, &RAC_PT1411HWRU_SWING_VERTICAL[0], RAC_PT1411HWRU_MESSAGE_LENGTH, 1);
      break;

    case climate::CLIMATE_SWING_OFF:
    default:
      encode_(data, &RAC_PT1411HWRU_SWING_OFF[0], RAC_PT1411HWRU_MESSAGE_LENGTH, 1);
  }

  data->space(TOSHIBA_PACKET_SPACE);
  transmit.perform();

  if (this->sensor_) {
    transmit_rac_pt1411hwru_temp_(true, false);
  }
}

void ToshibaClimate::transmit_rac_pt1411hwru_temp_(const bool cs_state, const bool cs_send_update) {
  if ((this->mode == climate::CLIMATE_MODE_HEAT) || (this->mode == climate::CLIMATE_MODE_COOL) ||
      (this->mode == climate::CLIMATE_MODE_HEAT_COOL)) {
    uint8_t message[RAC_PT1411HWRU_MESSAGE_LENGTH] = {0};
    float temperature = clamp<float>(this->current_temperature, 0.0, TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX + 1);
    auto transmit = this->transmitter_->transmit();
    auto *data = transmit.get_data();
    // "Comfort Sense" feature notes
    // IR Code: 0xBA45 xxXX yyYY
    // xx: Temperature in °C
    //     Bit 6: feature state (on/off)
    //     Bit 7: message contains temperature data for feature (bit 6 must also be set)
    // XX: Bitwise complement of xx
    // yy: Mode: Auto=0x7A, Cool=0x72, Heat=0x7E
    // YY: Bitwise complement of yy
    //
    // Byte 0:  Header upper (0xBA)
    message[0] = RAC_PT1411HWRU_CS_HEADER;
    // Byte 1:  Header lower (0x45)
    message[1] = ~message[0];
    // Byte 2: Temperature in °C
    message[2] = static_cast<uint8_t>(roundf(temperature));
    if (cs_send_update) {
      message[2] |= RAC_PT1411HWRU_CS_ENABLED | RAC_PT1411HWRU_CS_DATA;
    } else if (cs_state) {
      message[2] |= RAC_PT1411HWRU_CS_ENABLED;
    }
    // Byte 3: Bitwise complement of byte 2
    message[3] = ~message[2];
    // Byte 4: Footer upper
    switch (this->mode) {
      case climate::CLIMATE_MODE_HEAT:
        message[4] = RAC_PT1411HWRU_CS_FOOTER_HEAT;
        break;

      case climate::CLIMATE_MODE_COOL:
        message[4] = RAC_PT1411HWRU_CS_FOOTER_COOL;
        break;

      case climate::CLIMATE_MODE_HEAT_COOL:
        message[4] = RAC_PT1411HWRU_CS_FOOTER_AUTO;

      default:
        break;
    }
    // Byte 5: Footer lower/bitwise complement of byte 4
    message[5] = ~message[4];

    ESP_LOGV(TAG, "*** Generated code: 0x%.2X%.2X%.2X%.2X%.2X%.2X", message[0], message[1], message[2], message[3],
             message[4], message[5]);
    // load IR code and repeat it once
    encode_(data, message, RAC_PT1411HWRU_MESSAGE_LENGTH, 1);

    transmit.perform();
  }
}

uint8_t ToshibaClimate::is_valid_rac_pt1411hwru_header_(const uint8_t *message) {
  const std::vector<uint8_t> header{RAC_PT1411HWRU_MESSAGE_HEADER0, RAC_PT1411HWRU_CS_HEADER,
                                    RAC_PT1411HWRU_SWING_HEADER};

  for (auto i : header) {
    if ((message[0] == i) && (message[1] == static_cast<uint8_t>(~i)))
      return i;
  }
  if (message[0] == RAC_PT1411HWRU_MESSAGE_HEADER1)
    return RAC_PT1411HWRU_MESSAGE_HEADER1;

  return 0;
}

bool ToshibaClimate::compare_rac_pt1411hwru_packets_(const uint8_t *message1, const uint8_t *message2) {
  for (uint8_t i = 0; i < RAC_PT1411HWRU_MESSAGE_LENGTH; i++) {
    if (message1[i] != message2[i])
      return false;
  }
  return true;
}

bool ToshibaClimate::is_valid_rac_pt1411hwru_message_(const uint8_t *message) {
  uint8_t checksum = 0;

  switch (is_valid_rac_pt1411hwru_header_(message)) {
    case RAC_PT1411HWRU_MESSAGE_HEADER0:
    case RAC_PT1411HWRU_CS_HEADER:
    case RAC_PT1411HWRU_SWING_HEADER:
      if (is_valid_rac_pt1411hwru_header_(message) && (message[2] == static_cast<uint8_t>(~message[3])) &&
          (message[4] == static_cast<uint8_t>(~message[5]))) {
        return true;
      }
      break;

    case RAC_PT1411HWRU_MESSAGE_HEADER1:
      for (uint8_t i = 0; i < RAC_PT1411HWRU_MESSAGE_LENGTH - 1; i++) {
        checksum += message[i];
      }
      if (checksum == message[RAC_PT1411HWRU_MESSAGE_LENGTH - 1]) {
        return true;
      }
      break;

    default:
      return false;
  }

  return false;
}

bool ToshibaClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t message[18] = {0};
  uint8_t message_length = TOSHIBA_HEADER_LENGTH, temperature_code = 0;

  // Validate header
  if (!data.expect_item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE)) {
    return false;
  }
  // Read incoming bits into buffer
  if (!decode_(&data, message, message_length)) {
    return false;
  }
  // Determine incoming message protocol version and/or length
  if (is_valid_rac_pt1411hwru_header_(message)) {
    // We already received four bytes
    message_length = RAC_PT1411HWRU_MESSAGE_LENGTH - 4;
  } else if ((message[0] ^ message[1] ^ message[2]) != message[3]) {
    // Return false if first checksum was not valid
    return false;
  } else {
    // First checksum was valid so continue receiving the remaining bits
    message_length = message[2] + 2;
  }
  // Decode the remaining bytes
  if (!decode_(&data, &message[4], message_length)) {
    return false;
  }
  // If this is a RAC-PT1411HWRU message, we expect the first packet a second time and also possibly a third packet
  if (is_valid_rac_pt1411hwru_header_(message)) {
    // There is always a space between packets
    if (!data.expect_item(TOSHIBA_BIT_MARK, TOSHIBA_GAP_SPACE)) {
      return false;
    }
    // Validate header 2
    if (!data.expect_item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE)) {
      return false;
    }
    if (!decode_(&data, &message[6], RAC_PT1411HWRU_MESSAGE_LENGTH)) {
      return false;
    }
    // If this is a RAC-PT1411HWRU message, there may also be a third packet.
    // We do not fail the receive if we don't get this; it isn't always present
    if (data.expect_item(TOSHIBA_BIT_MARK, TOSHIBA_GAP_SPACE)) {
      // Validate header 3
      data.expect_item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE);
      if (decode_(&data, &message[12], RAC_PT1411HWRU_MESSAGE_LENGTH)) {
        if (!is_valid_rac_pt1411hwru_message_(&message[12])) {
          // If a third packet was received but the checksum is not valid, fail
          return false;
        }
      }
    }
    if (!compare_rac_pt1411hwru_packets_(&message[0], &message[6])) {
      // If the first two packets don't match each other, fail
      return false;
    }
    if (!is_valid_rac_pt1411hwru_message_(&message[0])) {
      // If the first packet isn't valid, fail
      return false;
    }
  }

  // Header has been verified, now determine protocol version and set the climate component properties
  switch (is_valid_rac_pt1411hwru_header_(message)) {
    // Power, temperature, mode, fan speed
    case RAC_PT1411HWRU_MESSAGE_HEADER0:
      // Get the mode
      switch (message[4] & 0x0F) {
        case RAC_PT1411HWRU_MODE_AUTO:
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
          break;

        // case RAC_PT1411HWRU_MODE_OFF:
        case RAC_PT1411HWRU_MODE_COOL:
          if (((message[4] >> 4) == RAC_PT1411HWRU_TEMPERATURE_FAN_ONLY) && (message[2] == RAC_PT1411HWRU_FAN_OFF)) {
            this->mode = climate::CLIMATE_MODE_OFF;
          } else {
            this->mode = climate::CLIMATE_MODE_COOL;
          }
          break;

        // case RAC_PT1411HWRU_MODE_DRY:
        case RAC_PT1411HWRU_MODE_FAN:
          if ((message[4] >> 4) == RAC_PT1411HWRU_TEMPERATURE_FAN_ONLY) {
            this->mode = climate::CLIMATE_MODE_FAN_ONLY;
          } else {
            this->mode = climate::CLIMATE_MODE_DRY;
          }
          break;

        case RAC_PT1411HWRU_MODE_HEAT:
          this->mode = climate::CLIMATE_MODE_HEAT;
          break;

        default:
          this->mode = climate::CLIMATE_MODE_OFF;
          break;
      }
      // Get the fan speed/mode
      switch (message[2]) {
        case RAC_PT1411HWRU_FAN_LOW.code1:
          this->fan_mode = climate::CLIMATE_FAN_LOW;
          break;

        case RAC_PT1411HWRU_FAN_MED.code1:
          this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
          break;

        case RAC_PT1411HWRU_FAN_HIGH.code1:
          this->fan_mode = climate::CLIMATE_FAN_HIGH;
          break;

        case RAC_PT1411HWRU_FAN_AUTO.code1:
        default:
          this->fan_mode = climate::CLIMATE_FAN_AUTO;
          break;
      }
      // Get the target temperature
      if (is_valid_rac_pt1411hwru_message_(&message[12])) {
        temperature_code =
            (message[4] >> 4) | (message[14] & RAC_PT1411HWRU_FLAG_FRAC) | (message[15] & RAC_PT1411HWRU_FLAG_NEG);
        if (message[15] & RAC_PT1411HWRU_FLAG_FAH) {
          for (size_t i = 0; i < RAC_PT1411HWRU_TEMPERATURE_F.size(); i++) {
            if (RAC_PT1411HWRU_TEMPERATURE_F[i] == temperature_code) {
              this->target_temperature = static_cast<float>((i + TOSHIBA_RAC_PT1411HWRU_TEMP_F_MIN - 32) * 5) / 9;
            }
          }
        } else {
          for (size_t i = 0; i < RAC_PT1411HWRU_TEMPERATURE_C.size(); i++) {
            if (RAC_PT1411HWRU_TEMPERATURE_C[i] == temperature_code) {
              this->target_temperature = i + TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN;
            }
          }
        }
      }
      break;
    // "Comfort Sense" temperature packet
    case RAC_PT1411HWRU_CS_HEADER:
      // "Comfort Sense" feature notes
      // IR Code: 0xBA45 xxXX yyYY
      // xx: Temperature in °C
      //     Bit 6: feature state (on/off)
      //     Bit 7: message contains temperature data for feature (bit 6 must also be set)
      // XX: Bitwise complement of xx
      // yy: Mode: Auto: 7A
      //           Cool: 72
      //           Heat: 7E
      // YY: Bitwise complement of yy
      if ((message[2] & RAC_PT1411HWRU_CS_ENABLED) && (message[2] & RAC_PT1411HWRU_CS_DATA)) {
        // Setting current_temperature this way allows the unit's remote to provide the temperature to HA
        this->current_temperature = message[2] & ~(RAC_PT1411HWRU_CS_ENABLED | RAC_PT1411HWRU_CS_DATA);
      }
      break;
    // Swing mode
    case RAC_PT1411HWRU_SWING_HEADER:
      if (message[4] == RAC_PT1411HWRU_SWING_VERTICAL[4]) {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      } else {
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      }
      break;
    // Generic (old) Toshiba packet
    default:
      uint8_t checksum = 0;
      // Add back the length of the header (we pruned it above)
      message_length += TOSHIBA_HEADER_LENGTH;
      // Validate the second checksum before trusting any more of the message
      for (uint8_t i = TOSHIBA_HEADER_LENGTH; i < message_length - 1; i++) {
        checksum ^= message[i];
      }
      // Did our computed checksum and the provided checksum match?
      if (checksum != message[message_length - 1]) {
        return false;
      }
      // Check if this is a short swing/fix message
      if (message[4] & TOSHIBA_COMMAND_MOTION) {
        // Not supported yet
        return false;
      }

      // Get the mode
      switch (message[6] & 0x0F) {
        case TOSHIBA_MODE_OFF:
          this->mode = climate::CLIMATE_MODE_OFF;
          break;

        case TOSHIBA_MODE_COOL:
          this->mode = climate::CLIMATE_MODE_COOL;
          break;

        case TOSHIBA_MODE_DRY:
          this->mode = climate::CLIMATE_MODE_DRY;
          break;

        case TOSHIBA_MODE_FAN_ONLY:
          this->mode = climate::CLIMATE_MODE_FAN_ONLY;
          break;

        case TOSHIBA_MODE_HEAT:
          this->mode = climate::CLIMATE_MODE_HEAT;
          break;

        case TOSHIBA_MODE_AUTO:
        default:
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
      }

      // Get the target temperature
      this->target_temperature = (message[5] >> 4) + TOSHIBA_GENERIC_TEMP_C_MIN;
  }

  this->publish_state();
  return true;
}

void ToshibaClimate::encode_(remote_base::RemoteTransmitData *data, const uint8_t *message, const uint8_t nbytes,
                             const uint8_t repeat) {
  data->set_carrier_frequency(TOSHIBA_CARRIER_FREQUENCY);

  for (uint8_t copy = 0; copy <= repeat; copy++) {
    data->item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE);

    for (uint8_t byte = 0; byte < nbytes; byte++) {
      for (uint8_t bit = 0; bit < 8; bit++) {
        data->mark(TOSHIBA_BIT_MARK);
        if (message[byte] & (1 << (7 - bit))) {
          data->space(TOSHIBA_ONE_SPACE);
        } else {
          data->space(TOSHIBA_ZERO_SPACE);
        }
      }
    }
    data->item(TOSHIBA_BIT_MARK, TOSHIBA_GAP_SPACE);
  }
}

bool ToshibaClimate::decode_(remote_base::RemoteReceiveData *data, uint8_t *message, const uint8_t nbytes) {
  for (uint8_t byte = 0; byte < nbytes; byte++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data->expect_item(TOSHIBA_BIT_MARK, TOSHIBA_ONE_SPACE)) {
        message[byte] |= 1 << (7 - bit);
      } else if (data->expect_item(TOSHIBA_BIT_MARK, TOSHIBA_ZERO_SPACE)) {
        message[byte] &= static_cast<uint8_t>(~(1 << (7 - bit)));
      } else {
        return false;
      }
    }
  }
  return true;
}

}  // namespace toshiba
}  // namespace esphome
