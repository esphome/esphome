#include "axs5106_touchscreen.h"

namespace esphome {
namespace axs5106 {

static const char *const TAG = "axs5106.touchscreen";

const uint8_t TOUCH_AXS5106_TOUCH_POINTS_REG = 0x01;
const uint8_t TOUCH_AXS5106_TOUCH_ID_REG = 0x08;

/* Reading 14 bytes out of REG 1 gets:
 * 0 - ??
 * 1 - touch events
 * 2 - X hi (4 bits)
 * 3 - X lo (8 bits)
 * 4 - Y hi
 * 5 - Y lo
 * 6 - ??
 * 7 - ??
 * 8-13 repeat for next touch
 */

/* Original code just does a 10m reset line bounce and is ready to go.
 */
void AXS5106Touchscreen::setup() {
  if (this->reset_pin_ == nullptr) {
    this->mark_failed();
    return;
  }
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(false);

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = this->display_->get_native_width();
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = this->display_->get_native_height();
  }

  // Take out of reset off the main loop
  this->set_timeout(10, [this]() { this->reset_pin_->digital_write(true); });
}

void AXS5106Touchscreen::update_touches() {
  uint8_t data[14] = {0};  // copying byte read size in case it's fixed

  /* This bit is a little stupid.  You can't use `read_register` here
   * because the micro needs a little rest before actually having
   * the data ready.
   *
   * The datasheet for the CST5106L says to wait 45us and then you'll
   * still get a NACK
   */
  i2c::ErrorCode err = this->write(&TOUCH_AXS5106_TOUCH_POINTS_REG, 1);
  if (err != i2c::ERROR_OK) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    this->skip_update_ = true;
    ESP_LOGE(TAG, "Read failed");
    return;
  }
  delayMicroseconds(45);
  this->read_bytes_raw(data, 14);

  this->status_clear_warning();

  for (int i = 0; i < 14; i++) {
    ESP_LOGVV(TAG, "  reg[%d]=%02x", i + 1, data[i]);
  }

  // I don't think this even supports two touches, can't see them
  uint8_t num_touches = data[1] & 0xf;
  if (num_touches > 2) {
    ESP_LOGV(TAG, "Limiting number of touches from %u to 2", num_touches);
    num_touches = 2;
  }

  // num_touches can be zero to indicate end of gesture

  for (int i = 0; i < num_touches; i++) {
    int idx = 2 + (6 * i);
    int16_t x = ((data[idx] & 0xf) << 8) | data[idx + 1];
    int16_t y = ((data[idx + 2] & 0xf) << 8) | data[idx + 3];
    this->add_raw_touch_position_(i, x, y);
    ESP_LOGV(TAG, "Read touch %d: %d/%d", i, x, y);
  }
}

void AXS5106Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "AXS5106 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG,
                "  X Raw Min: %d, X Raw Max: %d\n"
                "  Y Raw Min: %d, Y Raw Max: %d",
                this->x_raw_min_, this->x_raw_max_, this->y_raw_min_, this->y_raw_max_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace axs5106
}  // namespace esphome
