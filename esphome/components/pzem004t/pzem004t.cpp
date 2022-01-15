#include "pzem004t.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pzem004t {

static const char *const TAG = "pzem004t";

void PZEM004T::setup() {
  // Clear UART buffer
  while (this->available())
    this->read();
  // Set module address
  this->write_state_(SET_ADDRESS);
}

void PZEM004T::loop() {
  const uint32_t now = millis();
  if (now - this->last_read_ > 500 && this->available() < 7) {
    while (this->available())
      this->read();
    this->last_read_ = now;
  }

  // PZEM004T packet size is 7 byte
  while (this->available() >= 7) {
    auto resp = *this->read_array<7>();
    // packet format:
    // 0: packet type
    // 1-5: data
    // 6: checksum (sum of other bytes)
    // see https://github.com/olehs/PZEM004T
    uint8_t sum = 0;
    for (int i = 0; i < 6; i++)
      sum += resp[i];

    if (sum != resp[6]) {
      ESP_LOGV(TAG, "PZEM004T invalid checksum! 0x%02X != 0x%02X", sum, resp[6]);
      continue;
    }

    switch (resp[0]) {
      case 0xA4: {  // Set Module Address Response
        this->write_state_(READ_VOLTAGE);
        break;
      }
      case 0xA0: {  // Voltage Response
        uint16_t int_voltage = (uint16_t(resp[1]) << 8) | (uint16_t(resp[2]) << 0);
        float voltage = int_voltage + (resp[3] / 10.0f);
        if (this->voltage_sensor_ != nullptr)
          this->voltage_sensor_->publish_state(voltage);
        ESP_LOGD(TAG, "Got Voltage %.1f V", voltage);
        this->write_state_(READ_CURRENT);
        break;
      }
      case 0xA1: {  // Current Response
        uint16_t int_current = (uint16_t(resp[1]) << 8) | (uint16_t(resp[2]) << 0);
        float current = int_current + (resp[3] / 100.0f);
        if (this->current_sensor_ != nullptr)
          this->current_sensor_->publish_state(current);
        ESP_LOGD(TAG, "Got Current %.2f A", current);
        this->write_state_(READ_POWER);
        break;
      }
      case 0xA2: {  // Active Power Response
        uint16_t power = (uint16_t(resp[1]) << 8) | (uint16_t(resp[2]) << 0);
        if (this->power_sensor_ != nullptr)
          this->power_sensor_->publish_state(power);
        ESP_LOGD(TAG, "Got Power %u W", power);
        this->write_state_(READ_ENERGY);
        break;
      }

      case 0xA3: {  // Energy Response
        uint32_t energy = (uint32_t(resp[1]) << 16) | (uint32_t(resp[2]) << 8) | (uint32_t(resp[3]));
        if (this->energy_sensor_ != nullptr)
          this->energy_sensor_->publish_state(energy);
        ESP_LOGD(TAG, "Got Energy %u Wh", energy);
        this->write_state_(DONE);
        break;
      }

      case 0xA5:  // Set Power Alarm Response
      case 0xB0:  // Voltage Request
      case 0xB1:  // Current Request
      case 0xB2:  // Active Power Response
      case 0xB3:  // Energy Request
      case 0xB4:  // Set Module Address Request
      case 0xB5:  // Set Power Alarm Request
      default:
        break;
    }

    this->last_read_ = now;
  }
}
void PZEM004T::update() { this->write_state_(READ_VOLTAGE); }
void PZEM004T::write_state_(PZEM004T::PZEM004TReadState state) {
  if (state == DONE) {
    this->read_state_ = state;
    return;
  }
  std::array<uint8_t, 7> data{};
  data[0] = state;
  data[1] = 192;
  data[2] = 168;
  data[3] = 1;
  data[4] = 1;
  data[5] = 0;
  data[6] = 0;
  for (int i = 0; i < 6; i++)
    data[6] += data[i];

  this->write_array(data);
  this->read_state_ = state;
}
void PZEM004T::dump_config() {
  ESP_LOGCONFIG(TAG, "PZEM004T:");
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
}

}  // namespace pzem004t
}  // namespace esphome
