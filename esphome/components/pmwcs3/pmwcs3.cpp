#include "pmwcs3.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pmwcs3 {

static const uint8_t PMWCS3_I2C_ADDRESS = 0x63;
static const uint8_t PMWCS3_REG_READ_START = 0x01;
static const uint8_t PMWCS3_REG_READ_E25 = 0x02;
static const uint8_t PMWCS3_REG_READ_EC = 0x03;
static const uint8_t PMWCS3_REG_READ_TEMP = 0x04;
static const uint8_t PMWCS3_REG_READ_VWC = 0x05;
static const uint8_t PMWCS3_REG_CALIBRATE_AIR = 0x06;
static const uint8_t PMWCS3_REG_CALIBRATE_WATER = 0x07;
static const uint8_t PMWCS3_SET_I2C_ADDRESS = 0x08;
static const uint8_t PMWCS3_REG_GET_DATA = 0x09;
static const uint8_t PMWCS3_REG_CALIBRATE_EC = 0x10;
static const uint8_t PMWCS3_REG_CAP = 0x0A;
static const uint8_t PMWCS3_REG_RES = 0x0B;
static const uint8_t PMWCS3_REG_RC = 0x0C;
static const uint8_t PMWCS3_REG_RT = 0x0D;

static const char *const TAG = "pmwcs3";

void PMWCS3Component::new_i2c_address(uint8_t address) {
  if (!this->write_byte(PMWCS3_SET_I2C_ADDRESS, address)) {
    this->status_set_warning();
    ESP_LOGW(TAG, "couldn't write the new I2C address %d", address);
    return;
  }
  this->set_i2c_address(address);  // Allows device to continue working until new firmware is written with new address.
  ESP_LOGVV(TAG, "changed I2C address to %d", address);
  this->status_clear_warning();
}

void PMWCS3Component::air_calibration() {
  if (!this->write_bytes(PMWCS3_REG_CALIBRATE_AIR, nullptr, 0)) {
    this->status_set_warning();
    ESP_LOGW(TAG, "couldn't start air calibration");
    return;
  }
  ESP_LOGW(TAG, "Start air calibration during the next 300s");
}
void PMWCS3Component::water_calibration() {
  if (!this->write_bytes(PMWCS3_REG_CALIBRATE_WATER, nullptr, 0)) {
    this->status_set_warning();
    ESP_LOGW(TAG, "couldn't start water calibration");
    return;
  }
  ESP_LOGW(TAG, "Start water calibration during the next 300s");
}

void PMWCS3Component::setup() { ESP_LOGCONFIG(TAG, "Setting up PMWCS3..."); }

void PMWCS3Component::update() { this->read_data_(); }

float PMWCS3Component::get_setup_priority() const { return setup_priority::DATA; }

void PMWCS3Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PMWCS3");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with PMWCS3 failed!");
  }
  ESP_LOGI(TAG, "%s", this->is_failed() ? "FAILED" : "OK");

  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "e25", this->e25_sensor_);
  LOG_SENSOR("  ", "ec", this->ec_sensor_);
  LOG_SENSOR("  ", "temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "vwc", this->vwc_sensor_);
}
void PMWCS3Component::read_data_() {
  uint8_t data[8];
  float e25, ec, temperature, vwc;

  /////// Super important !!!! first activate reading PMWCS3_REG_READ_START (if not, return always the same values) ////

  if (!this->write_bytes(PMWCS3_REG_READ_START, nullptr, 0)) {
    this->status_set_warning();
    ESP_LOGVV(TAG, "Failed to write into REG_READ_START register !!!");
    return;
  }
  // NOLINT  delay(100);

  if (!this->read_bytes(PMWCS3_REG_GET_DATA, (uint8_t *) &data, 8)) {
    ESP_LOGVV(TAG, "Error reading PMWCS3_REG_GET_DATA registers");
    this->mark_failed();
    return;
  }
  if (this->e25_sensor_ != nullptr) {
    e25 = ((data[1] << 8) | data[0]) / 100.0;
    this->e25_sensor_->publish_state(e25);
    ESP_LOGVV(TAG, "e25: data[0]=%d, data[1]=%d, result=%f", data[0], data[1], e25);
  }
  if (this->ec_sensor_ != nullptr) {
    ec = ((data[3] << 8) | data[2]) / 10.0;
    this->ec_sensor_->publish_state(ec);
    ESP_LOGVV(TAG, "ec: data[2]=%d, data[3]=%d, result=%f", data[2], data[3], ec);
  }
  if (this->temperature_sensor_ != nullptr) {
    temperature = ((data[5] << 8) | data[4]) / 100.0;
    this->temperature_sensor_->publish_state(temperature);
    ESP_LOGVV(TAG, "temp: data[4]=%d, data[5]=%d, result=%f", data[4], data[5], temperature);
  }
  if (this->vwc_sensor_ != nullptr) {
    vwc = ((data[7] << 8) | data[6]) / 10.0;
    this->vwc_sensor_->publish_state(vwc);
    ESP_LOGVV(TAG, "vwc: data[6]=%d, data[7]=%d, result=%f", data[6], data[7], vwc);
  }
}

}  // namespace pmwcs3
}  // namespace esphome
