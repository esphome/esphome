#include "stcc4.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace stcc4 {

static const char *const TAG = "stcc4";

// I2C Commands
static const uint16_t STCC4_CMD_START_CONTINUOUS_MEASUREMENT = 0x218b;
static const uint16_t STCC4_CMD_READ_MEASUREMENT = 0xec05;
static const uint16_t STCC4_CMD_STOP_CONTINUOUS_MEASUREMENT = 0x3f86;
static const uint16_t STCC4_CMD_MEASURE_SINGLE_SHOT = 0x219d;
static const uint16_t STCC4_CMD_GET_PRODUCT_ID = 0x365b;
static const uint16_t STCC4_CMD_SET_RHT_COMPENSATION = 0xe000;
static const uint16_t STCC4_CMD_SET_PRESSURE_COMPENSATION = 0xe016;
// Exit sleep is an 8-bit command (single byte 0x00), not 16-bit
static const uint8_t STCC4_CMD_EXIT_SLEEP_MODE = 0x00;

void STCC4Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up STCC4...");

  // Wake sensor from sleep mode (send single byte 0x00)
  // The sensor needs some time after power-up
  this->set_timeout(100, [this]() {
    // Send exit sleep mode command (8-bit, NACK expected if already awake)
    if (!this->write_command(STCC4_CMD_EXIT_SLEEP_MODE)) {
      ESP_LOGW(TAG, "Failed to send exit sleep command, sensor may already be awake");
    }

    // Wait 5ms for sensor to be ready after exiting sleep
    this->set_timeout(5, [this]() {
      // Read product ID to verify communication (6 words: 2 for product_id + 4 for serial)
      uint16_t raw_product_id[6];
      if (!this->get_register(STCC4_CMD_GET_PRODUCT_ID, raw_product_id, 6, 1)) {
        ESP_LOGE(TAG, "Failed to read product ID");
        this->mark_failed();
        return;
      }

      uint32_t product_id = (uint32_t(raw_product_id[0]) << 16) | raw_product_id[1];
      uint64_t serial_number = (uint64_t(raw_product_id[2]) << 48) | (uint64_t(raw_product_id[3]) << 32) |
                               (uint64_t(raw_product_id[4]) << 16) | raw_product_id[5];
      ESP_LOGD(TAG, "Product ID: 0x%08" PRIX32 ", Serial: 0x%016" PRIX64, product_id, serial_number);

      // Set initial pressure compensation if configured
      if (this->ambient_pressure_ != 0) {
        if (!this->update_ambient_pressure_compensation_(this->ambient_pressure_)) {
          ESP_LOGE(TAG, "Failed to set ambient pressure compensation");
          this->mark_failed();
          return;
        }
      }

      this->initialized_ = true;

      // Start continuous measurement if in continuous mode
      if (this->measurement_mode_ == CONTINUOUS) {
        this->start_measurement_();
      }
    });
  });
}

