#include "ee895.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ee895 {

static const char *const TAG = "ee895";

void EE895Component::setup() {
  uint16_t crc16_check = 0;
  ESP_LOGCONFIG(TAG, "Setting up EE895...");
  uint8_t address[] = {0x03, 0x00, 0x00, 0x00, 0x08, 0x49, 0x72};
  this->write(address, 7, false);
  uint8_t SerialNumber[20];
  this->read(SerialNumber, 20);

  crc16_check = (SerialNumber[19] << 8) + SerialNumber[18];
  if (crc16_check != calcCrc16(SerialNumber, 19)) {
    this->error_code_ = CRC_CHECK_FAILED;
    this->mark_failed();
    return;
  }
  uint32_t serialNumber1 = (SerialNumber[2] << 24) + (SerialNumber[3] << 16) + (SerialNumber[4] << 8) + SerialNumber[5];
  uint32_t serialNumber2 = (SerialNumber[6] << 24) + (SerialNumber[7] << 16) + (SerialNumber[8] << 8) + SerialNumber[9];
  uint32_t serialNumber3 =
      (SerialNumber[10] << 24) + (SerialNumber[11] << 16) + (SerialNumber[12] << 8) + SerialNumber[13];
  uint32_t serialNumber4 =
      (SerialNumber[14] << 24) + (SerialNumber[15] << 16) + (SerialNumber[16] << 8) + SerialNumber[17];
  ESP_LOGV(TAG, "    Serial Number: 0x%08X%08X%08X%08X", serialNumber1, serialNumber2, serialNumber3, serialNumber4);
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
  uint8_t address_1[] = {0x03, 0x03, 0xEA, 0x00, 0x02, 0xE8, 0xC5};
  this->write(address_1, 7, true);
  this->set_timeout(50, [this]() {
    uint16_t crc16_check = 0;
    uint8_t i2cResponse[8];
    this->read(i2cResponse, 8);
    crc16_check = (i2cResponse[7] << 8) + i2cResponse[6];
    if (crc16_check != calcCrc16(i2cResponse, 7)) {
      this->error_code_ = CRC_CHECK_FAILED;
      this->status_set_warning();
      return;
    }
    uint32_t x = i2cResponse[4] * 16777216 + i2cResponse[5] * 65536 + i2cResponse[2] * 256 + i2cResponse[3];
    float temperature = *(float *) &x;

    uint8_t address_2[] = {0x03, 0x04, 0x24, 0x00, 0x02, 0x88, 0x4E};
    this->write(address_2, 7, true);
    this->read(i2cResponse, 8);
    crc16_check = (i2cResponse[7] << 8) + i2cResponse[6];
    if (crc16_check != calcCrc16(i2cResponse, 7)) {
      this->error_code_ = CRC_CHECK_FAILED;
      this->status_set_warning();
      return;
    }
    x = i2cResponse[4] * 16777216 + i2cResponse[5] * 65536 + i2cResponse[2] * 256 + i2cResponse[3];
    float co2 = *(float *) &x;
    uint8_t address_3[] = {0x03, 0x04, 0xB0, 0x00, 0x02, 0xC9, 0xA2};
    this->write(address_3, 7, true);
    this->read(i2cResponse, 8);
    crc16_check = (i2cResponse[7] << 8) + i2cResponse[6];
    if (crc16_check != calcCrc16(i2cResponse, 7)) {
      this->error_code_ = CRC_CHECK_FAILED;
      this->status_set_warning();
      return;
    }
    x = i2cResponse[4] * 16777216 + i2cResponse[5] * 65536 + i2cResponse[2] * 256 + i2cResponse[3];
    float pressure = *(float *) &x;
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

uint16_t EE895Component::calcCrc16(unsigned char buf[], unsigned char len) {
  uint16_t crc = CRC16_ONEWIRE_START;
  unsigned char i;
  unsigned char j;
  unsigned char crcCheckBuf[22];
  for (i = 0; i < len; i++) {
    crcCheckBuf[i + 1] = buf[i];
  }
  crcCheckBuf[0] = 0x5F;

  for (i = 0; i < len; i++) {
    crc ^= (uint16_t) crcCheckBuf[i];  // XOR byte into least sig. byte of crc
    for (j = 8; j != 0; j--) {         // Loop over each bit
      if ((crc & 0x0001) != 0)         // If the LSB is set
      {
        crc >>= 1;  // Shift right and XOR 0xA001
        crc ^= 0xA001;
      } else  // Else LSB is not set
      {
        crc >>= 1;  // Just shift right
      }
    }
  }
  return crc;
}
}  // namespace ee895
}  // namespace esphome
