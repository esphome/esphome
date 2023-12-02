#include "tof10120_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

// Very basic support for TOF10120 distance sensor

namespace esphome {
namespace tof10120 {

static const char *const TAG = "tof10120";
static const uint8_t TOF10120_READ_DISTANCE_CMD[] = {0x00};
static const uint8_t TOF10120_DEFAULT_DELAY = 30;

static const uint8_t TOF10120_DIR_SEND_REGISTER = 0x0e;
static const uint8_t TOF10120_DISTANCE_REGISTER = 0x00;

static const uint16_t TOF10120_OUT_OF_RANGE_VALUE = 2000;

void TOF10120Sensor::dump_config() {
  LOG_SENSOR("", "TOF10120", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
}

void TOF10120Sensor::setup() {}

void TOF10120Sensor::update() {
  if (!this->write_bytes(TOF10120_DISTANCE_REGISTER, TOF10120_READ_DISTANCE_CMD, sizeof(TOF10120_READ_DISTANCE_CMD))) {
    ESP_LOGE(TAG, "Communication with TOF10120 failed on write");
    this->status_set_warning();
    return;
  }

  uint8_t data[2];
  if (this->write(&TOF10120_DISTANCE_REGISTER, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  delay(TOF10120_DEFAULT_DELAY);
  if (this->read(data, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with TOF10120 failed on read");
    this->status_set_warning();
    return;
  }

  uint32_t distance_mm = (data[0] << 8) | data[1];
  ESP_LOGI(TAG, "Data read: %" PRIu32 "mm", distance_mm);

  if (distance_mm == TOF10120_OUT_OF_RANGE_VALUE) {
    ESP_LOGW(TAG, "Distance measurement out of range");
    this->publish_state(NAN);
  } else {
    this->publish_state(distance_mm / 1000.0f);
  }
  this->status_clear_warning();
}

}  // namespace tof10120
}  // namespace esphome