void STCC4Component::dump_config() {
  ESP_LOGCONFIG(TAG, "STCC4:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGW(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  ESP_LOGCONFIG(TAG, "  Measurement mode: %s",
                this->measurement_mode_ == CONTINUOUS ? "Continuous (1s)" : "Single shot");
  if (this->ambient_pressure_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Dynamic ambient pressure compensation using '%s'",
                  this->ambient_pressure_source_->get_name().c_str());
  } else if (this->ambient_pressure_ != 0) {
    ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %" PRIu16 " hPa", this->ambient_pressure_);
  }
  if (this->temperature_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Temperature compensation using '%s'", this->temperature_source_->get_name().c_str());
  }
  if (this->humidity_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Humidity compensation using '%s'", this->humidity_source_->get_name().c_str());
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void STCC4Component::update() {
  if (!this->initialized_) {
    return;
  }

  // Update RHT compensation from external sensors
  this->update_rht_compensation_();

  // Update pressure compensation from external sensor
  if (this->ambient_pressure_source_ != nullptr) {
    float pressure = this->ambient_pressure_source_->state;
    if (!std::isnan(pressure)) {
      uint16_t new_pressure = static_cast<uint16_t>(pressure);
      if (new_pressure != this->ambient_pressure_) {
        this->update_ambient_pressure_compensation_(new_pressure);
        this->ambient_pressure_ = new_pressure;
      }
    }
  }

  uint32_t wait_time = 0;
  if (this->measurement_mode_ == SINGLE_SHOT) {
    // Start single shot measurement
    if (!this->write_command(STCC4_CMD_MEASURE_SINGLE_SHOT)) {
      ESP_LOGW(TAG, "Failed to start single shot measurement");
      this->status_set_warning();
      return;
    }
    wait_time = 500;  // Single shot takes 500ms
  }

  this->set_timeout(wait_time, [this]() {
    // Read measurement data: 4 words (CO2, temperature, humidity, status)
    uint16_t raw_data[4];
    if (!this->get_register(STCC4_CMD_READ_MEASUREMENT, raw_data, 4, 1)) {
      ESP_LOGW(TAG, "Failed to read measurement data");
      this->status_set_warning();
      return;
    }

    // CO2 value is in ppm as int16 (can be negative during warm-up)
    int16_t co2_raw = static_cast<int16_t>(raw_data[0]);

    // Only publish valid CO2 values (non-negative)
    if (co2_raw >= 0 && this->co2_sensor_ != nullptr) {
      this->co2_sensor_->publish_state(co2_raw);
    }

    // Temperature: -45 + 175 * (raw / 65535)
    if (this->temperature_sensor_ != nullptr) {
      float temperature = -45.0f + (175.0f * raw_data[1]) / 65535.0f;
      this->temperature_sensor_->publish_state(temperature);
    }

    // Humidity: -6 + 125 * (raw / 65535)
    if (this->humidity_sensor_ != nullptr) {
      float humidity = -6.0f + (125.0f * raw_data[2]) / 65535.0f;
      this->humidity_sensor_->publish_state(humidity);
    }

    this->status_clear_warning();
  });
}

bool STCC4Component::update_rht_compensation_() {
  if (this->temperature_source_ == nullptr || this->humidity_source_ == nullptr) {
    return true;  // No compensation sources configured
  }

  float temperature = this->temperature_source_->state;
  float humidity = this->humidity_source_->state;

  if (std::isnan(temperature) || std::isnan(humidity)) {
    return false;  // No valid data yet
  }

  // Convert to ticks using SHT4x formula:
  // temperature = -45 + 175 * (ticks / 65535)
  // humidity = -6 + 125 * (ticks / 65535)
  uint16_t temp_ticks = static_cast<uint16_t>(((temperature + 45.0f) / 175.0f) * 65535.0f);
  uint16_t humidity_ticks = static_cast<uint16_t>(((humidity + 6.0f) / 125.0f) * 65535.0f);

  uint16_t data[2] = {temp_ticks, humidity_ticks};
  if (!this->write_command(STCC4_CMD_SET_RHT_COMPENSATION, data, 2)) {
    ESP_LOGW(TAG, "Failed to set RHT compensation");
    return false;
  }

  return true;
}

bool STCC4Component::update_ambient_pressure_compensation_(uint16_t pressure_in_hpa) {
  // Pressure is sent as Pa / 2 (so hPa * 50)
  uint16_t pressure_raw = pressure_in_hpa * 50;

  if (!this->write_command(STCC4_CMD_SET_PRESSURE_COMPENSATION, pressure_raw)) {
    ESP_LOGE(TAG, "Failed to set ambient pressure compensation");
    return false;
  }
  ESP_LOGD(TAG, "Set ambient pressure compensation to %" PRIu16 " hPa", pressure_in_hpa);
  return true;
}

bool STCC4Component::start_measurement_() {
  if (!this->write_command(STCC4_CMD_START_CONTINUOUS_MEASUREMENT)) {
    ESP_LOGE(TAG, "Failed to start continuous measurement");
    this->status_set_warning();
    return false;
  }
  ESP_LOGD(TAG, "Started continuous measurement");
  return true;
}

void STCC4Component::set_ambient_pressure_compensation(float pressure_in_hpa) {
  this->ambient_pressure_ = static_cast<uint16_t>(pressure_in_hpa);
}

}  // namespace stcc4
}  // namespace esphome
