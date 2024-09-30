#include "mics_4514.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mics_4514 {

static const char *const TAG = "mics_4514";

static const uint8_t SENSOR_REGISTER = 0x04;
static const uint8_t POWER_MODE_REGISTER = 0x0a;

void MICS4514Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MICS 4514...");
  uint8_t power_mode;
  this->read_register(POWER_MODE_REGISTER, &power_mode, 1);
  if (power_mode == 0x00) {
    ESP_LOGCONFIG(TAG, "Waking up MICS 4514, sensors will have data after 3 minutes...");
    power_mode = 0x01;
    this->write_register(POWER_MODE_REGISTER, &power_mode, 1);
    delay(100);  // NOLINT
    this->set_timeout("warmup", 3 * 60 * 1000, [this]() {
      this->warmed_up_ = true;
      ESP_LOGCONFIG(TAG, "MICS 4514 setup complete.");
    });
    this->status_set_warning();
    return;
  }
  ESP_LOGCONFIG(TAG, "Device already awake.");
  this->warmed_up_ = true;
  ESP_LOGCONFIG(TAG, "MICS 4514 setup complete.");
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
float MICS4514Component::get_setup_priority() const { return setup_priority::DATA; }
void MICS4514Component::update() {
  if (!this->warmed_up_) {
    return;
  }
  uint8_t data[6];
  if (this->read_register(SENSOR_REGISTER, data, 6) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  ESP_LOGV(TAG, "Got data: %02X %02X %02X %02X %02X %02X", data[0], data[1], data[2], data[3], data[4], data[5]);
  uint16_t ox = encode_uint16(data[0], data[1]);
  uint16_t red = encode_uint16(data[2], data[3]);
  uint16_t power = encode_uint16(data[4], data[5]);

  if (this->initial_) {
    this->initial_ = false;
    this->ox_calibration_ = (float) (power - ox);
    this->red_calibration_ = (float) (power - red);
    return;
  }

  float red_f = (float) (power - red) / this->red_calibration_;
  float ox_f = (float) (power - ox) / this->ox_calibration_;

  if (this->carbon_monoxide_sensor_ != nullptr) {
    float co = 0.0f;
    if (red_f > 3.4f) {
      co = 0.0;
    } else if (red_f < 0.01) {
      co = 1000.0;
    } else {
      co = 4.2 / pow(red_f, 1.2);
    }
    this->carbon_monoxide_sensor_->publish_state(co);
  }

  if (this->nitrogen_dioxide_sensor_ != nullptr) {
    float nitrogendioxide = 0.0f;
    if (ox_f < 0.3f) {
      nitrogendioxide = 0.0;
    } else {
      nitrogendioxide = 0.164 * pow(ox_f, 0.975);
    }
    this->nitrogen_dioxide_sensor_->publish_state(nitrogendioxide);
  }

  if (this->methane_sensor_ != nullptr) {
    float methane = 0.0f;
    if (red_f > 0.9f || red_f < 0.5) {  // outside the range->unlikely
      methane = 0.0;
    } else {
      methane = 630 / pow(red_f, 4.4);
    }
    this->methane_sensor_->publish_state(methane);
  }

  if (this->ethanol_sensor_ != nullptr) {
    float ethanol = 0.0f;
    if (red_f > 1.0f || red_f < 0.02) {  // outside the range->unlikely
      ethanol = 0.0;
    } else {
      ethanol = 1.52 / pow(red_f, 1.55);
    }
    this->ethanol_sensor_->publish_state(ethanol);
  }

  if (this->hydrogen_sensor_ != nullptr) {
    float hydrogen = 0.0f;
    if (red_f > 0.9f || red_f < 0.02) {  // outside the range->unlikely
      hydrogen = 0.0;
    } else {
      hydrogen = 0.85 / pow(red_f, 1.75);
    }
    this->hydrogen_sensor_->publish_state(hydrogen);
  }

  if (this->ammonia_sensor_ != nullptr) {
    float ammonia = 0.0f;
    if (red_f > 0.98f || red_f < 0.2532) {  // outside the ammonia range->unlikely
      ammonia = 0.0;
    } else {
      ammonia = 0.9 / pow(red_f, 4.6);
    }
    this->ammonia_sensor_->publish_state(ammonia);
  }
}

}  // namespace mics_4514
}  // namespace esphome
