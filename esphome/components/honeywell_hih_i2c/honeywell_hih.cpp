// Honeywell HumidIcon I2C Sensors
// https://prod-edam.honeywell.com/content/dam/honeywell-edam/sps/siot/en-us/products/sensors/humidity-with-temperature-sensors/common/documents/sps-siot-i2c-comms-humidicon-tn-009061-2-en-ciid-142171.pdf
//

#include "honeywell_hih.h"
#include "esphome/core/log.h"

namespace esphome {
namespace honeywell_hih_i2c {

static const char *const TAG = "honeywell_hih.i2c";

static const uint8_t REQUEST_CMD[1] = {0x00};  // Measurement Request Format
static const uint16_t MAX_COUNT = 0x3FFE;      // 2^14 - 2

void HoneywellHIComponent::read_sensor_data_() {
  uint8_t data[4];

  if (this->read(data, sizeof(data)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with Honeywell HIH failed!");
    this->mark_failed();
    return;
  }

  const uint16_t raw_humidity = (static_cast<uint16_t>(data[0] & 0x3F) << 8) | data[1];
  float humidity = (static_cast<float>(raw_humidity) / MAX_COUNT) * 100;

  const uint16_t raw_temperature = (static_cast<uint16_t>(data[2]) << 6) | (data[3] >> 2);
  float temperature = (static_cast<float>(raw_temperature) / MAX_COUNT) * 165 - 40;

  ESP_LOGD(TAG, "Got temperature=%.2f°C humidity=%.2f%%", temperature, humidity);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(humidity);
}

void HoneywellHIComponent::start_measurement_() {
  if (this->write(REQUEST_CMD, sizeof(REQUEST_CMD)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with Honeywell HIH failed!");
    this->mark_failed();
    return;
  }

  this->measurement_running_ = true;
}

bool HoneywellHIComponent::is_measurement_ready_() {
  uint8_t data[1];

  if (this->read(data, sizeof(data)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with Honeywell HIH failed!");
    this->mark_failed();
    return false;
  }

  // Check status bits
  return ((data[0] & 0xC0) == 0x00);
}

void HoneywellHIComponent::measurement_timeout_() {
  ESP_LOGE(TAG, "Honeywell HIH Timeout!");
  this->measurement_running_ = false;
  this->mark_failed();
}

void HoneywellHIComponent::update() {
  ESP_LOGV(TAG, "Update Honeywell HIH Sensor");

  this->start_measurement_();
  // The measurement cycle duration is typically 36.65 ms for temperature and humidity readings.
  this->set_timeout("meas_timeout", 100, [this] { this->measurement_timeout_(); });
}

void HoneywellHIComponent::loop() {
  if (this->measurement_running_ && this->is_measurement_ready_()) {
    this->measurement_running_ = false;
    this->cancel_timeout("meas_timeout");
    this->read_sensor_data_();
  }
}

void HoneywellHIComponent::dump_config() {
  ESP_LOGD(TAG, "Honeywell HIH:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with Honeywell HIH failed!");
  }
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

float HoneywellHIComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace honeywell_hih_i2c
}  // namespace esphome
