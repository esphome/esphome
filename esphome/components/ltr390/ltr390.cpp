#include "ltr390.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <bitset>

namespace esphome {
namespace ltr390 {

static const char *const TAG = "ltr390";

static const float GAINVALUES[5] = {1.0, 3.0, 6.0, 9.0, 18.0};
static const float RESOLUTIONVALUE[6] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125};
static const uint32_t MODEADDRESSES[2] = {0x0D, 0x10};

uint32_t little_endian_bytes_to_int(const uint8_t *buffer, uint8_t num_bytes) {
  uint32_t value = 0;

  for (int i = 0; i < num_bytes; i++) {
    value <<= 8;
    value |= buffer[num_bytes - i - 1];
  }

  return value;
}

optional<uint32_t> LTR390Component::read_sensor_data_(LTR390MODE mode) {
  const uint8_t num_bytes = 3;
  uint8_t buffer[num_bytes];

  // Wait until data available
  const uint32_t now = millis();
  while (true) {
    std::bitset<8> status = this->reg(LTR390_MAIN_STATUS).get();
    bool available = status[3];
    if (available)
      break;

    if (millis() - now > 100) {
      ESP_LOGW(TAG, "Sensor didn't return any data, aborting");
      return {};
    }
    ESP_LOGD(TAG, "Waiting for data");
    delay(2);
  }

  if (!this->read_bytes(MODEADDRESSES[mode], buffer, num_bytes)) {
    ESP_LOGW(TAG, "Reading data from sensor failed!");
    return {};
  }

  return little_endian_bytes_to_int(buffer, num_bytes);
}

void LTR390Component::read_als_() {
  auto val = this->read_sensor_data_(LTR390_MODE_ALS);
  if (!val.has_value())
    return;
  uint32_t als = *val;

  if (this->light_sensor_ != nullptr) {
    float lux = (0.6 * als) / (GAINVALUES[this->gain_] * RESOLUTIONVALUE[this->res_]) * this->wfac_;
    this->light_sensor_->publish_state(lux);
  }

  if (this->als_sensor_ != nullptr) {
    this->als_sensor_->publish_state(als);
  }
}

void LTR390Component::read_uvs_() {
  auto val = this->read_sensor_data_(LTR390_MODE_UVS);
  if (!val.has_value())
    return;
  uint32_t uv = *val;

  if (this->uvi_sensor_ != nullptr) {
    this->uvi_sensor_->publish_state(uv / LTR390_SENSITIVITY * this->wfac_);
  }

  if (this->uv_sensor_ != nullptr) {
    this->uv_sensor_->publish_state(uv);
  }
}

void LTR390Component::read_mode_(int mode_index) {
  // Set mode
  LTR390MODE mode = std::get<0>(this->mode_funcs_[mode_index]);

  std::bitset<8> ctrl = this->reg(LTR390_MAIN_CTRL).get();
  ctrl[LTR390_CTRL_MODE] = mode;
  this->reg(LTR390_MAIN_CTRL) = ctrl.to_ulong();

  // After the sensor integration time do the following
  this->set_timeout(((uint32_t) RESOLUTIONVALUE[this->res_]) * 100, [this, mode_index]() {
    // Read from the sensor
    std::get<1>(this->mode_funcs_[mode_index])();

    // If there are more modes to read then begin the next
    // otherwise stop
    if (mode_index + 1 < (int) this->mode_funcs_.size()) {
      this->read_mode_(mode_index + 1);
    } else {
      this->reading_ = false;
    }
  });
}

void LTR390Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ltr390...");

  // reset
  std::bitset<8> ctrl = this->reg(LTR390_MAIN_CTRL).get();
  ctrl[LTR390_CTRL_RST] = true;
  this->reg(LTR390_MAIN_CTRL) = ctrl.to_ulong();
  delay(10);

  // Enable
  ctrl = this->reg(LTR390_MAIN_CTRL).get();
  ctrl[LTR390_CTRL_EN] = true;
  this->reg(LTR390_MAIN_CTRL) = ctrl.to_ulong();

  // check enabled
  ctrl = this->reg(LTR390_MAIN_CTRL).get();
  bool enabled = ctrl[LTR390_CTRL_EN];

  if (!enabled) {
    ESP_LOGW(TAG, "Sensor didn't respond with enabled state");
    this->mark_failed();
    return;
  }

  // Set gain
  this->reg(LTR390_GAIN) = gain_;

  // Set resolution
  uint8_t res = this->reg(LTR390_MEAS_RATE).get();
  // resolution is in bits 5-7
  res &= ~0b01110000;
  res |= res << 4;
  this->reg(LTR390_MEAS_RATE) = res;

  // Set sensor read state
  this->reading_ = false;

  // If we need the light sensor then add to the list
  if (this->light_sensor_ != nullptr || this->als_sensor_ != nullptr) {
    this->mode_funcs_.emplace_back(LTR390_MODE_ALS, std::bind(&LTR390Component::read_als_, this));
  }

  // If we need the UV sensor then add to the list
  if (this->uvi_sensor_ != nullptr || this->uv_sensor_ != nullptr) {
    this->mode_funcs_.emplace_back(LTR390_MODE_UVS, std::bind(&LTR390Component::read_uvs_, this));
  }
}

void LTR390Component::dump_config() { LOG_I2C_DEVICE(this); }

void LTR390Component::update() {
  if (!this->reading_ && !mode_funcs_.empty()) {
    this->reading_ = true;
    this->read_mode_(0);
  }
}

}  // namespace ltr390
}  // namespace esphome
