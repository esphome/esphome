#include "ms5611.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ms5611 {

static const char *const TAG = "ms5611";

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

// Calculations are taken from the datasheet which can be found here:
// https://www.te.com/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Data+Sheet%7FMS5611-01BA03%7FB3%7Fpdf%7FEnglish%7FENG_DS_MS5611-01BA03_B3.pdf%7FCAT-BLPS0036
// Sections PRESSURE AND TEMPERATURE CALCULATION and SECOND ORDER TEMPERATURE COMPENSATION
// Variable names below match variable names from the datasheet but lowercased
void MS5611Component::calculate_values_(uint32_t raw_temperature, uint32_t raw_pressure) {
  const uint32_t c1 = uint32_t(this->prom_[0]);
  const uint32_t c2 = uint32_t(this->prom_[1]);
  const uint16_t c3 = uint16_t(this->prom_[2]);
  const uint16_t c4 = uint16_t(this->prom_[3]);
  const int32_t c5 = int32_t(this->prom_[4]);
  const uint16_t c6 = uint16_t(this->prom_[5]);
  const uint32_t d1 = raw_pressure;
  const int32_t d2 = raw_temperature;

  // Promote dt to 64 bit here to make the math below cleaner
  const int64_t dt = d2 - (c5 << 8);
  int32_t temp = (2000 + ((dt * c6) >> 23));

  int64_t off = (c2 << 16) + ((dt * c4) >> 7);
  int64_t sens = (c1 << 15) + ((dt * c3) >> 8);

  if (temp < 2000) {
    const int32_t t2 = (dt * dt) >> 31;
    int32_t off2 = ((5 * (temp - 2000) * (temp - 2000)) >> 1);
    int32_t sens2 = ((5 * (temp - 2000) * (temp - 2000)) >> 2);
    if (temp < -1500) {
      off2 = (off2 + 7 * (temp + 1500) * (temp + 1500));
      sens2 = sens2 + ((11 * (temp + 1500) * (temp + 1500)) >> 1);
    }
    temp = temp - t2;
    off = off - off2;
    sens = sens - sens2;
  }

  // Here we multiply unsigned 32-bit by signed 64-bit using signed 64-bit math.
  // Possible ranges of D1 and SENS from the datasheet guarantee
  // that this multiplication does not overflow
  const int32_t p = ((((d1 * sens) >> 21) - off) >> 15);

  const float temperature = temp / 100.0f;
  const float pressure = p / 100.0f;
  ESP_LOGD(TAG, "Got temperature=%0.02f°C pressure=%0.01fhPa", temperature, pressure);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure);  // hPa
  this->status_clear_warning();
}

}  // namespace ms5611
}  // namespace esphome
