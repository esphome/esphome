#include "af4991.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace af4991 {

static const char *const TAG = "af4991";

void AF4991::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AF4991...");

  // Software Reset the device
  this->write_8((uint16_t) adafruit_seesaw::SEESAW_STATUS_BASE << 8 | adafruit_seesaw::SEESAW_STATUS_SWRST,
                (uint8_t) 0xFF);

  delay(100);  // NOLINT

  // Check connected device matches the expected device
  this->read_32((uint16_t) adafruit_seesaw::SEESAW_STATUS_BASE << 8 | adafruit_seesaw::SEESAW_STATUS_VERSION,
                &product_version_);
  product_version_ = (product_version_ >> 16 & 0xFFFF);

  if (product_version_ != 4991) {
    this->error_code_ = INCORRECT_FIRMWARE_DETECTED;
    this->mark_failed();
    return;
  }
}

void AF4991::dump_config() {
  ESP_LOGCONFIG(TAG, "AF4991");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Product ID: %d", product_version_);

  switch (this->error_code_) {
    case INCORRECT_FIRMWARE_DETECTED:
      ESP_LOGE(TAG, "The expected firmware for the Adafruit 4991 I2C Encoder board is: 4991, but got: %d",
               product_version_);
    case NONE:
    default:
      break;
  }
}

}  // namespace af4991
}  // namespace esphome
