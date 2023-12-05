// Implementation based on:
//  - AHTXX: https://github.com/Thinary/AHTXX
//  - Official Datasheet (cn):
//  http://www.aosong.com/userfiles/files/media/aht10%E8%A7%84%E6%A0%BC%E4%B9%A6v1_1%EF%BC%8820191015%EF%BC%89.pdf
//  - Unofficial Translated Datasheet (en):
//  https://wiki.liutyi.info/download/attachments/30507639/Aosong_AHTXX_en_draft_0c.pdf
//
// When configured for humidity, the log 'Components should block for at most 20-30ms in loop().' will be generated in
// verbose mode. This is due to technical specs of the sensor and can not be avoided.
//
// According to the datasheet, the component is supposed to respond in more than 75ms. In fact, it can answer almost
// immediately for temperature. But for humidity, it takes >90ms to get a valid data. From experience, we have best
// results making successive requests; the current implementation makes 3 attempts with a delay of 30ms each time.

#include "ahtxx.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace ahtxx {

static const char *const TAG = "ahtxx";
static const uint8_t AHTXX_CALIBRATE_CMD[] = {
  0xE1,
  0xBE,
  0xBE,
};
static const char    AHTXX_NAME[] = { '1', '2', '3' };
static const uint8_t AHTXX_MEASURE_CMD[] = {0xAC, 0x33, 0x00};
static const uint8_t AHTXX_ATTEMPTS = 3;         // safety margin, normally 3 attempts are enough: 3*30=90ms
static const uint8_t AHTXX_MEASURE_DELAY = 80;   // ms
static const uint8_t AHTXX_DEFAULT_DELAY = 40;   // ms

int failMark = 0;

void AHTXXComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AHT%cX...", AHTXX_NAME[this->type_]);

  const uint8_t cal[3] = { AHTXX_CALIBRATE_CMD[this->type_], 0x08, 0x00 };

  if (this->write(cal, sizeof(cal)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with AHTXX failed 0!");
    failMark = 1;
//    this->mark_failed();
    return;
  }
  uint8_t data = 0;
  delay(AHTXX_DEFAULT_DELAY);
  if (this->read(&data, 1) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Communication with AHTXX failed 1!");
    failMark = 2;
    this->mark_failed();
    return;
  }
  if (this->read(&data, 1) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Communication with AHTXX failed 2!");
    failMark = 3;
    this->mark_failed();
    return;
  }
  if ((data & 0x68) != 0x08) {  // Bit[6:5] = 0b00, NORMAL mode and Bit[3] = 0b1, CALIBRATED
    ESP_LOGE(TAG, "AHTXX calibration failed!");
    failMark = 4;
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "AHTXX calibrated");
}

void AHTXXComponent::update() {
  if (this->write(AHTXX_MEASURE_CMD, sizeof(AHTXX_MEASURE_CMD)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with AHTXX failed!");
    const uint8_t cal[3] = { 0xBC, 0x08, 0x00 };
    if (this->write(cal, sizeof(cal)) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Reset of AHTXX failed!");
    }

    this->status_set_warning();
    return;
  }
  uint8_t data[6];
  uint8_t delay_ms = AHTXX_MEASURE_DELAY;
  bool success = false;
  for (int i = 0; i < AHTXX_ATTEMPTS; ++i) {
    ESP_LOGVV(TAG, "Attempt %d at %6" PRIu32, i, millis());
    delay(delay_ms);
    if (this->read(data, 6) != i2c::ERROR_OK) {
      ESP_LOGD(TAG, "Communication with AHTXX failed, waiting...");
      continue;
    }

    if ((data[0] & 0x80) == 0x80) {  // Bit[7] = 0b1, device is busy
      ESP_LOGD(TAG, "AHTXX is busy, waiting...");
    } else if (data[1] == 0x0 && data[2] == 0x0 && (data[3] >> 4) == 0x0) {
      // Unrealistic humidity (0x0)
      if (this->humidity_sensor_ == nullptr) {
        ESP_LOGVV(TAG, "ATH%c0 Unrealistic humidity (0x0), but humidity is not required", AHTXX_NAME[this->type_]);
        break;
      } else {
        ESP_LOGD(TAG, "ATH%c0 Unrealistic humidity (0x0), retrying...", AHTXX_NAME[this->type_]);
        if (!this->write_bytes(0, AHTXX_MEASURE_CMD, sizeof(AHTXX_MEASURE_CMD))) {
          ESP_LOGE(TAG, "Communication with AHTXX failed!");
          this->status_set_warning();
          return;
        }
      }
    } else {
      // data is valid, we can break the loop
      ESP_LOGVV(TAG, "Answer at %6" PRIu32, millis());
      success = true;
      break;
    }
  }
  if (!success || (data[0] & 0x80) == 0x80) {
    ESP_LOGE(TAG, "Measurements reading timed-out!");
    this->status_set_warning();
    return;
  }

  uint32_t raw_temperature = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
  uint32_t raw_humidity = ((data[1] << 16) | (data[2] << 8) | data[3]) >> 4;

  float temperature = ((200.0f * (float) raw_temperature) / 1048576.0f) - 50.0f;
  float humidity;
  if (raw_humidity == 0) {  // unrealistic value
    humidity = NAN;
  } else {
    humidity = (float) raw_humidity * 100.0f / 1048576.0f;
  }

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature);
  }
  if (this->humidity_sensor_ != nullptr) {
    if (std::isnan(humidity)) {
      ESP_LOGW(TAG, "Invalid humidity! Sensor reported 0%% Hum");
    }
    this->humidity_sensor_->publish_state(humidity);
  }
  this->status_clear_warning();
}

float AHTXXComponent::get_setup_priority() const { return setup_priority::DATA; }

void AHTXXComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "AHT%cX:", AHTXX_NAME[this->type_]);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AHTXX failed -> %d!", failMark);
  }
  ESP_LOGW(TAG, "Communication with AHTXX failmark -> %d!", failMark);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

}  // namespace ahtxx
}  // namespace esphome
