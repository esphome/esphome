#include "daikin_brc52a61.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace daikin_brc52a61 {

static const char *const TAG = "daikin_brc52a61.climate";

uint8_t uint2bcd(uint8_t dec) { return ((dec / 10) << 4) + (dec % 10); }

uint8_t bcd2uint(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0xF); }

struct IRData {
  union {
    uint8_t bytes[8];

    struct {
      uint8_t magic : 8;

      uint8_t mode : 4;
      uint8_t fan_speed : 4;

      uint8_t mins : 8;
      uint8_t hours : 8;

      uint8_t on_hours : 6;
      uint8_t on_30mins : 1;
      uint8_t on_timer : 1;

      uint8_t off_hours : 6;
      uint8_t off_30mins : 1;
      uint8_t off_timer : 1;

      uint8_t temperature : 8;

      uint8_t swing : 1;
      uint8_t sleep : 1;
      uint8_t unused : 1;
      uint8_t power : 1;
      uint8_t sum : 4;
    };
  };

  void set_power(bool on) {
    // remote only supports toggle power on/off
    // since we cannot reliably know the current state, we cannot reliably
    // turn it on or off

    // workaround by abusing timer on/off to set it to a known state

    this->on_timer = 0;
    this->off_timer = 0;

    this->hours = uint2bcd(00);
    this->mins = uint2bcd(00);

    if (on) {
      this->on_hours = 0;
      this->on_30mins = 0;
      this->on_timer = 1;
    } else {
      this->off_hours = 0;
      this->off_30mins = 0;
      this->off_timer = 1;
    }
  }

  void set_checksum() {
    this->sum = 0;

    uint8_t sum = 0;
    for (auto &byte : this->bytes) {
      sum += byte & 0xF;
      sum += byte >> 4;
    }
    sum &= 0xF;

    this->sum = sum;
  }
};

void DaikinClimate::transmit_state() {
  IRData irdata{{{0x16}}};
  irdata.mode = this->operation_mode_();
  irdata.fan_speed = this->fan_speed_();
  irdata.temperature = this->temperature_();
  irdata.swing = this->swing_();
  irdata.set_power(this->mode != climate::CLIMATE_MODE_OFF);
  irdata.set_checksum();

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(DAIKIN_IR_FREQUENCY);

  data->mark(DAIKIN_PRE_MARK);
  data->space(DAIKIN_PRE_SPACE);
  data->mark(DAIKIN_PRE_MARK);
  data->space(DAIKIN_PRE_SPACE);

  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);
  for (auto byte : irdata.bytes) {
    for (int8_t i = 0; i < 8; i++) {  // iterate through bit mask
      data->mark(DAIKIN_BIT_MARK);
      data->space(byte & 1 ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
      byte >>= 1;
    }
  }
  data->mark(DAIKIN_BIT_MARK);
  data->space(DAIKIN_FOOTER_SPACE);
  data->mark(DAIKIN_FOOTER_MARK);

  transmit.perform();
}

uint8_t DaikinClimate::operation_mode_() {
  uint8_t operating_mode = DAIKIN_MODE_AUTO;
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode = DAIKIN_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_COOL:
      operating_mode = DAIKIN_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode = DAIKIN_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode = DAIKIN_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode = DAIKIN_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = DAIKIN_MODE_AUTO;
      break;
  }
  return operating_mode;
}

uint8_t DaikinClimate::fan_speed_() {
  uint8_t fan_speed = DAIKIN_FAN_AUTO;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_QUIET:
      fan_speed = DAIKIN_FAN_QUIET;
      break;
    case climate::CLIMATE_FAN_LOW:
      fan_speed = DAIKIN_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = DAIKIN_FAN_MED;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = DAIKIN_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = DAIKIN_FAN_AUTO;
  }
  return fan_speed;
}

uint8_t DaikinClimate::temperature_() {
  uint8_t temperature = (uint8_t) roundf(clamp<float>(this->target_temperature, DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX));
  return uint2bcd(temperature);
}

uint8_t DaikinClimate::swing_() {
  uint8_t swing_mode = 1;
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      swing_mode = 1;
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      swing_mode = 0;
      break;
  }
  return swing_mode;
}

bool DaikinClimate::parse_state_frame_(IRData &irdata) {
  const uint8_t sum = irdata.sum;
  irdata.set_checksum();
  if (sum != irdata.sum)
    return false;

  switch (irdata.mode) {
    case DAIKIN_MODE_AUTO:
      this->mode = climate::CLIMATE_MODE_HEAT_COOL;
      break;
    case DAIKIN_MODE_COOL:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
    case DAIKIN_MODE_HEAT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      break;
    case DAIKIN_MODE_FAN:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case DAIKIN_MODE_DRY:
      this->mode = climate::CLIMATE_MODE_DRY;
      break;
    default:
      this->mode = climate::CLIMATE_MODE_OFF;
      break;
  }

  this->target_temperature = bcd2uint(irdata.temperature);

  switch (irdata.fan_speed) {
    case DAIKIN_FAN_QUIET:
      this->fan_mode = climate::CLIMATE_FAN_QUIET;
      break;
    case DAIKIN_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case DAIKIN_FAN_MED:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case DAIKIN_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case DAIKIN_FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  if (irdata.swing) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  this->publish_state();
  return true;
}

bool DaikinClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!(data.expect_item(DAIKIN_PRE_MARK, DAIKIN_PRE_SPACE) && data.expect_item(DAIKIN_PRE_MARK, DAIKIN_PRE_SPACE))) {
    return false;
  }

  if (!(data.expect_item(DAIKIN_HEADER_MARK, DAIKIN_HEADER_SPACE))) {
    return false;
  }

  IRData irdata{{{0}}};
  for (auto &byte : irdata.bytes) {
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ZERO_SPACE)) {
        byte &= ~(1 << bit);
      } else {
        return false;
      }
    }
  }

  // frame header
  if (irdata.bytes[0] != 0x16)
    return false;

  return this->parse_state_frame_(irdata);
}

}  // namespace daikin_brc52a61
}  // namespace esphome
