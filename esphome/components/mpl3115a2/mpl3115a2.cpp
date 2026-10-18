#include "mpl3115a2.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::mpl3115a2 {

ESPHOME_LOG_TAG(TAG, "mpl3115a2");

void MPL3115A2Component::setup() {
  if (!this->check_chip_id(this, MPL3115A2_WHOAMI, 0xC4))
    return;

  // reset
  this->write_byte(MPL3115A2_CTRL_REG1, MPL3115A2_CTRL_REG1_RST);
  delay(15);

  // enable data ready events for pressure/altitude and temperature
  this->write_byte(MPL3115A2_PT_DATA_CFG,
                   MPL3115A2_PT_DATA_CFG_TDEFE | MPL3115A2_PT_DATA_CFG_PDEFE | MPL3115A2_PT_DATA_CFG_DREM);
}

void MPL3115A2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MPL3115A2:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Pressure", this->pressure_);
  LOG_SENSOR("  ", "Altitude", this->altitude_);
}

void MPL3115A2Component::update() {
  uint8_t mode = MPL3115A2_CTRL_REG1_OS128;
  this->write_byte(MPL3115A2_CTRL_REG1, mode);
  // Trigger a new reading
  mode |= MPL3115A2_CTRL_REG1_OST;
  if (this->altitude_ != nullptr)
    mode |= MPL3115A2_CTRL_REG1_ALT;
  this->write_byte(MPL3115A2_CTRL_REG1, mode);

  // Wait until status shows reading available
  uint8_t status = 0;
  if (!this->read_byte(MPL3115A2_REGISTER_STATUS, &status) || (status & MPL3115A2_REGISTER_STATUS_PDR) == 0) {
    delay(10);
    if (!this->read_byte(MPL3115A2_REGISTER_STATUS, &status) || (status & MPL3115A2_REGISTER_STATUS_PDR) == 0) {
      return;
    }
  }

  uint8_t buffer[5] = {0, 0, 0, 0, 0};
  this->read_register(MPL3115A2_REGISTER_PRESSURE_MSB, buffer, 5);

  float altitude = 0, pressure = 0;
  if (this->altitude_ != nullptr) {
    int32_t alt = encode_uint32(buffer[0], buffer[1], buffer[2], 0);
    altitude = float(alt) / 65536.0f;
    this->altitude_->publish_state(altitude);
  } else {
    uint32_t p = encode_uint32(0, buffer[0], buffer[1], buffer[2]);
    pressure = float(p) / 6400.0f;
    if (this->pressure_ != nullptr)
      this->pressure_->publish_state(pressure);
  }
  int16_t t = encode_uint16(buffer[3], buffer[4]);
  float temperature = float(t) / 256.0f;
  if (this->temperature_ != nullptr)
    this->temperature_->publish_state(temperature);

  ESP_LOGD(TAG, "Got Temperature=%.1f°C Altitude=%.1f Pressure=%.1f", temperature, altitude, pressure);

  this->status_clear_warning();
}

}  // namespace esphome::mpl3115a2
