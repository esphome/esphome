#include "daikin_brc.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace daikin_brc {

static const char *const TAG = "daikin_brc.climate";

void DaikinBrcClimate::control(const climate::ClimateCall &call) {
  this->mode_button_ = 0x00;
  if (call.get_mode().has_value()) {
    // Need to determine if this is call due to Mode button pressed so that we can set the Mode button byte
    this->mode_button_ = DAIKIN_BRC_IR_MODE_BUTTON;
  }
  ClimateIR::control(call);
}

void DaikinBrcClimate::transmit_state() {
  uint8_t remote_state[DAIKIN_BRC_TRANSMIT_FRAME_SIZE] = {0x11, 0xDA, 0x17, 0x18, 0x04, 0x00, 0x1E, 0x11,
                                                          0xDA, 0x17, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                          0x00, 0x00, 0x00, 0x00, 0x20, 0x00};

  remote_state[12] = this->alt_mode_();
  remote_state[13] = this->mode_button_;
  remote_state[14] = this->operation_mode_();
  remote_state[17] = this->temperature_();
  remote_state[18] = this->fan_speed_swing_();

  // Calculate checksum
  for (int i = DAIKIN_BRC_PREAMBLE_SIZE; i < DAIKIN_BRC_TRANSMIT_FRAME_SIZE - 1; i++) {
    remote_state[DAIKIN_BRC_TRANSMIT_FRAME_SIZE - 1] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(DAIKIN_BRC_IR_FREQUENCY);

  data->mark(DAIKIN_BRC_HEADER_MARK);
  data->space(DAIKIN_BRC_HEADER_SPACE);
  for (int i = 0; i < DAIKIN_BRC_PREAMBLE_SIZE; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_BRC_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? DAIKIN_BRC_ONE_SPACE : DAIKIN_BRC_ZERO_SPACE);
    }
  }
  data->mark(DAIKIN_BRC_BIT_MARK);
  data->space(DAIKIN_BRC_MESSAGE_SPACE);
  data->mark(DAIKIN_BRC_HEADER_MARK);
  data->space(DAIKIN_BRC_HEADER_SPACE);

  for (int i = DAIKIN_BRC_PREAMBLE_SIZE; i < DAIKIN_BRC_TRANSMIT_FRAME_SIZE; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_BRC_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? DAIKIN_BRC_ONE_SPACE : DAIKIN_BRC_ZERO_SPACE);
    }
  }

  data->mark(DAIKIN_BRC_BIT_MARK);
  data->space(0);

  transmit.perform();
}

uint8_t DaikinBrcClimate::alt_mode_() {
  uint8_t alt_mode = 0x00;
  switch (this->mode) {
    case climate::CLIMATE_MODE_DRY:
      alt_mode = 0x23;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      alt_mode = 0x63;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_COOL:
    case climate::CLIMATE_MODE_HEAT:
    default:
      alt_mode = 0x73;
      break;
  }
  return alt_mode;
}

uint8_t DaikinBrcClimate::operation_mode_() {
  uint8_t operating_mode = DAIKIN_BRC_MODE_ON;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= DAIKIN_BRC_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= DAIKIN_BRC_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= DAIKIN_BRC_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode |= DAIKIN_BRC_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= DAIKIN_BRC_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = DAIKIN_BRC_MODE_OFF;
      break;
  }

  return operating_mode;
}

uint8_t DaikinBrcClimate::fan_speed_swing_() {
  uint16_t fan_speed;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed = DAIKIN_BRC_FAN_1;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = DAIKIN_BRC_FAN_2;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = DAIKIN_BRC_FAN_3;
      break;
    default:
      fan_speed = DAIKIN_BRC_FAN_1;
  }

  // If swing is enabled switch first 4 bits to 1111
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
      fan_speed |= DAIKIN_BRC_IR_SWING_ON;
      break;
    default:
      fan_speed |= DAIKIN_BRC_IR_SWING_OFF;
      break;
  }
  return fan_speed;
}

