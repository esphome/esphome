#include "daikin.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace daikin {

static const char *TAG = "daikin.climate";
// TODO: Make these inputs
static bool ARC432A14 = true;
static bool comfort_mode = false;
static bool powerful_mode = false;
static bool economy_mode = false;
static const uint8_t FRAME_THREE_OFFSET = 16;
static const uint8_t HEADER_ID = 3;
static const uint8_t MESSAGE_ID = 4;
static const uint8_t COMFORT_ID = 6;
static const uint8_t SWING_VERTICAL_ID = (FRAME_THREE_OFFSET + 8);
static const uint8_t SWING_HORIZONTAL_ID = (FRAME_THREE_OFFSET + 9);
static const uint8_t TIMER_A_ID = (FRAME_THREE_OFFSET + 10);
static const uint8_t TIMER_B_ID = (FRAME_THREE_OFFSET + 11);
static const uint8_t TIMER_C_ID = (FRAME_THREE_OFFSET + 12);
static const uint8_t POWERFUL_ID = (FRAME_THREE_OFFSET + 13);
static const uint8_t ECONOMY_ID = (FRAME_THREE_OFFSET + 16);

void DaikinClimate::transmit_state() {
  uint8_t remote_state[35] = {0x11, 0xDA, 0x27, 0x00, 0xC5, 0x00, 0x00, 0x00, 0x11, 0xDA, 0x27, 0x00,
                              0x42, 0x49, 0x05, 0xA2, 0x11, 0xDA, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00};
  // Original author's setting. Untested.
  if (!ARC432A14) {
    remote_state[27] = 0x06;
    remote_state[28] = 0x60;
  }

  // Frame #1
  if (ARC432A14) {
    remote_state[HEADER_ID] = 0xF0;
    remote_state[MESSAGE_ID] = 0x00;
  }
  if (comfort_mode) {
    remote_state[COMFORT_ID] = 0x10;
  }
  // Calculate checksum
  for (int i = 0; i < 7; i++) {
    remote_state[7] += remote_state[i];
  }

  // Calculations frame #3
  if (this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL) {
    remote_state[SWING_HORIZONTAL_ID] |= 0xF0;
  }
  // FUTURE:
#if 0
  if (this->swing_mode == climate::CLIMATE_SWING_WIDE)
  {
     remote_state[SWING_HORIZONTAL_ID] &= 0x0F;
     remote_state[SWING_HORIZONTAL_ID] |= 0xE0;
  }
#endif

  remote_state[21] = this->operation_mode_();
  remote_state[22] = this->temperature_();
  // Vertical swing covered in fan speed.
  uint16_t fan_speed = this->fan_speed_();
  remote_state[24] = fan_speed >> 8;
  remote_state[25] = fan_speed & 0xff;
  if (powerful_mode) {
    remote_state[POWERFUL_ID] = 0x01;
  }
  if (economy_mode) {
    remote_state[ECONOMY_ID] |= 0x04;
  }

  // Calculate checksum
  for (int i = 16; i < 34; i++) {
    remote_state[34] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();
  data->set_carrier_frequency(DAIKIN_IR_FREQUENCY);

  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);
  for (int i = 0; i < 8; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
    }
  }
  data->mark(DAIKIN_BIT_MARK);
  data->space(DAIKIN_MESSAGE_SPACE);
  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);

  // Second frame not necessary for ARC432A14
  if (!ARC432A14) {
    for (int i = 8; i < 16; i++) {
      for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
        data->mark(DAIKIN_BIT_MARK);
        bool bit = remote_state[i] & mask;
        data->space(bit ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
      }
    }
    data->mark(DAIKIN_BIT_MARK);
    data->space(DAIKIN_MESSAGE_SPACE);
    data->mark(DAIKIN_HEADER_MARK);
    data->space(DAIKIN_HEADER_SPACE);
  }

  for (int i = 16; i < 35; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
    }
  }
  data->mark(DAIKIN_BIT_MARK);
  data->space(0);

  transmit.perform();
}

uint8_t DaikinClimate::operation_mode_() {
  uint8_t operating_mode = DAIKIN_MODE_ON;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= DAIKIN_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= DAIKIN_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= DAIKIN_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_AUTO:
      operating_mode |= DAIKIN_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= DAIKIN_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = DAIKIN_MODE_OFF;
      break;
  }

  return operating_mode;
}

uint16_t DaikinClimate::fan_speed_() {
  uint16_t fan_speed;
  switch (this->fan_mode) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed = DAIKIN_FAN_1 << 8;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = DAIKIN_FAN_3 << 8;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = DAIKIN_FAN_5 << 8;
      break;
    case climate::CLIMATE_FAN_FOCUS:
      fan_speed = DAIKIN_FAN_SILENT;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = DAIKIN_FAN_AUTO << 8;
  }

  // If swing is enabled switch first 4 bits to 1111
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      fan_speed |= 0x0F00;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      fan_speed |= 0x000F;
      break;
    case climate::CLIMATE_SWING_BOTH:
      fan_speed |= 0x0F0F;
      break;
    default:
      break;
  }
  return fan_speed;
}

uint8_t DaikinClimate::temperature_() {
  // Force special temperatures depending on the mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      return 0x32;
    case climate::CLIMATE_MODE_AUTO:
    case climate::CLIMATE_MODE_DRY:
      return 0xc0;
    default:
      uint8_t temperature = (uint8_t) roundf(clamp(this->target_temperature, DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX));
      return temperature << 1;
  }
}

bool DaikinClimate::parse_state_frame_(const uint8_t frame[]) {
  uint8_t checksum = 0;
  for (int i = 0; i < (DAIKIN_STATE_FRAME_SIZE - 1); i++) {
    checksum += frame[i];
  }
  if (frame[DAIKIN_STATE_FRAME_SIZE - 1] != checksum)
    return false;
  uint8_t mode = frame[5];
  if (mode & DAIKIN_MODE_ON) {
    switch (mode & 0xF0) {
      case DAIKIN_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case DAIKIN_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case DAIKIN_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case DAIKIN_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_AUTO;
        break;
      case DAIKIN_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }
  uint8_t temperature = frame[6];
  if (!(temperature & 0xC0)) {
    this->target_temperature = temperature >> 1;
  }
  uint8_t fan_mode = frame[8];
  uint8_t swing_mode = frame[9];
  if (fan_mode & 0xF && swing_mode & 0xF)
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  else if (fan_mode & 0xF)
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  else if (swing_mode & 0xF)
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  else
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  switch (fan_mode & 0xF0) {
    case DAIKIN_FAN_1:
    case DAIKIN_FAN_2:
    case DAIKIN_FAN_SILENT:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case DAIKIN_FAN_3:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case DAIKIN_FAN_4:
    case DAIKIN_FAN_5:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case DAIKIN_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }
  this->publish_state();
  return true;
}

bool DaikinClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[DAIKIN_STATE_FRAME_SIZE] = {};
  if (!data.expect_item(DAIKIN_HEADER_MARK, DAIKIN_HEADER_SPACE)) {
    return false;
  }
  for (uint8_t pos = 0; pos < DAIKIN_STATE_FRAME_SIZE; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ONE_SPACE))
        byte |= 1 << bit;
      else if (!data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ZERO_SPACE)) {
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
      if (byte != 0x27)
        return false;
    } else if (pos == 3) {
      // frame header
      if (byte != 0x00)
        return false;
    } else if (pos == 4) {
      // frame type
      if (byte != 0x00)
        return false;
    }
  }
  return this->parse_state_frame_(state_frame);
}

}  // namespace daikin
}  // namespace esphome
