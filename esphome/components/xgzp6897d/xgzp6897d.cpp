#include "xgzp6897d.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace xgzp6897d {

static const char *const TAG = "xgzp6897d.sensor";

static const uint8_t XGZP6897D_REGISTER_DATA = 0x06;
static const uint8_t XGZP6897D_REGISTER_CMD = 0x30;
static const uint8_t XGZP6897D_REGISTER_SYSCONFIG = 0xA5;
static const uint8_t XGZP6897D_REGISTER_PCONFIG = 0xA6;

static const char *oversampling_to_str(XGZP6897DOversampling oversampling) {
  switch (oversampling) {
    case XGZP6897D_OVERSAMPLING_256X:
      return "256x";
    case XGZP6897D_OVERSAMPLING_512X:
      return "512x";
    case XGZP6897D_OVERSAMPLING_1024X:
      return "1024x";
    case XGZP6897D_OVERSAMPLING_2048X:
      return "2048x";
    case XGZP6897D_OVERSAMPLING_4096X:
      return "4096x";
    case XGZP6897D_OVERSAMPLING_8192X:
      return "8192x";
    case XGZP6897D_OVERSAMPLING_16384X:
      return "16384x";
    case XGZP6897D_OVERSAMPLING_32768X:
      return "32768x";
    default:
      return "UNKNOWN";
  }
}

static const char *kvalue_to_str(uint16_t kvalue) {
  switch (kvalue) {
    case 32:
      return "131 < kPa <= 260";
    case 64:
      return "65 < kPa <= 131";
    case 128:
      return "32 < kPa <= 65";
    case 256:
      return "16 < kPa <= 32";
    case 512:
      return "8 < kPa <= 16";
    case 1024:
      return "4 < kPa <= 8";
    case 2048:
      return "2 <= kPa <= 4";
    case 4096:
      return "1 <= kPa < 2";
    case 8192:
      return "kPa < 1";
    default:
      return "UNKNOWN";
  }
}

void XGZP6897DComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up XGZP6897D...");

  uint8_t pconfig;
  if (!this->read_byte(XGZP6897D_REGISTER_PCONFIG, &pconfig)) {
    this->mark_failed();
    return;
  }

  pconfig &= ~0b00000111;
  pconfig |= this->oversampling_;

  if (!this->write_byte(XGZP6897D_REGISTER_PCONFIG, pconfig)) {
    this->mark_failed();
    return;
  }

  if (this->continuous_mode_) {
    // Set continuous mode
    uint8_t cmd = 0b1011;

    cmd |= ((sleep_time_ & 0b1111) << 4);

    if (!this->write_byte(XGZP6897D_REGISTER_CMD, cmd)) {
      this->mark_failed();
      return;
    }
  }
}
void XGZP6897DComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "XGZP6897D:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with XGZP6897D failed!");
      break;
    case NONE:
    default:
      break;
  }
  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG, "  Pressure Range: %s", kvalue_to_str(this->kvalue_));
  ESP_LOGCONFIG(TAG, "  Continues Mode: %s (Sleep Time: %.1fms)", (this->continuous_mode_ ? "true" : "false"),
                (sleep_time_ * 62.5f));
  ESP_LOGCONFIG(TAG, "  Oversampling: %s", oversampling_to_str(this->oversampling_));
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}
float XGZP6897DComponent::get_setup_priority() const { return setup_priority::DATA; }

void XGZP6897DComponent::update() {
  if (!this->continuous_mode_) {
    ESP_LOGV(TAG, "Sending conversion request...");
    uint8_t cmd = 0b1010;
    if (!this->write_byte(XGZP6897D_REGISTER_CMD, cmd)) {
      this->status_set_warning();
      return;
    }

    uint32_t start = millis();
    while (this->read_byte(XGZP6897D_REGISTER_CMD, &cmd) && (cmd & 0b1000) > 0) {
      if (millis() - start > 100) {
        ESP_LOGW(TAG, "Reading XGZP6897D timed out");
        this->status_set_warning();
        return;
      }
      delay(1);
    }
    ESP_LOGV(TAG, "Conversion took %dms.", millis() - start);
  }

  uint8_t data[5];
  if (!this->read_bytes(XGZP6897D_REGISTER_DATA, data, 5)) {
    ESP_LOGW(TAG, "Error reading registers.");
    this->status_set_warning();
    return;
  }

  float pressure = this->read_pressure_(data);
  float temperature = this->read_temperature_(data);

  ESP_LOGV(TAG, "Got pressure=%.2fhPa temperature=%.1f°C", pressure, temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  this->status_clear_warning();
}

float XGZP6897DComponent::read_pressure_(const uint8_t *data) {
  int32_t pressure = (data[0] << 16) | (data[1] << 8) | data[2];

  if (pressure & (1 << 23))
    pressure = pressure - (1 << 24);

  return ((float) pressure / (float) kvalue_) / 100.0f;
}

float XGZP6897DComponent::read_temperature_(const uint8_t *data) {
  int32_t temperature = (data[3] << 8) | data[4];

  if (temperature & (1 << 15))
    temperature = temperature - (1 << 16);

  return (float) temperature / 256.0f;
}

}  // namespace xgzp6897d
}  // namespace esphome
