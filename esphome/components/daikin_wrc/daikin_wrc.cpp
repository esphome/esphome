#include "daikin_wrc.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace daikin_wrc {

static const char *const TAG = "daikin_wrc.climate";

void DaikinWrcClimate::setup() {
  if (this->state_sensor_ != nullptr) {
    this->state_sensor_->add_on_state_callback([this](bool state) {
      if ((state && this->mode == climate::CLIMATE_MODE_OFF) || (!state && this->mode != climate::CLIMATE_MODE_OFF)) {
        this->mode = state ? (this->previous_mode_ != climate::CLIMATE_MODE_OFF ? this->previous_mode_
                                                                                : climate::CLIMATE_MODE_AUTO)
                           : climate::CLIMATE_MODE_OFF;
        this->previous_mode_ = this->mode;
        this->publish_state();
      }
    });
  }
  ClimateIR::setup();
}

void DaikinWrcClimate::control(const climate::ClimateCall &call) {
  this->previous_mode_ = this->mode;
  ClimateIR::control(call);
}

void DaikinWrcClimate::transmit_state() {
  uint8_t remote_state[16] = {0x6, 0x1, 0x2, 0x1, 0x0, 0x0, 0x0, 0x0, 0x2, 0x6, 0x2, 0x4, 0x2, 0x2, 0xc, 0x0};

  remote_state[2] = this->operation_mode_();
  remote_state[3] = this->fan_speed_();
  uint8_t temperature = this->temperature_();
  remote_state[12] = temperature & 0x0f;
  remote_state[13] = temperature >> 4;
  remote_state[14] = this->special_flags_();

  for (int i = 0; i < 15; i++) {
    remote_state[15] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(DAIKIN_WRC_IR_FREQUENCY);

  data->mark(DAIKIN_WRC_HEADER_MARK);
  data->space(DAIKIN_WRC_HEADER_SPACE);
  data->mark(DAIKIN_WRC_HEADER_MARK);
  data->space(DAIKIN_WRC_HEADER_SPACE);
  data->mark(DAIKIN_WRC_HDR_MSG_MARK);
  data->space(DAIKIN_WRC_HDR_MSG_SPACE);
  for (int i = 0; i < 16; i++) {
    for (uint8_t mask = 1; mask <= 8; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_WRC_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? DAIKIN_WRC_ONE_SPACE : DAIKIN_WRC_ZERO_SPACE);
    }
  }
  data->mark(DAIKIN_WRC_BIT_MARK);
  data->space(DAIKIN_WRC_MESSAGE_SPACE);
  data->mark(DAIKIN_WRC_HEADER_MARK);
  data->space(DAIKIN_WRC_END_SPACE);

  transmit.perform();
}

uint8_t DaikinWrcClimate::operation_mode_() const {
  uint8_t operating_mode = DAIKIN_WRC_MODE_AUTO;
  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
      operating_mode = DAIKIN_WRC_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_COOL:
      operating_mode = DAIKIN_WRC_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode = DAIKIN_WRC_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode = DAIKIN_WRC_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode = DAIKIN_WRC_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode = DAIKIN_WRC_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = DAIKIN_WRC_MODE_OFF;
      break;
  }

  return operating_mode;
}

uint8_t DaikinWrcClimate::fan_speed_() const {
  uint8_t fan_speed = DAIKIN_WRC_FAN_AUTO;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_QUIET:
      fan_speed = DAIKIN_WRC_FAN_SILENT;
      break;
    case climate::CLIMATE_FAN_LOW:
      fan_speed = DAIKIN_WRC_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = DAIKIN_WRC_FAN_MEDIUM;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = DAIKIN_WRC_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = DAIKIN_WRC_FAN_AUTO;
  }

  if (this->preset == climate::CLIMATE_PRESET_BOOST) {
    fan_speed = DAIKIN_WRC_FAN_TURBO;
  }

  return fan_speed;
}

