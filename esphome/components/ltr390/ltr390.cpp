#include "ltr390.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <bitset>

namespace esphome {
namespace ltr390 {

static const char *const TAG = "ltr390";

static const uint8_t LTR390_WAKEUP_TIME = 10;
static const uint8_t LTR390_SETTLE_TIME = 5;

static const uint8_t LTR390_MAIN_CTRL = 0x00;
static const uint8_t LTR390_MEAS_RATE = 0x04;
static const uint8_t LTR390_GAIN = 0x05;
static const uint8_t LTR390_PART_ID = 0x06;
static const uint8_t LTR390_MAIN_STATUS = 0x07;

static const float GAINVALUES[5] = {1.0, 3.0, 6.0, 9.0, 18.0};
static const float RESOLUTIONVALUE[6] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125};
static const uint8_t RESOLUTION_BITS[6] = {20, 19, 18, 17, 16, 13};

// Request fastest measurement rate - will be slowed by device if conversion rate is slower.
static const float RESOLUTION_SETTING[6] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x50};
static const uint32_t MODEADDRESSES[2] = {0x0D, 0x10};

static const float SENSITIVITY_MAX = 2300;
static const float INTG_MAX = RESOLUTIONVALUE[0] * 100;
static const int GAIN_MAX = GAINVALUES[4];

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
    float lux = ((0.6 * als) / (GAINVALUES[this->gain_als_] * RESOLUTIONVALUE[this->res_als_])) * this->wfac_;
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
    this->uvi_sensor_->publish_state((uv / this->sensitivity_uv_) * this->wfac_);
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
  ctrl[LTR390_CTRL_EN] = true;
  this->reg(LTR390_MAIN_CTRL) = ctrl.to_ulong();

  uint32_t int_time{0};
  // Set gain, resolution and measurement rate
  switch (mode) {
    case LTR390_MODE_ALS:
      this->reg(LTR390_GAIN) = this->gain_als_;
      this->reg(LTR390_MEAS_RATE) = RESOLUTION_SETTING[this->res_als_];
      int_time = ((uint32_t) RESOLUTIONVALUE[this->res_als_]) * 100;
      break;
    case LTR390_MODE_UVS:
      this->reg(LTR390_GAIN) = this->gain_uv_;
      this->reg(LTR390_MEAS_RATE) = RESOLUTION_SETTING[this->res_uv_];
      int_time = ((uint32_t) RESOLUTIONVALUE[this->res_uv_]) * 100;
      break;
  }

  // After the sensor integration time do the following
  this->set_timeout(int_time + LTR390_WAKEUP_TIME + LTR390_SETTLE_TIME, [this, mode_index]() {
    // Read from the sensor
    std::get<1>(this->mode_funcs_[mode_index])();

    // If there are more modes to read then begin the next
    // otherwise stop
    if (mode_index + 1 < (int) this->mode_funcs_.size()) {
      this->read_mode_(mode_index + 1);
    } else {
      // put sensor in standby
      std::bitset<8> ctrl = this->reg(LTR390_MAIN_CTRL).get();
      ctrl[LTR390_CTRL_EN] = false;
      this->reg(LTR390_MAIN_CTRL) = ctrl.to_ulong();
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

  // Set sensitivity by linearly scaling against known value in the datasheet
  float gain_scale_uv = GAINVALUES[this->gain_uv_] / GAIN_MAX;
  float intg_scale_uv = (RESOLUTIONVALUE[this->res_uv_] * 100) / INTG_MAX;
  this->sensitivity_uv_ = SENSITIVITY_MAX * gain_scale_uv * intg_scale_uv;

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

void LTR390Component::dump_config() {
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  ALS Gain: X%.0f", GAINVALUES[this->gain_als_]);
  ESP_LOGCONFIG(TAG, "  ALS Resolution: %u-bit", RESOLUTION_BITS[this->res_als_]);
  ESP_LOGCONFIG(TAG, "  UV Gain: X%.0f", GAINVALUES[this->gain_uv_]);
  ESP_LOGCONFIG(TAG, "  UV Resolution: %u-bit", RESOLUTION_BITS[this->res_uv_]);
}

void LTR390Component::update() {
  if (!this->reading_ && !mode_funcs_.empty()) {
    this->reading_ = true;
    this->read_mode_(0);
  }
}

}  // namespace ltr390
}  // namespace esphome
