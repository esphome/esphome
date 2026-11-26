#include "emmeti.h"
#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::emmeti {

ESPHOME_LOG_TAG(TAG, "emmeti.climate");

using climate_ir::from_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_HEAT_COOL, EMMETI_MODE_HEAT_COOL},
    {climate::CLIMATE_MODE_COOL, EMMETI_MODE_COOL},
    {climate::CLIMATE_MODE_DRY, EMMETI_MODE_DRY},
    {climate::CLIMATE_MODE_FAN_ONLY, EMMETI_MODE_FAN},
    {climate::CLIMATE_MODE_HEAT, EMMETI_MODE_HEAT},
};
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, EMMETI_FAN_AUTO},
    {climate::CLIMATE_FAN_LOW, EMMETI_FAN_1},
    {climate::CLIMATE_FAN_MEDIUM, EMMETI_FAN_2},
    {climate::CLIMATE_FAN_HIGH, EMMETI_FAN_3},
};

// setters
uint8_t EmmetiClimate::set_temp_() {
  return (uint8_t) roundf(clamp<float>(this->target_temperature, EMMETI_TEMP_MIN, EMMETI_TEMP_MAX) - EMMETI_TEMP_MIN);
}

uint8_t EmmetiClimate::set_mode_() { return to_ir_code(MODES, this->mode, EMMETI_MODE_HEAT_COOL); }

uint8_t EmmetiClimate::set_blades_() {
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    switch (this->blades_) {
      case EMMETI_BLADES_1:
      case EMMETI_BLADES_2:
      case EMMETI_BLADES_HIGH:
        this->blades_ = EMMETI_BLADES_HIGH;
        break;
      case EMMETI_BLADES_3:
      case EMMETI_BLADES_MID:
        this->blades_ = EMMETI_BLADES_MID;
        break;
      case EMMETI_BLADES_4:
      case EMMETI_BLADES_5:
      case EMMETI_BLADES_LOW:
        this->blades_ = EMMETI_BLADES_LOW;
        break;
      default:
        this->blades_ = EMMETI_BLADES_FULL;
        break;
    }
  } else {
    switch (this->blades_) {
      case EMMETI_BLADES_1:
      case EMMETI_BLADES_2:
      case EMMETI_BLADES_HIGH:
        this->blades_ = EMMETI_BLADES_1;
        break;
      case EMMETI_BLADES_3:
      case EMMETI_BLADES_MID:
        this->blades_ = EMMETI_BLADES_3;
        break;
      case EMMETI_BLADES_4:
      case EMMETI_BLADES_5:
      case EMMETI_BLADES_LOW:
        this->blades_ = EMMETI_BLADES_5;
        break;
      default:
        this->blades_ = EMMETI_BLADES_STOP;
        break;
    }
  }
  return this->blades_;
}

uint8_t EmmetiClimate::gen_checksum_() { return (this->set_temp_() + this->set_mode_() + 2) % 16; }

// getters
float EmmetiClimate::get_temp_(uint8_t temp) { return (float) (temp + EMMETI_TEMP_MIN); }

climate::ClimateSwingMode EmmetiClimate::get_swing_(uint8_t bitmap) {
  return (bitmap >> 1) & 0x01 ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
}

template<typename T> T EmmetiClimate::reverse_(T val, size_t len) {
  T result = 0;
  for (size_t i = 0; i < len; i++) {
    result |= ((val & 1 << i) != 0) << (len - 1 - i);
  }
  return result;
}

template<typename T> void EmmetiClimate::add_(T val, size_t len, esphome::remote_base::RemoteTransmitData *data) {
  for (size_t i = len; i > 0; i--) {
    data->mark(EMMETI_BIT_MARK);
    data->space((val & (1 << (i - 1))) ? EMMETI_ONE_SPACE : EMMETI_ZERO_SPACE);
  }
}

template<typename T> void EmmetiClimate::add_(T val, esphome::remote_base::RemoteTransmitData *data) {
  data->mark(EMMETI_BIT_MARK);
  data->space((val & 1) ? EMMETI_ONE_SPACE : EMMETI_ZERO_SPACE);
}

template<typename T>
void EmmetiClimate::reverse_add_(T val, size_t len, esphome::remote_base::RemoteTransmitData *data) {
  this->add_(this->reverse_(val, len), len, data);
}

bool EmmetiClimate::check_checksum_(uint8_t checksum) {
  uint8_t expected = this->gen_checksum_();
  ESP_LOGV(TAG, "Expected checksum: %X, Checksum received: %X", expected, checksum);

  return checksum == expected;
}