uint8_t DaikinBrcClimate::temperature_() {
  // Force special temperatures depending on the mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
    case climate::CLIMATE_MODE_DRY:
      if (this->fahrenheit_) {
        return DAIKIN_BRC_IR_DRY_FAN_TEMP_F;
      }
      return DAIKIN_BRC_IR_DRY_FAN_TEMP_C;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      uint8_t temperature;
      // Temperature in remote is in F
      if (this->fahrenheit_) {
        temperature = (uint8_t) roundf(
            clamp<float>(((this->target_temperature * 1.8) + 32), DAIKIN_BRC_TEMP_MIN_F, DAIKIN_BRC_TEMP_MAX_F));
      } else {
        temperature = ((uint8_t) roundf(this->target_temperature) - 9) << 1;
      }
      return temperature;
  }
}

bool DaikinBrcClimate::parse_state_frame_(const uint8_t frame[]) {
  uint8_t checksum = 0;
  for (int i = 0; i < (DAIKIN_BRC_STATE_FRAME_SIZE - 1); i++) {
    checksum += frame[i];
  }
  if (frame[DAIKIN_BRC_STATE_FRAME_SIZE - 1] != checksum) {
    ESP_LOGCONFIG(TAG, "Bad CheckSum %x", checksum);
    return false;
  }

  uint8_t mode = frame[7];
  if (mode & DAIKIN_BRC_MODE_ON) {
    switch (mode & 0xF0) {
      case DAIKIN_BRC_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case DAIKIN_BRC_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case DAIKIN_BRC_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case DAIKIN_BRC_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case DAIKIN_BRC_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  uint8_t temperature = frame[10];
  float temperature_c;
  if (this->fahrenheit_) {
    temperature_c = clamp<float>(((temperature - 32) / 1.8), DAIKIN_BRC_TEMP_MIN_C, DAIKIN_BRC_TEMP_MAX_C);
  } else {
    temperature_c = (temperature >> 1) + 9;
  }

  this->target_temperature = temperature_c;

  uint8_t fan_mode = frame[11];
  uint8_t swing_mode = frame[11];
  switch (swing_mode & 0xF) {
    case DAIKIN_BRC_IR_SWING_ON:
      this->swing_mode = climate::CLIMATE_SWING_BOTH;
      break;
    case DAIKIN_BRC_IR_SWING_OFF:
      this->swing_mode = climate::CLIMATE_SWING_OFF;
      break;
  }

  switch (fan_mode & 0xF0) {
    case DAIKIN_BRC_FAN_1:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case DAIKIN_BRC_FAN_2:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case DAIKIN_BRC_FAN_3:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
  }
  this->publish_state();
  return true;
}

bool DaikinBrcClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[DAIKIN_BRC_STATE_FRAME_SIZE] = {};
  if (!data.expect_item(DAIKIN_BRC_HEADER_MARK, DAIKIN_BRC_HEADER_SPACE)) {
    return false;
  }
  for (uint8_t pos = 0; pos < DAIKIN_BRC_STATE_FRAME_SIZE; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(DAIKIN_BRC_BIT_MARK, DAIKIN_BRC_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(DAIKIN_BRC_BIT_MARK, DAIKIN_BRC_ZERO_SPACE)) {
        return false;
      }
    }
    state_frame[pos] = byte;
    if (pos == 0) {
      // frame header
      if (byte != 0x11)
        return false;
    } else if (pos == 1) {
      // frame header
      if (byte != 0xDA)
        return false;
    } else if (pos == 2) {
      // frame header
      if (byte != 0x17)
        return false;
    } else if (pos == 3) {
      // frame header
      if (byte != 0x18)
        return false;
    } else if (pos == 4) {
      // frame type
      if (byte != 0x00)
        return false;
    }
  }
  return this->parse_state_frame_(state_frame);
}

}  // namespace daikin_brc
}  // namespace esphome
