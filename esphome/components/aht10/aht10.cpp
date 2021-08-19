// Implementation based on:
//  - AHT10: https://github.com/Thinary/AHT10
//  - Official Datasheet (cn):
//  http://www.aosong.com/userfiles/files/media/aht10%E8%A7%84%E6%A0%BC%E4%B9%A6v1_1%EF%BC%8820191015%EF%BC%89.pdf
//  - Unofficial Translated Datasheet (en):
//  https://wiki.liutyi.info/download/attachments/30507639/Aosong_AHT10_en_draft_0c.pdf
//
// When configured for humidity, the log 'Components should block for at most 20-30ms in loop().' will be generated in
// verbose mode. This is due to technical specs of the sensor and can not be avoided.
//
// According to the datasheet, the component is supposed to respond in more than 75ms. In fact, it can answer almost
// immediately for temperature. But for humidity, it takes >90ms to get a valid data. From experience, we have best
// results making successive requests; the current implementation makes 3 attempts with a delay of 30ms each time.

#include "aht10.h"
#include "esphome/core/log.h"

namespace esphome {
namespace aht10 {

static const char *const TAG = "aht10";
static const uint8_t AHT10_CALIBRATE_CMD[] = {0xE1};
static const uint8_t AHT10_MEASURE_CMD[] = {0xAC, 0x33, 0x00};
static const uint8_t AHT10_DEFAULT_DELAY = 5;    // ms, for calibration and temperature measurement
static const uint8_t AHT10_HUMIDITY_DELAY = 30;  // ms
static const uint8_t AHT10_ATTEMPTS = 3;         // safety margin, normally 3 attempts are enough: 3*30=90ms

void AHT10Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AHT10...");

  if (!this->write_bytes(0, AHT10_CALIBRATE_CMD, sizeof(AHT10_CALIBRATE_CMD))) {
    ESP_LOGE(TAG, "Communication with AHT10 failed!");
    this->mark_failed();
    return;
  }
  uint8_t data;
  if (!this->read_byte(0, &data, AHT10_DEFAULT_DELAY)) {
    ESP_LOGD(TAG, "Communication with AHT10 failed!");
    this->mark_failed();
    return;
  }
  if ((data & 0x68) != 0x08) {  // Bit[6:5] = 0b00, NORMAL mode and Bit[3] = 0b1, CALIBRATED
    ESP_LOGE(TAG, "AHT10 calibration failed!");
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "AHT10 calibrated");
}

void AHT10Component::update() {
  if (!this->write_bytes(0, AHT10_MEASURE_CMD, sizeof(AHT10_MEASURE_CMD))) {
    ESP_LOGE(TAG, "Communication with AHT10 failed!");
    this->status_set_warning();
    return;
  }
  uint8_t data[6];
  uint8_t delay = AHT10_DEFAULT_DELAY;
  if (this->humidity_sensor_ != nullptr)
    delay = AHT10_HUMIDITY_DELAY;
  for (int i = 0; i < AHT10_ATTEMPTS; ++i) {
    ESP_LOGVV(TAG, "Attempt %u at %6ld", i, millis());
    delay_microseconds_accurate(4);
    if (!this->read_bytes(0, data, 6, delay)) {
      ESP_LOGD(TAG, "Communication with AHT10 failed, waiting...");
    } else if ((data[0] & 0x80) == 0x80) {  // Bit[7] = 0b1, device is busy
      ESP_LOGD(TAG, "AHT10 is busy, waiting...");
    } else if (data[1] == 0x0 && data[2] == 0x0 && (data[3] >> 4) == 0x0) {
      // Unrealistic humidity (0x0)
      if (this->humidity_sensor_ == nullptr) {
        ESP_LOGVV(TAG, "ATH10 Unrealistic humidity (0x0), but humidity is not required");
        break;
      } else {
        ESP_LOGD(TAG, "ATH10 Unrealistic humidity (0x0), retrying...");
        if (!this->write_bytes(0, AHT10_MEASURE_CMD, sizeof(AHT10_MEASURE_CMD))) {
          ESP_LOGE(TAG, "Communication with AHT10 failed!");
          this->status_set_warning();
          return;
        }
      }
    } else {
      // data is valid, we can break the loop
      ESP_LOGVV(TAG, "Answer at %6ld", millis());
      break;
    }
  }
  if ((data[0] & 0x80) == 0x80) {
    ESP_LOGE(TAG, "Measurements reading timed-out!");
    this->status_set_warning();
    return;
  }

  uint32_t raw_temperature = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
  uint32_t raw_humidity = ((data[1] << 16) | (data[2] << 8) | data[3]) >> 4;

  float temperature = ((200.0 * (float) raw_temperature) / 1048576.0) - 50.0;
  float humidity;
  if (raw_humidity == 0) {  // unrealistic value
    humidity = NAN;
  } else {
    humidity = (float) raw_humidity * 100.0 / 1048576.0;
  }

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature);
  }
  if (this->humidity_sensor_ != nullptr) {
    if (isnan(humidity))
      ESP_LOGW(TAG, "Invalid humidity! Sensor reported 0%% Hum");
    this->humidity_sensor_->publish_state(humidity);
  }
  this->status_clear_warning();
}

float AHT10Component::get_setup_priority() const { return setup_priority::DATA; }

void AHT10Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AHT10:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AHT10 failed!");
  }
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

}  // namespace aht10
}  // namespace esphome