void EmmetiClimate::transmit_state() {
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(EMMETI_IR_FREQUENCY);

  data->mark(EMMETI_HEADER_MARK);
  data->space(EMMETI_HEADER_SPACE);

  if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->reverse_add_(this->set_mode_(), 3, data);
    this->add_(1, data);
    this->reverse_add_(to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO), EMMETI_FAN_AUTO), 2, data);
    this->add_(this->swing_mode != climate::CLIMATE_SWING_OFF, data);
    this->add_(0, data);  // sleep mode
    this->reverse_add_(this->set_temp_(), 4, data);
    this->add_(0, 8, data);      // zeros
    this->add_(0, data);         // turbo mode
    this->add_(1, data);         // light
    this->add_(1, data);         // tree icon thingy
    this->add_(0, data);         // blow mode
    this->add_(0x52, 11, data);  // idk

    data->mark(EMMETI_BIT_MARK);
    data->space(EMMETI_MESSAGE_SPACE);

    this->reverse_add_(this->set_blades_(), 4, data);
    this->add_(0, 4, data);          // zeros
    this->reverse_add_(2, 2, data);  // thermometer
    this->add_(0, 18, data);         // zeros
    this->reverse_add_(this->gen_checksum_(), 4, data);
  } else {
    this->add_(9, 12, data);
    this->add_(0, 8, data);
    this->add_(0x2052, 15, data);
    data->mark(EMMETI_BIT_MARK);
    data->space(EMMETI_MESSAGE_SPACE);
    this->add_(0, 8, data);
    this->add_(1, 2, data);
    this->add_(0, 18, data);
    this->add_(0x0C, 4, data);
  }
  data->mark(EMMETI_BIT_MARK);
  data->space(0);

  transmit.perform();
}

bool EmmetiClimate::parse_state_frame_(EmmetiState curr_state) {
  this->mode = from_ir_code(MODES, curr_state.mode, climate::CLIMATE_MODE_HEAT_COOL);
  this->fan_mode = from_ir_code(FANS, curr_state.fan_speed, climate::CLIMATE_FAN_AUTO);
  this->target_temperature = this->get_temp_(curr_state.temp);
  this->swing_mode = this->get_swing_(curr_state.bitmap);
  // this->blades_ = curr_state.fan_pos;
  if (!(curr_state.bitmap & 0x01)) {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  this->publish_state();
  return true;
}

bool EmmetiClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(EMMETI_HEADER_MARK, EMMETI_HEADER_SPACE)) {
    return false;
  }
  ESP_LOGD(TAG, "Received emmeti frame");

  EmmetiState curr_state;

  for (size_t pos = 0; pos < 3; pos++) {
    if (data.expect_item(EMMETI_BIT_MARK, EMMETI_ONE_SPACE)) {
      curr_state.mode |= 1 << pos;
    } else if (!data.expect_item(EMMETI_BIT_MARK, EMMETI_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Mode: %d", curr_state.mode);

  if (data.expect_item(EMMETI_BIT_MARK, EMMETI_ONE_SPACE)) {
    curr_state.bitmap |= 1 << 0;
  } else if (!data.expect_item(EMMETI_BIT_MARK, EMMETI_ZERO_SPACE)) {
    return false;
  }

  ESP_LOGD(TAG, "On: %d", curr_state.bitmap & 0x01);

  for (size_t pos = 0; pos < 2; pos++) {
    if (data.expect_item(EMMETI_BIT_MARK, EMMETI_ONE_SPACE)) {
      curr_state.fan_speed |= 1 << pos;
    } else if (!data.expect_item(EMMETI_BIT_MARK, EMMETI_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Fan speed: %d", curr_state.fan_speed);

  for (size_t pos = 0; pos < 2; pos++) {
    if (data.expect_item(EMMETI_BIT_MARK, EMMETI_ONE_SPACE)) {
      curr_state.bitmap |= 1 << (pos + 1);
    } else if (!data.expect_item(EMMETI_BIT_MARK, EMMETI_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Swing: %d, Sleep: %d", (curr_state.bitmap >> 1) & 0x01, (curr_state.bitmap >> 2) & 0x01);

  for (size_t pos = 0; pos < 4; pos++) {
    if (data.expect_item(EMMETI_BIT_MARK, EMMETI_ONE_SPACE)) {
      curr_state.temp |= 1 << pos;
    } else if (!data.expect_item(EMMETI_BIT_MARK, EMMETI_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Temp: %d", curr_state.temp);

  for (size_t pos = 0; pos < 8; pos++) {
    if (!data.expect_item(EMMETI_BIT_MARK, EMMETI_ZERO_SPACE)) {
      return false;
    }
  }

  for (size_t pos = 0; pos < 4; pos++) {
    if (data.expect_item(EMMETI_BIT_MARK, EMMETI_ONE_SPACE)) {
      curr_state.bitmap |= 1 << (pos + 3);
    } else if (!data.expect_item(EMMETI_BIT_MARK, EMMETI_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Turbo: %d, Light: %d, Tree: %d, Blow: %d", (curr_state.bitmap >> 3) & 0x01,
           (curr_state.bitmap >> 4) & 0x01, (curr_state.bitmap >> 5) & 0x01, (curr_state.bitmap >> 6) & 0x01);

  uint16_t control_data = 0;
  for (size_t pos = 0; pos < 11; pos++) {
    if (data.expect_item(EMMETI_BIT_MARK, EMMETI_ONE_SPACE)) {
      control_data |= 1 << pos;
    } else if (!data.expect_item(EMMETI_BIT_MARK, EMMETI_ZERO_SPACE)) {
      return false;
    }
  }

  if (control_data != 0x250) {
    return false;
  }

  return this->parse_state_frame_(curr_state);
}

}  // namespace esphome::emmeti
