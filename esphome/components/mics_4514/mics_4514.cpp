#include "mics_4514.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::mics_4514 {

ESPHOME_LOG_TAG(TAG, "mics_4514");

static constexpr uint32_t WARMUP_TIMEOUT_ID = 0;

static const uint8_t SENSOR_REGISTER = 0x04;
static const uint8_t POWER_MODE_REGISTER = 0x0a;

void MICS4514Component::setup() {
  uint8_t power_mode;
  this->read_register(POWER_MODE_REGISTER, &power_mode, 1);
  if (power_mode == 0x00) {
    ESP_LOGCONFIG(TAG, "Waking up MICS 4514, sensors will have data after 3 minutes");
    power_mode = 0x01;
    this->write_register(POWER_MODE_REGISTER, &power_mode, 1);
    delay(100);  // NOLINT
    this->set_timeout(WARMUP_TIMEOUT_ID, 3 * 60 * 1000, [this]() { this->warmed_up_ = true; });
    this->status_set_warning();
    return;
  }
  ESP_LOGCONFIG(TAG, "Device already awake.");
  this->warmed_up_ = true;
}
void MICS4514Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MICS 4514:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Nitrogen Dioxide", this->nitrogen_dioxide_sensor_);
  LOG_SENSOR("  ", "Carbon Monoxide", this->carbon_monoxide_sensor_);
  LOG_SENSOR("  ", "Methane", this->methane_sensor_);
  LOG_SENSOR("  ", "Ethanol", this->ethanol_sensor_);
  LOG_SENSOR("  ", "Hydrogen", this->hydrogen_sensor_);
  LOG_SENSOR("  ", "Ammonia", this->ammonia_sensor_);
}
void MICS4514Component::update() {
  if (!this->warmed_up_) {
    return;
  }
  uint8_t data[6];
  if (!this->status_comm_result(this->read_register(SENSOR_REGISTER, data, 6) == i2c::ERROR_OK))
    return;
  ESP_LOGV_HEX(TAG, data, sizeof(data), sizeof(data), "Got data: %s");
  uint16_t ox = encode_uint16(data[0], data[1]);
  uint16_t red = encode_uint16(data[2], data[3]);
  uint16_t power = encode_uint16(data[4], data[5]);

  if (this->initial_) {
    this->initial_ = false;
    this->ox_calibration_ = (float) (power - ox);
    this->red_calibration_ = (float) (power - red);
    return;
  }

  if (this->red_calibration_ == 0 || this->ox_calibration_ == 0) {
    ESP_LOGW(TAG, "Calibration values are zero, retrying");
    this->status_set_warning();
    this->initial_ = true;
    return;
  }
  float red_f = (float) (power - red) / this->red_calibration_;
  float ox_f = (float) (power - ox) / this->ox_calibration_;

  if (this->carbon_monoxide_sensor_ != nullptr) {
    float co = 0.0f;
    if (red_f > 3.4f) {
      co = 0.0;
    } else if (red_f < 0.01f) {
      co = 1000.0;
    } else {
      co = 4.2f / powf(red_f, 1.2f);
    }
    this->carbon_monoxide_sensor_->publish_state(co);
  }

  if (this->nitrogen_dioxide_sensor_ != nullptr) {
    const float nitrogendioxide = ox_f < 0.3f ? 0.0f : 0.164f * powf(ox_f, 0.975f);
    this->nitrogen_dioxide_sensor_->publish_state(nitrogendioxide);
  }

  if (this->methane_sensor_ != nullptr) {
    // 0 outside the sensor's range
    const float methane = (red_f > 0.9f || red_f < 0.5f) ? 0.0f : 630 / powf(red_f, 4.4f);
    this->methane_sensor_->publish_state(methane);
  }

  if (this->ethanol_sensor_ != nullptr) {
    // 0 outside the sensor's range
    const float ethanol = (red_f > 1.0f || red_f < 0.02f) ? 0.0f : 1.52f / powf(red_f, 1.55f);
    this->ethanol_sensor_->publish_state(ethanol);
  }

  if (this->hydrogen_sensor_ != nullptr) {
    // 0 outside the sensor's range
    const float hydrogen = (red_f > 0.9f || red_f < 0.02f) ? 0.0f : 0.85f / powf(red_f, 1.75f);
    this->hydrogen_sensor_->publish_state(hydrogen);
  }

  if (this->ammonia_sensor_ != nullptr) {
    // 0 outside the sensor's range
    const float ammonia = (red_f > 0.98f || red_f < 0.2532f) ? 0.0f : 0.9f / powf(red_f, 4.6f);
    this->ammonia_sensor_->publish_state(ammonia);
  }
}

}  // namespace esphome::mics_4514
