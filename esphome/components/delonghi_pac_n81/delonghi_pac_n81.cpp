#include "delonghi_pac_n81.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace delonghi_pac_n81 {

static const char *const TAG = "delonghi_pac_n81.climate";

void DelonghiClimate::transmit_state() {
  uint8_t remote_state[DELONGHI_STATE_FRAME_SIZE] = {0};
  remote_state[0] = DELONGHI_ADDRESS;

  remote_state[1] = this->fan_speed_();
  remote_state[1] |= (this->operation_mode_()) << 4;

  remote_state[2] = this->power_();

  remote_state[3] = this->temperature_();

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
  uint8_t operating_mode = 0b0000;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode = DELONGHI_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode = DELONGHI_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode = DELONGHI_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_HEAT:
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = DELONGHI_MODE_DEFAULT;
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
      fan_speed = DELONGHI_FAN_MEDIUM;
  }
  return fan_speed;
}

uint8_t DelonghiClimate::temperature_() {
  uint8_t temperature = 0b0001;

  temperature = (uint8_t) roundf(this->target_temperature) - DELONGHI_TEMP_OFFSET_COOL;

  if (temperature > DELONGHI_TEMP_MAX - DELONGHI_TEMP_MIN) {
    temperature = DELONGHI_TEMP_MAX - DELONGHI_TEMP_MIN;  // clamp maximum
  }

  return temperature;
}

uint8_t DelonghiClimate::power_() {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    return 0x01;
  } else {
    return 0x82;
  }
}

}  // namespace delonghi_pac_n81
}  // namespace esphome
