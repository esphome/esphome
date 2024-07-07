#include "mcp3428_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mcp3428 {

static const char *const TAG = "mcp3426/7/8.sensor";
static const uint8_t MEASUREMENT_INITIATE_MAX_TRIES = 10;

float MCP3428Sensor::sample() {
  uint32_t wait = 0;
  float res = NAN;
  // initiate Measurement
  if (!this->parent_->request_measurement(this->multiplexer_, this->gain_, this->resolution_, wait)) {
    return res;  // if sensor is busy there is no easy way the situation can be resolved in a synchronous manner
  }
  delay(wait);  // certainly not ideal but necessary when the result needs to be returned now

  bool success = this->parent_->poll_result(res);
  if ((!success) || std::isnan(res)) {
    this->parent_->abandon_current_measurement();
  } else {
    this->status_clear_warning();
  }
  return res;
}

void MCP3428Sensor::update() {
  this->set_retry(MEASUREMENT_TIME_16BIT_MS, MEASUREMENT_INITIATE_MAX_TRIES,
                  [this](const uint8_t remaining_initiate_attempts) {
                    uint32_t wait;
                    if (this->parent_->request_measurement(this->multiplexer_, this->gain_, this->resolution_, wait)) {
                      // measurement started, set timeout for retrieving value
                      this->set_timeout(wait, [this]() {
                        float res = NAN;
                        bool success = this->parent_->poll_result(res);
                        if (success && !std::isnan(res)) {
                          ESP_LOGD(TAG, "'%s': Got Voltage=%fV", this->get_name().c_str(), res);
                          this->publish_state(res);
                          this->status_clear_warning();
                        } else {
                          this->status_set_warning("No valid measurement returned");
                          this->parent_->abandon_current_measurement();
                        }
                      });
                      return RetryResult::DONE;
                    }
                    if (remaining_initiate_attempts == 0) {
                      this->status_set_warning("Could not initiate Measurement");
                    }
                    return RetryResult::RETRY;
                  });
}

void MCP3428Sensor::dump_config() {
  LOG_SENSOR("  ", "MCP3426/7/8 Sensor", this);
  ESP_LOGCONFIG(TAG, "    Multiplexer: Channel %u", this->multiplexer_ + 1);
  ESP_LOGCONFIG(TAG, "    Gain: %u", 0b0001 << this->gain_);
  ESP_LOGCONFIG(TAG, "    Resolution: %u", 12 + 2 * this->resolution_);
}

}  // namespace mcp3428
}  // namespace esphome
