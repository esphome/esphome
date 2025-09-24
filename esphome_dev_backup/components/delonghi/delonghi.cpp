#include "delonghi.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace delonghi {

static const char *const TAG = "delonghi.climate";

void DelonghiClimate::transmit_state() {
  uint8_t remote_state[DELONGHI_STATE_FRAME_SIZE] = {0};
  remote_state[0] = DELONGHI_ADDRESS;
  remote_state[1] = this->temperature_();
  remote_state[1] |= (this->fan_speed_()) << 5;
  remote_state[2] = this->operation_mode_();
  // Calculate checksum
  for (int i = 0; i < DELONGHI_STATE_FRAME_SIZE - 1; i++) {
    remote_state[DELONGHI_STATE_FRAME_SIZE - 1] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(DELONGHI_IR_FREQUENCY);

  data->mark(DELONGHI_HEADER_MARK);
  data->space(DELONGHI_HEADER_SPACE);
  for (unsigned char b : remote_state) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DELONGHI_BIT_MARK);
      bool bit = b & mask;
      data->space(bit ? DELONGHI_ONE_SPACE : DELONGHI_ZERO_SPACE);
    }
  }
  data->mark(DELONGHI_BIT_MARK);
  data->space(0);

  transmit.perform();
}

uint8_t DelonghiClimate::operation_mode_() {
  uint8_t operating_mode = DELONGHI_MODE_ON;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= DELONGHI_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= DELONGHI_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= DELONGHI_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode |= DELONGHI_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= DELONGHI_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = DELONGHI_MODE_OFF;
      break;
  }
  return operating_mode;
}

uint16_t DelonghiClimate::fan_speed_() {
  uint16_t fan_speed;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed = DELONGHI_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = DELONGHI_FAN_MEDIUM;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = DELONGHI_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = DELONGHI_FAN_AUTO;
  }
  return fan_speed;
}

uint8_t DelonghiClimate::temperature_() {
  // Force special temperatures depending on the mode
  uint8_t temperature = 0b0001;
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:
      temperature = (uint8_t) roundf(this->target_temperature) - DELONGHI_TEMP_OFFSET_HEAT;
      break;
    case climate::CLIMATE_MODE_COOL:
    case climate::CLIMATE_MODE_DRY:
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_FAN_ONLY:
    case climate::CLIMATE_MODE_OFF:
    default:
      temperature = (uint8_t) roundf(this->target_temperature) - DELONGHI_TEMP_OFFSET_COOL;
  }
  if (temperature > 0x0F) {
    temperature = 0x0F;  // clamp maximum
  }
  return temperature;
}

bool DelonghiClimate::parse_state_frame_(const uint8_t frame[]) {
  uint8_t checksum = 0;
  for (int i = 0; i < (DELONGHI_STATE_FRAME_SIZE - 1); i++) {
    checksum += frame[i];
  }
  if (frame[DELONGHI_STATE_FRAME_SIZE - 1] != checksum) {
    return false;
  }
  uint8_t mode = frame[2] & 0x0F;
  if (mode & DELONGHI_MODE_ON) {
    switch (mode & 0x0E) {
      case DELONGHI_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case DELONGHI_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case DELONGHI_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case DELONGHI_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case DELONGHI_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }
  uint8_t temperature = frame[1] & 0x0F;
  if (this->mode == climate::CLIMATE_MODE_HEAT) {
    this->target_temperature = temperature + DELONGHI_TEMP_OFFSET_HEAT;
  } else {
    this->target_temperature = temperature + DELONGHI_TEMP_OFFSET_COOL;
  }
  uint8_t fan_mode = frame[1] >> 5;
  switch (fan_mode) {
    case DELONGHI_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case DELONGHI_FAN_MEDIUM:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case DELONGHI_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case DELONGHI_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }
  this->publish_state();
  return true;
}

bool DelonghiClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[DELONGHI_STATE_FRAME_SIZE] = {};
  if (!data.expect_item(DELONGHI_HEADER_MARK, DELONGHI_HEADER_SPACE)) {
    return false;
  }
  for (uint8_t pos = 0; pos < DELONGHI_STATE_FRAME_SIZE; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(DELONGHI_BIT_MARK, DELONGHI_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(DELONGHI_BIT_MARK, DELONGHI_ZERO_SPACE)) {
        return false;
      }
    }
    state_frame[pos] = byte;
    if (pos == 0) {
      // frame header
      if (byte != DELONGHI_ADDRESS) {
        return false;
      }
    }
  }
  return this->parse_state_frame_(state_frame);
}

}  // namespace delonghi
}  // namespace esphome
