#include "ykh531e.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ykh531e {

static const char *const TAG = "ykh531e.climate";

uint8_t encode_temperature_celsius(float temperature) {
  if (temperature > TEMP_MAX) {
    return TEMP_MAX - 8;
  }
  if (temperature < TEMP_MIN) {
    return TEMP_MIN - 8;
  }
  return temperature - 8;
}

float decode_temperature_celsius(uint8_t encoded_temperature) { return encoded_temperature + 8.0f; }

void YKH531EClimate::transmit_state() {
  uint8_t ir_message[13] = {0};

  // byte 0: preamble
  ir_message[0] = 0b11000011;

  // byte1: temperature and vertical swing
  // byte2: 5 unknown bits and horizontal swing
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
      ir_message[1] |= SWING_ON;
      ir_message[2] |= SWING_ON << 5;
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      ir_message[1] |= SWING_ON;
      ir_message[2] |= SWING_OFF << 5;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      ir_message[1] |= SWING_OFF;
      ir_message[2] |= SWING_ON << 5;
      break;
    case climate::CLIMATE_SWING_OFF:
      ir_message[1] |= SWING_OFF;
      ir_message[2] |= SWING_OFF << 5;
      break;
  }

  ir_message[1] |= encode_temperature_celsius(this->target_temperature) << 3;

  // byte4: 5 unknown bits and fan speed
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      ir_message[4] |= FAN_SPEED_LOW << 5;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      ir_message[4] |= FAN_SPEED_MID << 5;
      break;
    case climate::CLIMATE_FAN_HIGH:
      ir_message[4] |= FAN_SPEED_HIGH << 5;
      break;
    case climate::CLIMATE_FAN_AUTO:
      ir_message[4] |= FAN_SPEED_AUTO << 5;
      break;
    default:
      break;
  }

  // byte6: 2 unknown bits, sleep, 2 unknown bits and mode
  ir_message[6] = this->preset == climate::CLIMATE_PRESET_SLEEP ? 1 << 2 : 0;

  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
    case climate::CLIMATE_MODE_HEAT_COOL:
      ir_message[6] |= MODE_AUTO << 5;
      break;
    case climate::CLIMATE_MODE_COOL:
      ir_message[6] |= MODE_COOL << 5;
      break;
    case climate::CLIMATE_MODE_DRY:
      ir_message[6] |= MODE_DRY << 5;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      ir_message[6] |= MODE_FAN << 5;
      break;
    default:
      break;
  }

  // byte9: 5 unknown bits, power and 2 unknown bits
  ir_message[9] = this->mode == climate::CLIMATE_MODE_OFF ? 0 : 1 << 5;

  // byte12: checksum
  uint8_t checksum = 0;
  for (int i = 0; i < 12; i++) {
    checksum += ir_message[i];
  }
  ir_message[12] = checksum;

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);  // 38 kHz PWM

  // header
  data->item(HEADER_MARK, HEADER_SPACE);

  // data
  for (uint8_t i : ir_message) {
    for (uint8_t j = 0; j < 8; j++) {
      bool bit = i & (1 << j);
      data->item(BIT_MARK, bit ? ONE_SPACE : ZERO_SPACE);
    }
  }

  // footer
  data->item(BIT_MARK, 0);

  transmit.perform();
}

bool YKH531EClimate::on_receive(remote_base::RemoteReceiveData data) {
  // validate header
  if (!data.expect_item(HEADER_MARK, HEADER_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  uint8_t ir_message[13] = {0};
  for (int i = 0; i < 13; i++) {
    // read all bits
    for (int j = 0; j < 8; j++) {
      if (data.expect_item(BIT_MARK, ONE_SPACE)) {
        ir_message[i] |= 1 << j;
      } else if (!data.expect_item(BIT_MARK, ZERO_SPACE)) {
        ESP_LOGV(TAG, "Byte %d bit %d fail", i, j);
        return false;
      }
    }
    ESP_LOGVV(TAG, "Byte %d %02X", i, ir_message[i]);
  }

  // validate footer
  if (!data.expect_mark(BIT_MARK)) {
    ESP_LOGV(TAG, "Footer fail");
    return false;
  }

  // validate checksum
  uint8_t checksum = 0;
  for (int i = 0; i < 12; i++) {
    checksum += ir_message[i];
  }
  if (ir_message[12] != checksum) {
    ESP_LOGV(TAG, "Checksum fail");
    return false;
  }

  bool power = (ir_message[9] & 0b11111011) >> 5;
  if (!power) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    uint8_t vertical_swing = ir_message[1] & 0b00000111;
    uint8_t horizontal_swing = (ir_message[2] & 0b11100000) >> 5;

    if (vertical_swing == SWING_ON && horizontal_swing == SWING_ON) {
      this->swing_mode = climate::CLIMATE_SWING_BOTH;
    } else if (vertical_swing == SWING_ON) {
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    } else if (horizontal_swing == SWING_ON) {
      this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
    } else {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }

    uint8_t temperature = decode_temperature_celsius((ir_message[1] & 0b11111000) >> 3);
    this->target_temperature = temperature;

    uint8_t fan_speed = (ir_message[4] & 0b11100000) >> 5;
    switch (fan_speed) {
      case FAN_SPEED_LOW:
        this->fan_mode = climate::CLIMATE_FAN_LOW;
        break;
      case FAN_SPEED_MID:
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        break;
      case FAN_SPEED_HIGH:
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        break;
      case FAN_SPEED_AUTO:
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        break;
    }

    this->preset = ir_message[6] & 0b00000100 ? climate::CLIMATE_PRESET_SLEEP : climate::CLIMATE_PRESET_NONE;

    uint8_t mode = (ir_message[6] & 0b11100000) >> 5;
    switch (mode) {
      case MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_AUTO;
        break;
      case MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  }

  this->publish_state();
  return true;
}

}  // namespace ykh531e
}  // namespace esphome