uint8_t DaikinWrcClimate::special_flags_() const {
  uint8_t special_flags = 0x4;  // Unknown bit (can also be disabled, but is enabled by default)

  // Power ON/OFF
  if (this->previous_mode_ != this->mode &&
      (this->mode == climate::CLIMATE_MODE_OFF || this->previous_mode_ == climate::CLIMATE_MODE_OFF)) {
    special_flags |= 0x8;
  }

  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    special_flags |= 0x1;
  }

  if (this->preset == climate::CLIMATE_PRESET_SLEEP) {
    special_flags |= 0x2;
  }

  return special_flags;
}

uint8_t DaikinWrcClimate::temperature_() const {
  // Force special temperatures depending on the mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      return 0x24;
    case climate::CLIMATE_MODE_DRY:
      return 0x24;
    default:
      uint8_t temperature =
          (uint8_t) roundf(clamp<float>(this->target_temperature, DAIKIN_WRC_TEMP_MIN, DAIKIN_WRC_TEMP_MAX));
      return ((temperature / 10) << 4) | (temperature % 10);
  }
}

bool DaikinWrcClimate::parse_state_frame_(const uint8_t frame[]) {
  uint8_t checksum = 0;
  for (int i = 0; i < (DAIKIN_WRC_STATE_FRAME_SIZE - 1); i++) {
    checksum += frame[i];
  }
  checksum &= 0x0f;
  if (frame[DAIKIN_WRC_STATE_FRAME_SIZE - 1] != checksum) {
    ESP_LOGE(TAG, "Checksum doesn't match");
    return false;
  }
  uint8_t mode = frame[2];
  bool power = frame[14] & 0x8;
  // Temperature is given in degrees celcius * 2
  // only update for states that use the temperature
  uint8_t temperature = (frame[13] * 10) + frame[12];
  if (((this->mode == climate::CLIMATE_MODE_OFF && power) || (this->mode != climate::CLIMATE_MODE_OFF && !power))) {
    switch (mode) {
      case DAIKIN_WRC_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        this->target_temperature = static_cast<float>(temperature);
        break;
      case DAIKIN_WRC_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case DAIKIN_WRC_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        this->target_temperature = static_cast<float>(temperature);
        break;
      case DAIKIN_WRC_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        this->target_temperature = static_cast<float>(temperature);
        break;
      case DAIKIN_WRC_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  this->preset = climate::CLIMATE_PRESET_NONE;

  uint8_t fan_mode = frame[3];
  bool swing_mode = frame[14] & 0x1;
  if (swing_mode) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }
  switch (fan_mode) {
    case DAIKIN_WRC_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case DAIKIN_WRC_FAN_MEDIUM:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case DAIKIN_WRC_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case DAIKIN_WRC_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case DAIKIN_WRC_FAN_SILENT:
      this->fan_mode = climate::CLIMATE_FAN_QUIET;
      break;
    case DAIKIN_WRC_FAN_TURBO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      this->preset = climate::CLIMATE_PRESET_BOOST;
      break;
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
  }

  bool sleep_mode = frame[14] & 0x2;
  if (sleep_mode) {
    this->preset = climate::CLIMATE_PRESET_SLEEP;
  }

  this->publish_state();
  return true;
}

bool DaikinWrcClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[DAIKIN_WRC_STATE_FRAME_SIZE] = {};
  if (!data.expect_item(DAIKIN_WRC_HEADER_MARK, DAIKIN_WRC_HEADER_SPACE) ||
      !data.expect_item(DAIKIN_WRC_HEADER_MARK, DAIKIN_WRC_HEADER_SPACE) ||
      !data.expect_item(DAIKIN_WRC_HDR_MSG_MARK, DAIKIN_WRC_HDR_MSG_SPACE)) {
    return false;
  }
  for (uint8_t pos = 0; pos < DAIKIN_WRC_STATE_FRAME_SIZE; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 4; bit++) {
      if (data.expect_item(DAIKIN_WRC_BIT_MARK, DAIKIN_WRC_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(DAIKIN_WRC_BIT_MARK, DAIKIN_WRC_ZERO_SPACE)) {
        return false;
      }
    }
    state_frame[pos] = byte;
    if (pos == 0) {
      // frame header
      if (byte != 0x6)
        return false;
    } else if (pos == 1) {
      // frame header
      if (byte != 0x1)
        return false;
    }
  }
  return this->parse_state_frame_(state_frame);
}

}  // namespace daikin_wrc
}  // namespace esphome
