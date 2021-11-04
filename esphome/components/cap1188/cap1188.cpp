#include "cap1188.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace cap1188 {

static const char *const TAG = "cap1188";

void CAP1188Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CAP1188...");

  // Reset device using the reset pin
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->pin_mode(esphome::gpio::FLAG_OUTPUT);
    this->reset_pin_->digital_write(false);
    delay(100);  // NOLINT
    this->reset_pin_->digital_write(true);
    delay(100);  // NOLINT
    this->reset_pin_->digital_write(false);
    delay(100);  // NOLINT
  }

  // Check if CAP1188 is actually connected
  uint8_t cap1188_product_id = 0;
  uint8_t cap1188_manufacture_id = 0;
  uint8_t cap1188_revision = 0;

  this->read_byte(CAP1188_PRODUCT_ID, &cap1188_product_id);
  this->read_byte(CAP1188_MANUFACTURE_ID, &cap1188_manufacture_id);
  this->read_byte(CAP1188_REVISION, &cap1188_revision);

  if ((cap1188_product_id != 0x50) || (cap1188_manufacture_id != 0x5D) || (cap1188_revision != 0x83)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  // Set sensitivity
  uint8_t sensitivity = 0;
  this->read_byte(CAP1188_SENSITVITY, &sensitivity);
  sensitivity = sensitivity & 0x0f;
  this->write_byte(CAP1188_SENSITVITY, sensitivity | touch_threshold_);

  // Allow multiple touches
  this->write_byte(CAP1188_MULTI_TOUCH, allow_multiple_touches_);

  // Have LEDs follow touches
  this->write_byte(CAP1188_LED_LINK, 0xFF);

  // Speed up a bit
  this->write_byte(CAP1188_STAND_BY_CONFIGURATION, 0x30);
}

void CAP1188Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CAP1188:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with CAP1188 failed!");
      break;
    case NONE:
    default:
      break;
  }
}

void CAP1188Component::loop() {
  uint8_t touched = 0;

  this->read_register(CAP1188_SENSOR_INPUT_STATUS, &touched, 1);

  if (touched) {
    uint8_t data = 0;
    this->read_register(CAP1188_MAIN, &data, 1);
    data = data & ~CAP1188_MAIN_INT;

    this->write_register(CAP1188_MAIN, &data, 2);
  }

  for (auto *channel : this->channels_) {
    channel->process(touched);
  }
}

}  // namespace cap1188
}  // namespace esphome
