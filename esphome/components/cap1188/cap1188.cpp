#include "cap1188.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace cap1188 {

static const char *const TAG = "cap1188";

void CAP1188Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CAP1188...");

  // Reset device using the reset pin
  this->turn_off();
  this->pin_->setup();
  this->turn_off();
  delay(100);  // NOLINT
  this->turn_on();
  delay(100);  // NOLINT
  this->turn_off();
  delay(100);  // NOLINT

  // Check if CAP1188 is actually connected
  uint8_t cap1188_product_id = 0;
  uint8_t cap1188_manufacture_id = 0;
  uint8_t cap1188_revision = 0;

  this->read_byte(CAP1188_PRODID, &cap1188_product_id);
  this->read_byte(CAP1188_MANUID, &cap1188_manufacture_id);
  this->read_byte(CAP1188_REV, &cap1188_revision);

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
  this->write_byte(CAP1188_MULTITOUCH, allow_multiple_touches_);

  // Have LEDs follow touches
  this->write_byte(CAP1188_LEDLINK, 0xFF);

  // Speed up a bit
  this->write_byte(CAP1188_STANDBYCFG, 0x30);
}

void CAP1188Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CAP1188:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset Pin: ", this->pin_);
  LOG_BINARY_OUTPUT(this);
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

  this->read_register(CAP1188_SENINPUTSTATUS, &touched, 1);

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
