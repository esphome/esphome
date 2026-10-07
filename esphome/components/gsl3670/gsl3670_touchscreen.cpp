#include "gsl3670_touchscreen.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::gsl3670 {

static const char *const TAG = "gsl3670.touchscreen";
static const size_t MAX_TOUCHES = 3;
// ---------------------------------------------------------------------------
// setup() – mirrors esp_lcd_touch_gsl3670_init() in the Seeed BSP:
//   clear_reg → reset → load_fw → startup_chip → reset → startup_chip
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GSL3670 touchscreen...");

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = this->display_->get_native_width();
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = this->display_->get_native_height();
  }

  this->clear_reg_();
  this->reset_();
  this->load_firmware_();
  this->startup_chip_();
  this->reset_();
  this->startup_chip_();

  ESP_LOGCONFIG(TAG, "GSL3670 initialised OK");
}

void GSL3670Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG,
                "GSL3670 Touchscreen:\n"
                "  X-raw-max: %d\n"
                "  Y-raw-max: %d\n",
                this->x_raw_max_, this->y_raw_max_);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset Pin:     ", this->reset_pin_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  ESP_LOGCONFIG(TAG, "  Firmware records: %zu", this->firmware_len_);
}

// ---------------------------------------------------------------------------
// update_touches() – mirrors esp_lcd_touch_gsl3670_read_data() in Seeed BSP
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::update_touches() {
  uint8_t buf[44] = {};
  auto err = this->read_register(0x80, buf, sizeof(buf));
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read failed (%d)", err);
    return;
  }
  uint8_t finger_num = clamp_at_most(buf[0], MAX_TOUCHES);

  // Build gsl_touch_info exactly as the Seeed driver does
  for (uint8_t j = 0; j != finger_num; j++) {
    // buf[(j+1)*4 + 0..3]:  byte0=y_lo, byte1=y_hi, byte2=x_lo, byte3=id|x_hi
    auto x = (uint16_t) (((buf[(j + 1) * 4 + 3] & 0x0f) << 8) | buf[(j + 1) * 4 + 2]);
    auto y = (uint16_t) ((buf[(j + 1) * 4 + 1] << 8) | buf[(j + 1) * 4 + 0]);
    auto id = (buf[(j + 1) * 4 + 3] >> 4) & 0x0f;
    ESP_LOGV(TAG, "Touch id=%u, x=%u y=%u", id, x, y);
    if (x <= 8192 && y <= 8192)
      this->add_raw_touch_position_(id, x, y);
  }
}

// ---------------------------------------------------------------------------
// clear_reg_() – mirrors esp_lcd_touch_gsl3670_clear_reg()
//   GPIO reset → write 0x01 to 0x88 → write 0x04 to 0xe4 → write 0x00 to 0xe0
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::clear_reg_() {
  ESP_LOGD(TAG, "clear_reg");

  // GPIO reset pulse
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(1);
    this->reset_pin_->digital_write(true);
    delay(5);
  }

  this->write_reg8_(0x88, 0x01);
  // delay(5);
  this->write_reg8_(0xe4, 0x04);
  // delay(5);
  this->write_reg8_(0xe0, 0x00);
  // delay(5);
}

// ---------------------------------------------------------------------------
// reset_() – mirrors touch_gsl3670_reset()
//   GPIO reset → write 0x04 to 0xe4 → write 4×0x00 to 0xbc
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::reset_() {
  ESP_LOGD(TAG, "reset");

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(1);
    this->reset_pin_->digital_write(true);
    delay(5);
  }

  this->write_reg8_(0xe4, 0x04);

  uint8_t zeros[4] = {0, 0, 0, 0};
  this->write_reg_(0xbc, zeros, 4);
}

void GSL3670Touchscreen::load_firmware_() {
  if (firmware_ == nullptr || firmware_len_ == 0) {
    ESP_LOGW(TAG, "No firmware supplied – skipping");
    return;
  }

  ESP_LOGD(TAG, "Loading firmware (%zu blocks)...", firmware_len_);

  static constexpr size_t FW_BLK_SIZE = 128 + 4;

  for (size_t i = 0; i != this->firmware_len_; i++) {
    auto offset = i * FW_BLK_SIZE;
    uint8_t val = this->firmware_[offset + 0];
    ESP_LOGV(TAG, "Firmware address 0x%02X", val);
    this->write_reg_(0xf0, &val, 1);
    this->write_reg_(0, this->firmware_ + offset + 4, 128);
  }
  ESP_LOGD(TAG, "Firmware load complete");
}

// ---------------------------------------------------------------------------
// startup_chip_() – mirrors esp_lcd_touch_gsl3670_startup_chip()
//   write 0x00 to 0xe0
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::startup_chip_() {
  ESP_LOGD(TAG, "startup_chip");
  this->write_reg8_(0xe0, 0x00);
  delay(5);
}

// ---------------------------------------------------------------------------
// I2C helpers
// ---------------------------------------------------------------------------

bool GSL3670Touchscreen::write_reg_(uint8_t reg, const uint8_t *data, size_t len) {
  auto err = this->write_register(reg, data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write reg 0x%02X len %zu failed (%d)", reg, len, err);
    return false;
  }
  return true;
}

bool GSL3670Touchscreen::write_reg8_(uint8_t reg, uint8_t val) { return write_reg_(reg, &val, 1); }

}  // namespace esphome::gsl3670
