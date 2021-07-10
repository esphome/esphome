#include "bmp085.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bmp085 {

static const char *const TAG = "bmp085.sensor";

static const uint8_t BMP085_ADDRESS = 0x77;
static const uint8_t BMP085_REGISTER_AC1_H = 0xAA;
static const uint8_t BMP085_REGISTER_CONTROL = 0xF4;
static const uint8_t BMP085_REGISTER_DATA_MSB = 0xF6;
static const uint8_t BMP085_CONTROL_MODE_TEMPERATURE = 0x2E;
static const uint8_t BMP085_CONTROL_MODE_PRESSURE_3 = 0xF4;

void BMP085Component::update() {
  if (!this->set_mode_(BMP085_CONTROL_MODE_TEMPERATURE))
    return;

  this->set_timeout("temperature", 5, [this]() { this->read_temperature_(); });
}
void BMP085Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BMP085...");
  uint8_t data[22];
  if (!this->read_bytes(BMP085_REGISTER_AC1_H, data, 22)) {
    this->mark_failed();
    return;
  }

  // Load calibration
  this->calibration_.ac1 = ((data[0] & 0xFF) << 8) | (data[1] & 0xFF);
  this->calibration_.ac2 = ((data[2] & 0xFF) << 8) | (data[3] & 0xFF);
  this->calibration_.ac3 = ((data[4] & 0xFF) << 8) | (data[5] & 0xFF);
  this->calibration_.ac4 = ((data[6] & 0xFF) << 8) | (data[7] & 0xFF);
  this->calibration_.ac5 = ((data[8] & 0xFF) << 8) | (data[9] & 0xFF);
  this->calibration_.ac6 = ((data[10] & 0xFF) << 8) | (data[11] & 0xFF);
  this->calibration_.b1 = ((data[12] & 0xFF) << 8) | (data[13] & 0xFF);
  this->calibration_.b2 = ((data[14] & 0xFF) << 8) | (data[15] & 0xFF);
  this->calibration_.mb = ((data[16] & 0xFF) << 8) | (data[17] & 0xFF);
  this->calibration_.mc = ((data[18] & 0xFF) << 8) | (data[19] & 0xFF);
  this->calibration_.md = ((data[20] & 0xFF) << 8) | (data[21] & 0xFF);
}
void BMP085Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BMP085:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Connection with BMP085 failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Pressure", this->pressure_);
}

void BMP085Component::read_temperature_() {
  uint8_t buffer[2];
  // 0xF6
  if (!this->read_bytes(BMP085_REGISTER_DATA_MSB, buffer, 2)) {
    this->status_set_warning();
    return;
  }

  int32_t ut = ((buffer[0] & 0xFF) << 8) | (buffer[1] & 0xFF);
  if (ut == 0) {
    ESP_LOGW(TAG, "Invalid temperature!");
    this->status_set_warning();
    return;
  }

  double c5 = (pow(2, -15) / 160) * this->calibration_.ac5;
  double c6 = this->calibration_.ac6;
  double mc = (2048.0 / 25600.0) * this->calibration_.mc;
  double md = this->calibration_.md / 160.0;

  double a = c5 * (ut - c6);
  float temp = a + (mc / (a + md));

  this->calibration_.temp = temp;
  ESP_LOGD(TAG, "Got Temperature=%.1f °C", temp);
  if (this->temperature_ != nullptr)
    this->temperature_->publish_state(temp);
  this->status_clear_warning();

  if (!this->set_mode_(BMP085_CONTROL_MODE_PRESSURE_3)) {
    this->status_set_warning();
    return;
  }

  this->set_timeout("pressure", 26, [this]() { this->read_pressure_(); });
}
void BMP085Component::read_pressure_() {
  uint8_t buffer[3];
  if (!this->read_bytes(BMP085_REGISTER_DATA_MSB, buffer, 3)) {
    this->status_set_warning();
    return;
  }

  uint32_t value = (uint32_t(buffer[0]) << 16) | (uint32_t(buffer[1]) << 8) | uint32_t(buffer[0]);
  if ((value >> 5) == 0) {
    ESP_LOGW(TAG, "Invalid pressure!");
    this->status_set_warning();
    return;
  }

  double c3 = 160.0 * pow(2.0, -15.0) * this->calibration_.ac3;
  double c4 = pow(10.0, -3) * pow(2.0, -15.0) * this->calibration_.ac4;
  double b1 = pow(160.0, 2.0) * pow(2.0, -30.0) * this->calibration_.b1;
  double x0 = this->calibration_.ac1;
  double x1 = 160.0 * pow(2.0, -13.0) * this->calibration_.ac2;
  double x2 = pow(160.0, 2.0) * pow(2.0, -25.0) * this->calibration_.b2;
  double y0 = c4 * pow(2.0, 15.0);
  double y1 = c4 * c3;
  double y2 = c4 * b1;
  double p0 = (3791.0 - 8.0) / 1600.0;
  double p1 = 1.0 - 7357.0 * pow(2, -20);
  double p2 = 3038.0 * 100.0 * pow(2, -36);

  double p = value / 256.0;
  double s = this->calibration_.temp - 25.0;
  double x = (x2 * s * s) + (x1 * s) + x0;
  double y = (y2 * s * s) + (y1 * s) + y0;
  double z = (p - x) / y;
  float pressure = (p2 * z * z) + (p1 * z) + p0;

  ESP_LOGD(TAG, "Got Pressure=%.1f hPa", pressure);

  if (this->pressure_ != nullptr)
    this->pressure_->publish_state(pressure);
  this->status_clear_warning();
}
bool BMP085Component::set_mode_(uint8_t mode) {
  ESP_LOGV(TAG, "Setting mode to 0x%02X...", mode);
  return this->write_byte(BMP085_REGISTER_CONTROL, mode);
}
float BMP085Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace bmp085
}  // namespace esphome
