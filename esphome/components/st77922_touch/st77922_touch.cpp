#include "st77922_touch.h"
#include "esphome/core/log.h"

// Freenove FNK0104N ST77922 Touch
// Code based on ST77922_Touch from https://github.com/Freenove/Freenove_ESP32_S3_Display

namespace esphome {
namespace st77922_touch {

static const char *const TAG = "st77922_touch";

static const uint16_t TOUCH_WIDTH = 320;
static const uint16_t TOUCH_HEIGHT = 480;

static const uint16_t REG_STATUS = 0x0001;
static const uint16_t REG_MAX_TOUCHES = 0x0009;
static const uint16_t REG_TOUCH_INFO = 0x0010;
static const uint16_t REG_TOUCH_POINT0 = 0x0014;

static const uint8_t MAX_TOUCH_POINTS = 10;
static const uint8_t BYTES_PER_POINT = 7;

void ST77922Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(false);
    delay(100);  // NOLINT vendor driver reset timing at init
    this->reset_pin_->digital_write(true);
  }

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  // Raw coordinates are in native portrait panel space (320x480)
  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = (this->display_ != nullptr) ? this->display_->get_native_width() : TOUCH_WIDTH;
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = (this->display_ != nullptr) ? this->display_->get_native_height() : TOUCH_HEIGHT;
  }

  // The vendor driver waits 100 ms after releasing reset, then polls STATUS
  // indefinitely until the low nibble clears. A cold power-on needs noticeably
  // longer than a warm reboot, so retry off the event loop for several seconds
  // instead of blocking setup or failing early.
  this->set_timeout("st77922_init", 150, [this] { this->try_init_(); });
}

void ST77922Touchscreen::try_init_() {
  uint8_t status = 0xFF;
  if (this->read_reg16_(REG_STATUS, &status, 1) && (status & 0x0F) == 0) {
    if (!this->read_reg16_(REG_MAX_TOUCHES, &this->max_points_, 1) || this->max_points_ == 0 ||
        this->max_points_ > MAX_TOUCH_POINTS) {
      this->max_points_ = MAX_TOUCH_POINTS;
    }
    this->ready_ = true;
    ESP_LOGI(TAG, "Touch controller ready after %u attempt(s), max %u touch points", this->init_retries_ + 1,
             this->max_points_);
    return;
  }
  if (++this->init_retries_ < 50) {  // ~5 s total
    this->set_timeout("st77922_init", 100, [this] { this->try_init_(); });
  } else {
    ESP_LOGE(TAG, "Touch controller not ready after %u attempts (status 0x%02X)", this->init_retries_, status);
    this->mark_failed();
  }
}

void ST77922Touchscreen::update_touches() {
  if (!this->ready_) {
    this->skip_update_ = true;
    return;
  }
  uint8_t info = 0;
  if (!this->read_reg16_(REG_TOUCH_INFO, &info, 1)) {
    this->skip_update_ = true;
    return;
  }
  if ((info & 0x08) == 0) {
    // No new coordinate data; keep previous touch state (mirrors vendor Get_Touch())
    this->skip_update_ = true;
    return;
  }

  uint8_t data[BYTES_PER_POINT * MAX_TOUCH_POINTS] = {0};
  if (!this->read_reg16_(REG_TOUCH_POINT0, data, BYTES_PER_POINT * this->max_points_)) {
    this->skip_update_ = true;
    return;
  }

  for (uint8_t i = 0; i < this->max_points_; i++) {
    const uint8_t *p = &data[i * BYTES_PER_POINT];
    if ((p[0] & 0x80) == 0)
      continue;  // slot not active
    uint16_t x = ((p[0] & 0x3F) << 8) | p[1];
    uint16_t y = ((p[2] & 0x3F) << 8) | p[3];
    this->add_raw_touch_position_(i, x, y);
  }
}

bool ST77922Touchscreen::read_reg16_(uint16_t reg, uint8_t *data, size_t len) {
  // Single write+read transaction with big-endian register address —
  // same repeated-start pattern as the vendor driver
  return this->read_register16(reg, data, len) == i2c::ERROR_OK;
}

void ST77922Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "ST77922 (Sitronix-style) Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Max touch points: %d", this->max_points_);
}

}  // namespace st77922_touch
}  // namespace esphome
