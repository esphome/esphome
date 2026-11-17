#include "opt3001.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::opt3001 {

ESPHOME_LOG_TAG(TAG, "opt3001.sensor");

static constexpr uint32_t RESULT_RETRY_TIMEOUT_ID = 0;
static constexpr uint32_t READ_TIMEOUT_ID = 1;

static constexpr uint8_t OPT3001_REG_RESULT = 0x00;
static constexpr uint8_t OPT3001_REG_CONFIGURATION = 0x01;
// See datasheet for full description of each bit.
static constexpr uint16_t OPT3001_CONFIGURATION_RANGE_FULL = 0b1100000000000000;
static constexpr uint16_t OPT3001_CONFIGURATION_CONVERSION_TIME_800 = 0b100000000000;
static constexpr uint16_t OPT3001_CONFIGURATION_CONVERSION_MODE_MASK = 0b11000000000;
static constexpr uint16_t OPT3001_CONFIGURATION_CONVERSION_MODE_SINGLE_SHOT = 0b01000000000;
static constexpr uint16_t OPT3001_CONFIGURATION_CONVERSION_MODE_SHUTDOWN = 0b00000000000;
// tl;dr: Configure an automatic-ranged, 800ms single shot reading,
// with INT processing disabled
static constexpr uint16_t OPT3001_CONFIGURATION_FULL_RANGE_ONE_SHOT = OPT3001_CONFIGURATION_RANGE_FULL |
                                                                      OPT3001_CONFIGURATION_CONVERSION_TIME_800 |
                                                                      OPT3001_CONFIGURATION_CONVERSION_MODE_SINGLE_SHOT;
// give it 25 extra ms; it seems to not be ready quite often
static constexpr uint16_t OPT3001_CONVERSION_TIME_800 = 825;
// 10 ms apart: 1 s past the conversion wait covers an auto-range restart (10 ms plus another 800 ms conversion)
static constexpr uint8_t RESULT_MAX_RETRIES = 100;

/*
opt3001 properties:

- e (exponent) = high 4 bits of result register
- m (mantissa) = low 12 bits of result register
- formula: (0.01 * 2^e) * m lx

*/

void OPT3001Sensor::read_result_(uint8_t retries_left) {
  // ensure the single shot flag is clear, indicating it's done
  uint16_t raw_value;
  if (!this->read_byte_16(OPT3001_REG_CONFIGURATION, &raw_value)) {
    ESP_LOGW(TAG, "Reading configuration register failed");
    this->publish_lx_(NAN);
    return;
  }

  if ((raw_value & OPT3001_CONFIGURATION_CONVERSION_MODE_MASK) != OPT3001_CONFIGURATION_CONVERSION_MODE_SHUTDOWN) {
    if (retries_left == 0) {
      ESP_LOGW(TAG, "Measurement did not complete");
      this->publish_lx_(NAN);
      return;
    }
    ESP_LOGV(TAG, "Data not ready; waiting 10ms");
    this->set_timeout(RESULT_RETRY_TIMEOUT_ID, 10, [this, retries_left]() { this->read_result_(retries_left - 1); });
    return;
  }

  if (!this->read_byte_16(OPT3001_REG_RESULT, &raw_value)) {
    ESP_LOGW(TAG, "Reading result register failed");
    this->publish_lx_(NAN);
    return;
  }

  const uint8_t exponent = raw_value >> 12;
  const uint32_t mantissa = raw_value & 0b111111111111;
  this->publish_lx_(0.01f * static_cast<float>(mantissa << exponent));
}

void OPT3001Sensor::dump_config() {
  LOG_SENSOR("", "OPT3001", this);
  LOG_I2C_DEVICE(this);

  LOG_UPDATE_INTERVAL(this);
}

void OPT3001Sensor::update() {
  // skip while the previous measurement is still in flight
  if (this->updating_) {
    return;
  }
  this->updating_ = true;

  // start a one-shot measurement (the sensor powers down after each one)
  if (!this->write_byte_16(OPT3001_REG_CONFIGURATION, OPT3001_CONFIGURATION_FULL_RANGE_ONE_SHOT)) {
    ESP_LOGW(TAG, "Triggering one shot measurement failed");
    this->publish_lx_(NAN);
    return;
  }
  this->set_timeout(READ_TIMEOUT_ID, OPT3001_CONVERSION_TIME_800, [this]() { this->read_result_(RESULT_MAX_RETRIES); });
}

void OPT3001Sensor::publish_lx_(float val) {
  this->updating_ = false;

  if (!this->status_comm_result(!std::isnan(val))) {
    this->publish_state(NAN);
    return;
  }
  ESP_LOGD(TAG, "'%s': Illuminance=%.1flx", LOG_STR_ARG(this->get_log_name()), val);
  this->publish_state(val);
}

}  // namespace esphome::opt3001
