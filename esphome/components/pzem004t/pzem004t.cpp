#include "pzem004t.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cinttypes>

namespace esphome::pzem004t {

ESPHOME_LOG_TAG(TAG, "pzem004t");

void PZEM004T::setup() {
  this->discard_available();
  // Set module address
  this->write_state_(SET_ADDRESS);
}

void PZEM004T::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_read_ > 500 && this->available() < 7) {
    this->discard_available();
    this->last_read_ = now;
  }

  // PZEM004T packet size is 7 byte
  while (this->available() >= 7) {
    auto resp_opt = this->read_array<7>();
    if (!resp_opt.has_value())
      break;
    auto resp = *resp_opt;
    // packet format:
    // 0: packet type
    // 1-5: data
    // 6: checksum (sum of other bytes)
    // see https://github.com/olehs/PZEM004T
    const uint8_t sum = sum_bytes(resp.data(), 6);

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
        uint16_t int_voltage = encode_uint16(resp[1], resp[2]);
        float voltage = int_voltage + (resp[3] / 10.0f);
        PUBLISH_STATE(this->voltage_sensor_, voltage);
        ESP_LOGD(TAG, "Got Voltage %.1f V", voltage);
        this->write_state_(READ_CURRENT);
        break;
      }
      case 0xA1: {  // Current Response
        uint16_t int_current = encode_uint16(resp[1], resp[2]);
        float current = int_current + (resp[3] / 100.0f);
        PUBLISH_STATE(this->current_sensor_, current);
        ESP_LOGD(TAG, "Got Current %.2f A", current);
        this->write_state_(READ_POWER);
        break;
      }
      case 0xA2: {  // Active Power Response
        uint16_t power = encode_uint16(resp[1], resp[2]);
        PUBLISH_STATE(this->power_sensor_, power);
        ESP_LOGD(TAG, "Got Power %u W", power);
        this->write_state_(READ_ENERGY);
        break;
      }

      case 0xA3: {  // Energy Response
        uint32_t energy = encode_uint24(resp[1], resp[2], resp[3]);
        PUBLISH_STATE(this->energy_sensor_, energy);
        ESP_LOGD(TAG, "Got Energy %" PRIu32 " Wh", energy);
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
  data[6] = sum_bytes(data.data(), 6);

  this->write_array(data);
  this->read_state_ = state;
}
void PZEM004T::dump_config() {
  ESP_LOGCONFIG(TAG, "PZEM004T:");
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
}

}  // namespace esphome::pzem004t
