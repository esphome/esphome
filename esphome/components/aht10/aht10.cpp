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
#include "esphome/core/hal.h"

namespace esphome {
namespace aht10 {

static const char *const TAG = "aht10";
static const uint8_t AHT10_INITIALIZE_CMD[] = {0xE1, 0x08, 0x00};
static const uint8_t AHT20_INITIALIZE_CMD[] = {0xBE, 0x08, 0x00};
static const uint8_t AHT10_MEASURE_CMD[] = {0xAC, 0x33, 0x00};
static const uint8_t AHT10_SOFTRESET_CMD[] = {0xBA};

static const uint8_t AHT10_DEFAULT_DELAY = 5;     // ms, for initialization and temperature measurement
static const uint8_t AHT10_READ_DELAY = 80;       // ms, time to wait for conversion result
static const uint8_t AHT10_SOFTRESET_DELAY = 30;  // ms

static const uint8_t AHT10_ATTEMPTS = 3;  // safety margin, normally 3 attempts are enough: 3*30=90ms
static const uint8_t AHT10_INIT_ATTEMPTS = 10;

static const uint8_t AHT10_STATUS_BUSY = 0x80;

void AHT10Component::setup() {
  if (this->write(AHT10_SOFTRESET_CMD, sizeof(AHT10_SOFTRESET_CMD)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Reset AHT10 failed!");
  }
  delay(AHT10_SOFTRESET_DELAY);

  i2c::ErrorCode error_code = i2c::ERROR_INVALID_ARGUMENT;
  switch (this->variant_) {
    case AHT10Variant::AHT20:
      ESP_LOGCONFIG(TAG, "Setting up AHT20");
      error_code = this->write(AHT20_INITIALIZE_CMD, sizeof(AHT20_INITIALIZE_CMD));
      break;
    case AHT10Variant::AHT10:
      ESP_LOGCONFIG(TAG, "Setting up AHT10");
      error_code = this->write(AHT10_INITIALIZE_CMD, sizeof(AHT10_INITIALIZE_CMD));
      break;
  }
  if (error_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with AHT10 failed!");
    this->mark_failed();
    return;
  }
  uint8_t data = AHT10_STATUS_BUSY;
  int cal_attempts = 0;
  while (data & AHT10_STATUS_BUSY) {
    delay(AHT10_DEFAULT_DELAY);
    if (this->read(&data, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Communication with AHT10 failed!");
      this->mark_failed();
      return;
    }
    ++cal_attempts;
    if (cal_attempts > AHT10_INIT_ATTEMPTS) {
      ESP_LOGE(TAG, "AHT10 initialization timed out!");
      this->mark_failed();
      return;
    }
  }
  if ((data & 0x68) != 0x08) {  // Bit[6:5] = 0b00, NORMAL mode and Bit[3] = 0b1, CALIBRATED
    ESP_LOGE(TAG, "AHT10 initialization failed!");
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "AHT10 initialization");
}

void AHT10Component::restart_read_() {
  if (this->read_count_ == AHT10_ATTEMPTS) {
    this->read_count_ = 0;
    this->status_set_error("Measurements reading timed-out!");
    return;
  }
  this->read_count_++;
  this->set_timeout(AHT10_READ_DELAY, [this]() { this->read_data_(); });
}

void AHT10Component::read_data_() {
  uint8_t data[6];
  if (this->read_count_ > 1)
    ESP_LOGD(TAG, "Read attempt %d at %ums", this->read_count_, (unsigned) (millis() - this->start_time_));
  if (this->read(data, 6) != i2c::ERROR_OK) {
    this->status_set_warning("AHT10 read failed, retrying soon");
    this->restart_read_();
    return;
  }

  if ((data[0] & 0x80) == 0x80) {  // Bit[7] = 0b1, device is busy
    ESP_LOGD(TAG, "AHT10 is busy, waiting...");
    this->restart_read_();
    return;
  }
  if (data[1] == 0x0 && data[2] == 0x0 && (data[3] >> 4) == 0x0) {
    // Unrealistic humidity (0x0)
    if (this->humidity_sensor_ == nullptr) {
      ESP_LOGV(TAG, "ATH10 Unrealistic humidity (0x0), but humidity is not required");
    } else {
      ESP_LOGD(TAG, "ATH10 Unrealistic humidity (0x0), retrying...");
      if (this->write(AHT10_MEASURE_CMD, sizeof(AHT10_MEASURE_CMD)) != i2c::ERROR_OK) {
        this->status_set_warning("Communication with AHT10 failed!");
      }
      this->restart_read_();
      return;
    }
  }
  if (this->read_count_ > 1)
    ESP_LOGD(TAG, "Success at %ums", (unsigned) (millis() - this->start_time_));
  uint32_t raw_temperature = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
  uint32_t raw_humidity = ((data[1] << 16) | (data[2] << 8) | data[3]) >> 4;

  if (this->temperature_sensor_ != nullptr) {
    float temperature = ((200.0f * (float) raw_temperature) / 1048576.0f) - 50.0f;
    this->temperature_sensor_->publish_state(temperature);
  }
  if (this->humidity_sensor_ != nullptr) {
    float humidity;
    if (raw_humidity == 0) {  // unrealistic value
      humidity = NAN;
    } else {
      humidity = (float) raw_humidity * 100.0f / 1048576.0f;
    }
    if (std::isnan(humidity)) {
      ESP_LOGW(TAG, "Invalid humidity! Sensor reported 0%% Hum");
    }
    this->humidity_sensor_->publish_state(humidity);
  }
  this->status_clear_warning();
  this->read_count_ = 0;
}
void AHT10Component::update() {
  if (this->read_count_ != 0)
    return;
  this->start_time_ = millis();
  if (this->write(AHT10_MEASURE_CMD, sizeof(AHT10_MEASURE_CMD)) != i2c::ERROR_OK) {
    this->status_set_warning("Communication with AHT10 failed!");
    return;
  }
  this->restart_read_();
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
