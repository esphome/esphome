#include "eeh210.h"
#include "esphome/core/log.h"

namespace esphome {
namespace eeh210 {

static const char *const TAG = "eeh210.sensor";

static const uint8_t EEH210_I2C_COMMAND_TRIGGER_T_RH_MEASURE_HOLD = 0xE1;
static const uint8_t EEH210_I2C_COMMAND_TRIGGER_T_MEASURE_HOLD = 0xE3;
static const uint8_t EEH210_I2C_COMMAND_TRIGGER_RH_MEASURE_HOLD = 0xE5;

static const uint8_t EEH210_I2C_COMMAND_TRIGGER_T_RH_MEASURE_NO_HOLD = 0xF1;
static const uint8_t EEH210_I2C_COMMAND_TRIGGER_T_MEASURE_NO_HOLD = 0xF3;
static const uint8_t EEH210_I2C_COMMAND_TRIGGER_RH_MEASURE_NO_HOLD = 0xF5;

static const uint8_t EEH210_I2C_COMMAND_WRITE_USER_REGISTER = 0xE6;
static const uint8_t EEH210_I2C_COMMAND_READ_USER_REGISTER = 0xFE;

void EEH210Component::setup() { ESP_LOGCONFIG(TAG, "Setting up EEH210..."); }

void EEH210Component::dump_config() {
  ESP_LOGCONFIG(TAG, "EEH210:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with EEH210 failed!");
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
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

float EEH210Component::get_setup_priority() const { return setup_priority::DATA; }
void EEH210Component::update() {
  uint8_t command[] = {EEH210_I2C_COMMAND_TRIGGER_T_RH_MEASURE_HOLD};
  this->write(command, 1, true);
  this->set_timeout(50, [this]() {
    uint8_t i2c_response[6];
    this->read(i2c_response, 6);

    unsigned int raw_temperature = (i2c_response[1] << 8) | (i2c_response[2] & 0b11111100);
    float temperature = -46.85 + 175.72 * (raw_temperature / 65536.0);

    unsigned int raw_humidity = (i2c_response[3] << 8) | (i2c_response[4] & 0b11111100);
    float relative_humidity = -6.0 + 125.0 * (raw_humidity / 65536.0);

    ESP_LOGD(TAG, "Got temperature=%.4f°C humidity=%.4f%%", temperature, relative_humidity);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(relative_humidity);
    this->status_clear_warning();
  });
}

unsigned char EEH210Component::calc_crc8_(const unsigned char buf[], unsigned char from, unsigned char to) {
  unsigned char crc_val = 0xFF;
  unsigned char i = 0;
  unsigned char j = 0;
  for (i = from; i <= to; i++) {
    int cur_val = buf[i];
    for (j = 0; j < 8; j++) {
      if (((crc_val ^ cur_val) & 0x80) != 0)  // If MSBs are not equal
      {
        crc_val = ((crc_val << 1) ^ 0x31);
      } else {
        crc_val = (crc_val << 1);
      }
      cur_val = cur_val << 1;
    }
  }
  return crc_val;
}
}  // namespace eeh210
}  // namespace esphome
