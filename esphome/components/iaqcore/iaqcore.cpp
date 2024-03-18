#include "iaqcore.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace iaqcore {

static const char *const TAG = "iaqcore";

enum IAQCoreErrorCode : uint8_t { ERROR_OK = 0, ERROR_RUNIN = 0x10, ERROR_BUSY = 0x01, ERROR_ERROR = 0x80 };

struct SensorData {
  uint16_t co2;
  IAQCoreErrorCode status;
  int32_t resistance;
  uint16_t tvoc;

  SensorData(const uint8_t *buffer) {
    this->co2 = encode_uint16(buffer[0], buffer[1]);
    this->status = static_cast<IAQCoreErrorCode>(buffer[2]);
    this->resistance = encode_uint32(buffer[3], buffer[4], buffer[5], buffer[6]);
    this->tvoc = encode_uint16(buffer[7], buffer[8]);
  }
};

void IAQCore::setup() {
  if (this->write(nullptr, 0) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Communication failed!");
    this->mark_failed();
    return;
  }
}

void IAQCore::update() {
  uint8_t buffer[sizeof(SensorData)];

  if (this->read_register(0xB5, buffer, sizeof(buffer), false) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Read failed");
    this->status_set_warning();
    this->publish_nans_();
    return;
  }

  SensorData data(buffer);

  switch (data.status) {
    case ERROR_OK:
      ESP_LOGD(TAG, "OK");
      break;
    case ERROR_RUNIN:
      ESP_LOGI(TAG, "Warming up");
      break;
    case ERROR_BUSY:
      ESP_LOGI(TAG, "Busy");
      break;
    case ERROR_ERROR:
      ESP_LOGE(TAG, "Error");
      break;
  }

  if (data.status != ERROR_OK) {
    this->status_set_warning();
    this->publish_nans_();
    return;
  }

  if (this->co2_ != nullptr) {
    this->co2_->publish_state(data.co2);
  }
  if (this->tvoc_ != nullptr) {
    this->tvoc_->publish_state(data.tvoc);
  }

  this->status_clear_warning();
}

void IAQCore::publish_nans_() {
  if (this->co2_ != nullptr) {
    this->co2_->publish_state(NAN);
  }
  if (this->tvoc_ != nullptr) {
    this->tvoc_->publish_state(NAN);
  }
}

void IAQCore::dump_config() {
  ESP_LOGCONFIG(TAG, "AMS iAQ Core:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AMS iAQ Core failed!");
  }
  LOG_SENSOR("  ", "CO2", this->co2_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_);
}

}  // namespace iaqcore
}  // namespace esphome
