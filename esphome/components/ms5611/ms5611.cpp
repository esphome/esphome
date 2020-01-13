#include "ms5611.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ms5611 {

static const char *TAG = "ms5611";

static const uint8_t MS5611_ADDRESS = 0x77;
static const uint8_t MS5611_CMD_ADC_READ = 0x00;
static const uint8_t MS5611_CMD_RESET = 0x1E;
static const uint8_t MS5611_CMD_CONV_D1 = 0x40;
static const uint8_t MS5611_CMD_CONV_D2 = 0x50;
static const uint8_t MS5611_CMD_READ_PROM = 0xA2;

void MS5611Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MS5611...");
  if (!this->write_bytes(MS5611_CMD_RESET, nullptr, 0)) {
    this->mark_failed();
    return;
  }
  delay(100);  // NOLINT
  for (uint8_t offset = 0; offset < 6; offset++) {
    if (!this->read_byte_16(MS5611_CMD_READ_PROM + (offset * 2), &this->prom_[offset])) {
      this->mark_failed();
      return;
    }
  }
}
void MS5611Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MS5611:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MS5611 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
}
float MS5611Component::get_setup_priority() const { return setup_priority::DATA; }
void MS5611Component::update() {
  // request temperature reading
  if (!this->write_bytes(MS5611_CMD_CONV_D2 + 0x08, nullptr, 0)) {
    this->status_set_warning();
    return;
  }

  auto f = std::bind(&MS5611Component::read_temperature_, this);
  this->set_timeout("temperature", 10, f);
}
void MS5611Component::read_temperature_() {
  uint8_t bytes[3];
  if (!this->read_bytes(MS5611_CMD_ADC_READ, bytes, 3)) {
    this->status_set_warning();
    return;
  }
  const uint32_t raw_temperature = (uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]));

  // request pressure reading
  if (!this->write_bytes(MS5611_CMD_CONV_D1 + 0x08, nullptr, 0)) {
    this->status_set_warning();
    return;
  }

  auto f = std::bind(&MS5611Component::read_pressure_, this, raw_temperature);
  this->set_timeout("pressure", 10, f);
}
void MS5611Component::read_pressure_(uint32_t raw_temperature) {
  uint8_t bytes[3];
  if (!this->read_bytes(MS5611_CMD_ADC_READ, bytes, 3)) {
    this->status_set_warning();
    return;
  }
  const uint32_t raw_pressure = (uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]));
  this->calculate_values_(raw_temperature, raw_pressure);
}
void MS5611Component::calculate_values_(uint32_t raw_temperature, uint32_t raw_pressure) {
  const int32_t d_t = int32_t(raw_temperature) - (uint32_t(this->prom_[4]) << 8);
  float temperature = (2000 + (int64_t(d_t) * this->prom_[5]) / 8388608.0f) / 100.0f;

  float pressure_offset = (uint32_t(this->prom_[1]) << 16) + ((this->prom_[3] * d_t) >> 7);
  float pressure_sensitivity = (uint32_t(this->prom_[0]) << 15) + ((this->prom_[2] * d_t) >> 8);

  if (temperature < 20.0f) {
    const float t2 = (d_t * d_t) / 2147483648.0f;
    const float temp20 = (temperature - 20.0f) * 100.0f;
    float pressure_offset_2 = 2.5f * temp20 * temp20;
    float pressure_sensitivity_2 = 1.25f * temp20 * temp20;
    if (temp20 < -15.0f) {
      const float temp15 = (temperature + 15.0f) * 100.0f;
      pressure_offset_2 += 7.0f * temp15;
      pressure_sensitivity_2 += 5.5f * temp15;
    }
    temperature -= t2;
    pressure_offset -= pressure_offset_2;
    pressure_sensitivity -= pressure_sensitivity_2;
  }

  const float pressure = ((raw_pressure * pressure_sensitivity) / 2097152.0f - pressure_offset) / 3276800.0f;

  ESP_LOGD(TAG, "Got temperature=%0.02f°C pressure=%0.01fhPa", temperature, pressure);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure);  // hPa
  this->status_clear_warning();
}

}  // namespace ms5611
}  // namespace esphome
