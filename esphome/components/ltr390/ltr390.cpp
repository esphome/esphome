#include "ltr390.h"
#include <bitset>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ltr390 {

ESPHOME_LOG_TAG(TAG, "ltr390");

static const uint8_t LTR390_WAKEUP_TIME = 10;
static const uint8_t LTR390_SETTLE_TIME = 5;

static const uint8_t LTR390_MAIN_CTRL = 0x00;
static const uint8_t LTR390_MEAS_RATE = 0x04;
static const uint8_t LTR390_GAIN = 0x05;
static const uint8_t LTR390_PART_ID = 0x06;
static const uint8_t LTR390_MAIN_STATUS = 0x07;

static constexpr uint8_t LTR390_ALS_DATA = 0x0D;
static constexpr uint8_t LTR390_UVS_DATA = 0x10;

static constexpr uint8_t GAINVALUES[5] PROGMEM = {1, 3, 6, 9, 18};
static uint8_t gain_value(LTR390GAIN gain) { return progmem_read_byte(&GAINVALUES[gain]); }
static uint8_t resolution_bits(LTR390RESOLUTION res) { return res == LTR390_RESOLUTION_13BIT ? 13 : 20 - res; }

// UV counts per UVI at 18x gain and 20-bit resolution
static constexpr float SENSITIVITY_MAX = 2300;
static constexpr float GAIN_MAX = 18;

optional<uint32_t> LTR390Component::read_sensor_data_(LTR390MODE mode) {
  const uint8_t num_bytes = 3;
  uint8_t buffer[num_bytes];

  // Wait until data available
  constexpr uint32_t max_wait_ms = 25;
  const uint32_t now = millis();
  while (true) {
    std::bitset<8> status = this->reg(LTR390_MAIN_STATUS).get();
    bool available = status[3];
    if (available)
      break;

    if (millis() - now > max_wait_ms) {
      ESP_LOGW(TAG, "Sensor didn't return any data, aborting");
      return {};
    }
    ESP_LOGV(TAG, "Waiting for data");
    delay(1);
  }

  if (!this->read_bytes(mode == LTR390_MODE_UVS ? LTR390_UVS_DATA : LTR390_ALS_DATA, buffer, num_bytes)) {
    ESP_LOGW(TAG, "Reading data from sensor failed!");
    return {};
  }

  return encode_uint24(buffer[2], buffer[1], buffer[0]);
}

void LTR390Component::read_als_() {
  auto val = this->read_sensor_data_(LTR390_MODE_ALS);
  if (!val.has_value())
    return;
  uint32_t als = *val;

  if (this->light_sensor_ != nullptr) {
    // The integration time halves per resolution step, from 400 ms at 20-bit
    float lux = ((0.6f * als * (1 << this->res_als_)) / (4.0f * gain_value(this->gain_als_))) * this->wfac_;
    this->light_sensor_->publish_state(lux);
  }

  PUBLISH_STATE(this->als_sensor_, als);
}

void LTR390Component::read_uvs_() {
  auto val = this->read_sensor_data_(LTR390_MODE_UVS);
  if (!val.has_value())
    return;
  uint32_t uv = *val;

  if (this->uvi_sensor_ != nullptr) {
    // Set sensitivity by linearly scaling against known value in the datasheet
    float sensitivity_uv = SENSITIVITY_MAX * gain_value(this->gain_uv_) / (GAIN_MAX * (1 << this->res_uv_));

    this->uvi_sensor_->publish_state((uv / sensitivity_uv) * this->wfac_);
  }

  PUBLISH_STATE(this->uv_sensor_, uv);
}

void LTR390Component::standby_() {
  std::bitset<8> ctrl = this->reg(LTR390_MAIN_CTRL).get();
  ctrl[LTR390_CTRL_EN] = false;
  this->reg(LTR390_MAIN_CTRL) = ctrl.to_ulong();
  this->reading_ = false;
}

void LTR390Component::read_mode_(LTR390MODE mode) {
  // Set mode
  std::bitset<8> ctrl = this->reg(LTR390_MAIN_CTRL).get();
  ctrl[LTR390_CTRL_MODE] = (mode == LTR390_MODE_UVS);
  ctrl[LTR390_CTRL_EN] = true;
  this->reg(LTR390_MAIN_CTRL) = ctrl.to_ulong();

  const bool uv = mode == LTR390_MODE_UVS;
  const uint8_t res = uv ? this->res_uv_ : this->res_als_;
  this->reg(LTR390_GAIN) = uv ? this->gain_uv_ : this->gain_als_;
  // Resolution in bits 6:4, rate 0 (fastest); the device slows it to the conversion time
  this->reg(LTR390_MEAS_RATE) = res << 4;
  // Conversion time: 400 ms at 20-bit, halved per step (12 ms at 13-bit)
  const uint32_t int_time = 400u >> res;

  // After the sensor integration time do the following
  this->set_timeout(int_time + LTR390_WAKEUP_TIME + LTR390_SETTLE_TIME, [this, mode]() {
    // Read from the sensor and continue to next mode or standby
    if (mode == LTR390_MODE_ALS) {
      this->read_als_();
      if (this->enabled_modes_ & ENABLED_MODE_UVS) {
        this->read_mode_(LTR390_MODE_UVS);
        return;
      }
    } else {
      this->read_uvs_();
    }
    this->standby_();
  });
}

void LTR390Component::setup() {
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

  // Set sensor read state
  this->reading_ = false;

  // Determine which modes are enabled based on configured sensors
  if (this->light_sensor_ != nullptr || this->als_sensor_ != nullptr) {
    this->enabled_modes_ |= ENABLED_MODE_ALS;
  }
  if (this->uvi_sensor_ != nullptr || this->uv_sensor_ != nullptr) {
    this->enabled_modes_ |= ENABLED_MODE_UVS;
  }
}

void LTR390Component::dump_config() {
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG,
                "  ALS Gain: X%u\n"
                "  ALS Resolution: %u-bit\n"
                "  UV Gain: X%u\n"
                "  UV Resolution: %u-bit",
                gain_value(this->gain_als_), resolution_bits(this->res_als_), gain_value(this->gain_uv_),
                resolution_bits(this->res_uv_));
}

void LTR390Component::update() {
  if (this->reading_ || this->enabled_modes_ == 0)
    return;

  this->reading_ = true;
  this->read_mode_((this->enabled_modes_ & ENABLED_MODE_ALS) ? LTR390_MODE_ALS : LTR390_MODE_UVS);
}

}  // namespace esphome::ltr390
