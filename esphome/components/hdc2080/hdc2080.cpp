#include "hdc2080.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::hdc2080 {

static const char *const TAG = "hdc2080";

// Register map (Table 8-6)
static constexpr uint8_t REG_TEMPERATURE_LOW = 0x00;      // Temperature [7:0]
static constexpr uint8_t REG_TEMPERATURE_HIGH = 0x01;     // Temperature [15:8]
static constexpr uint8_t REG_HUMIDITY_LOW = 0x02;         // Humidity [7:0]
static constexpr uint8_t REG_HUMIDITY_HIGH = 0x03;        // Humidity [15:8]
static constexpr uint8_t REG_RESET_DRDY_INT_CONF = 0x0E;  // Soft Reset and Interrupt Configuration
static constexpr uint8_t REG_MEASUREMENT_CONFIGURATION = 0x0F;

// Measurement register (0x0F) bit fields
static constexpr uint8_t MEAS_TRIG = 0x01;       // Bit 0: start measurement
static constexpr uint8_t MEAS_CONF_TEMP = 0x02;  // Bits 2:1 = 01: temperature only
static constexpr uint8_t MEAS_CONF_HUM = 0x04;   // Bits 2:1 = 10: humidity only

void HDC2080Component::setup() {
  const uint8_t data = 0x00;  // automatic measurement mode disabled, heater off
  if (this->write_register(REG_RESET_DRDY_INT_CONF, &data, 1) != i2c::ERROR_OK) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
}

void HDC2080Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HDC2080:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void HDC2080Component::update() {
  uint8_t data = MEAS_TRIG;  // 14-bit resolution, measure both, start
  if (this->temperature_sensor_ != nullptr && this->humidity_sensor_ == nullptr) {
    data = MEAS_TRIG | MEAS_CONF_TEMP;
  } else if (this->temperature_sensor_ == nullptr && this->humidity_sensor_ != nullptr) {
    data = MEAS_TRIG | MEAS_CONF_HUM;
  }
  if (this->write_register(REG_MEASUREMENT_CONFIGURATION, &data, 1) != i2c::ERROR_OK) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  // wait for conversion to complete 2ms should be enough, more is fine
  this->set_timeout(5, [this]() {
    uint8_t raw_data[4];
    if (this->read_register(REG_TEMPERATURE_LOW, raw_data, 4) != i2c::ERROR_OK) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return;
    }
    this->status_clear_warning();
    if (this->temperature_sensor_ != nullptr) {
      float temp = encode_uint16(raw_data[1], raw_data[0]) * (165.0f / 65536.0f) - 40.5f;
      this->temperature_sensor_->publish_state(temp);
    }
    if (this->humidity_sensor_ != nullptr) {
      float humidity = encode_uint16(raw_data[3], raw_data[2]) * (100.0f / 65536.0f);
      this->humidity_sensor_->publish_state(humidity);
    }
  });
}

}  // namespace esphome::hdc2080
