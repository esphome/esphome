#include "ee895.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ee895 {

static const char *const TAG = "ee895";

static const uint16_t CRC16_ONEWIRE_START = 0xFFFF;
static const uint8_t FUNCTION_CODE_READ = 0x03;
static const uint16_t SERIAL_NUMBER = 0x0000;
static const uint16_t TEMPERATURE_ADDRESS = 0x03EA;
static const uint16_t CO2_ADDRESS = 0x0424;
static const uint16_t PRESSURE_ADDRESS = 0x04B0;

void EE895Component::setup() {
  uint16_t crc16_check = 0;
  ESP_LOGCONFIG(TAG, "Setting up EE895...");
  write_command_(SERIAL_NUMBER, 8);
  uint8_t serial_number[20];
  this->read(serial_number, 20);

  crc16_check = (serial_number[19] << 8) + serial_number[18];
  if (crc16_check != calc_crc16_(serial_number, 19)) {
    this->error_code_ = CRC_CHECK_FAILED;
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "    Serial Number: 0x%s", format_hex(serial_number + 2, 16).c_str());
}

void EE895Component::dump_config() {
  ESP_LOGCONFIG(TAG, "EE895:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with EE895 failed!");
      break;
    case CRC_CHECK_FAILED:
      ESP_LOGE(TAG, "The crc check failed");
      break;
    case NONE:
    default:
      break;
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
}

float EE895Component::get_setup_priority() const { return setup_priority::DATA; }

void EE895Component::update() {
  write_command_(TEMPERATURE_ADDRESS, 2);
  this->set_timeout(50, [this]() {
    float temperature = read_float_();

    write_command_(CO2_ADDRESS, 2);
    float co2 = read_float_();

    write_command_(PRESSURE_ADDRESS, 2);
    float pressure = read_float_();
    ESP_LOGD(TAG, "Got temperature=%.1f°C co2=%.0fppm pressure=%.1f%mbar", temperature, co2, pressure);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(co2);
    if (this->pressure_sensor_ != nullptr)
      this->pressure_sensor_->publish_state(pressure);
    this->status_clear_warning();
  });
}

void EE895Component::write_command_(uint16_t addr, uint16_t reg_cnt) {
  uint8_t address[7];
  uint16_t crc16 = 0;
  address[0] = FUNCTION_CODE_READ;
  address[1] = (addr >> 8) & 0xFF;
  address[2] = addr & 0xFF;
  address[3] = (reg_cnt >> 8) & 0xFF;
  address[4] = reg_cnt & 0xFF;
  crc16 = calc_crc16_(address, 6);
  address[5] = crc16 & 0xFF;
  address[6] = (crc16 >> 8) & 0xFF;
  this->write(address, 7, true);
}

float EE895Component::read_float_() {
  uint16_t crc16_check = 0;
  uint8_t i2c_response[8];
  this->read(i2c_response, 8);
  crc16_check = (i2c_response[7] << 8) + i2c_response[6];
  if (crc16_check != calc_crc16_(i2c_response, 7)) {
    this->error_code_ = CRC_CHECK_FAILED;
    this->status_set_warning();
    return 0;
  }
  uint32_t x = encode_uint32(i2c_response[4], i2c_response[5], i2c_response[2], i2c_response[3]);
  float value;
  memcpy(&value, &x, sizeof(value));  // convert uin32_t IEEE-754 format to float
  return value;
}

uint16_t EE895Component::calc_crc16_(const uint8_t buf[], uint8_t len) {
  uint8_t crc_check_buf[22];
  for (int i = 0; i < len; i++) {
    crc_check_buf[i + 1] = buf[i];
  }
  crc_check_buf[0] = this->address_;
  return crc16(crc_check_buf, len);
}
}  // namespace ee895
}  // namespace esphome
