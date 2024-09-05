#include "acurite_sensor.h"

namespace esphome {
namespace acurite {

static const char *const TAG = "acurite";

void AcuRiteSensor::update_uv(float value) {
  if (this->uv_sensor_) {
    // do not confirm light values as they can change rapidly
    if (value >= 0.0f && value <= 15.0f) {
      this->uv_sensor_->publish_state(value);
    }
  }
}

void AcuRiteSensor::update_lux(float value) {
  if (this->lux_sensor_) {
    // do not confirm light values as they can change rapidly
    if (value >= 0.0f && value <= 120000.0f) {
      this->lux_sensor_->publish_state(value);
    }
  }
}

void AcuRiteSensor::update_direction(float value) {
  if (this->direction_sensor_) {
    // do not confirm wind values as they can change rapidly
    if (value >= 0.0f && value <= 360.0f) {
      this->direction_sensor_->publish_state(value);
    }
  }
}

void AcuRiteSensor::update_speed(float value) {
  if (this->speed_sensor_) {
    // do not confirm wind values as they can change rapidly
    if (value >= 0.0f && value <= 257.0f) {
      this->speed_sensor_->publish_state(value);
    }
  }
}

void AcuRiteSensor::update_temperature(float value) {
  if (this->temperature_sensor_) {
    // filter out crc false positives by confirming any large changes in value
    if (fabsf(value - this->temperature_last_) < 1.0) {
      this->temperature_sensor_->publish_state(value);
    }
    this->temperature_last_ = value;
  }
}

void AcuRiteSensor::update_humidity(float value) {
  if (this->humidity_sensor_) {
    // filter out crc false positives by confirming any large changes in value
    if (fabsf(value - this->humidity_last_) < 1.0) {
      this->humidity_sensor_->publish_state(value);
    }
    this->humidity_last_ = value;
  }
}

void AcuRiteSensor::update_distance(float value) {
  if (this->distance_sensor_) {
    // filter out crc false positives by confirming any large changes in value
    if (fabsf(value - this->distance_last_) < 1.0) {
      this->distance_sensor_->publish_state(value);
    }
    this->distance_last_ = value;
  }
}

void AcuRiteSensor::update_lightning(uint32_t count) {
  if (this->lightning_sensor_) {
    // filter out crc false positives by confirming any change in value
    if (count == this->lightning_last_) {
      this->lightning_sensor_->publish_state(count);
    }
    this->lightning_last_ = count;
  }
}

void AcuRiteSensor::update_rainfall(uint32_t count) {
  if (this->rainfall_sensor_) {
    // filter out crc false positives by confirming any change in value
    if (count == this->rainfall_last_) {
      // update rainfall state and save to nvm
      if (count != this->rainfall_state_.device) {
        if (count >= this->rainfall_state_.device) {
          this->rainfall_state_.total += count - this->rainfall_state_.device;
        } else {
          this->rainfall_state_.total += count;
        }
        this->rainfall_state_.device = count;
        this->preferences_.save(&this->rainfall_state_);
      }
      this->rainfall_sensor_->publish_state(this->rainfall_state_.total * 0.254);
    }
    this->rainfall_last_ = count;
  }
}

void AcuRiteSensor::setup() {
  if (this->rainfall_sensor_) {
    // load state from nvm, count needs to increase even after a reset
    uint32_t type = fnv1_hash(std::string("AcuRite: ") + format_hex(this->id_));
    this->preferences_ = global_preferences->make_preference<RainfallState>(type);
    this->preferences_.load(&this->rainfall_state_);
  }
}

void AcuRiteSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "AcuRite 0x%04x:", this->id_);
  LOG_SENSOR("  ", "Speed", this->speed_sensor_);
  LOG_SENSOR("  ", "Direction", this->direction_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Rainfall", this->rainfall_sensor_);
  LOG_SENSOR("  ", "Distance", this->distance_sensor_);
  LOG_SENSOR("  ", "Lightning", this->lightning_sensor_);
  LOG_SENSOR("  ", "Lux", this->lux_sensor_);
  LOG_SENSOR("  ", "UV", this->uv_sensor_);
}

}  // namespace acurite
}  // namespace esphome