#include "cfa632.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace cfa632 {

static const char *TAG = "cfa632";

void CFA632::init_() {
  this->clear_();
  this->set_cursor_type(this->cursor_type_);
  this->set_wrap_enabled(this->wrap_enabled_);
  this->set_scroll_enabled(this->scroll_enabled_);
  this->set_backlight_brightness(this->brightness_);
  this->set_contrast(contrast_);
}

void CFA632::setup() {
  ESP_LOGCONFIG(TAG, "Setting up cfa632 display...");
  this->spi_setup();
  delay(500);  // NOLINT
  this->init_();
}

void CFA632::dump_config() {
  ESP_LOGCONFIG(TAG, "  Cursor type: %d, Wrap enabled: %s, Scroll enabled: %s, Brightness: %u", this->cursor_type_,
                this->wrap_enabled_ ? "true" : "false", this->scroll_enabled_ ? "true" : "false",
                uint8_t(this->brightness_ * 100));
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

float CFA632::get_setup_priority() const { return setup_priority::PROCESSOR; }

void CFA632::update() {
  if (this->writer_) {
    this->set_cursor_position(0, 0);
    this->call_writer_();
  }
}

void CFA632::clear_() { write(12); }

void CFA632::set_cursor_position(uint8_t row, uint8_t col) {
  this->write(17);
  this->write(col);
  this->write(row);
}

void CFA632::set_cursor_type(CursorType type) {
  this->cursor_type_ = type;

  write(this->cursor_type_);
}

void CFA632::set_scroll_enabled(bool enabled) {
  this->scroll_enabled_ = enabled;

  if (this->scroll_enabled_) {
    this->write(19);
  } else {
    this->write(20);
  }
}

void CFA632::set_wrap_enabled(bool enabled) {
  this->wrap_enabled_ = enabled;

  if (this->wrap_enabled_) {
    this->write(23);
  } else {
    this->write(24);
  }
}

void CFA632::set_backlight_brightness(float percent) {
  this->brightness_ = clamp(percent, 0, 1);

  this->write(14);
  this->write(uint8_t(this->brightness_ * 100));
}

void CFA632::set_contrast(float percent) {
  this->contrast_ = clamp(percent, 0, 1);

  this->write(15);
  this->write(uint8_t(this->contrast_ * 100));
}

CFA632::CFA632(float brightness, float contrast, bool scroll_enabled, bool wrap_enabled, CursorType cursor_type) {
  this->scroll_enabled_ = scroll_enabled;
  this->wrap_enabled_ = wrap_enabled;
  this->brightness_ = clamp(brightness, 0, 1);
  this->contrast_ = clamp(contrast, 0, 1);
  this->cursor_type_ = cursor_type;
}

size_t CFA632::write(uint8_t data) {
  this->enable();
  delayMicroseconds(1);
  this->write_byte(data);
  this->disable();
  delay(1);

  return 1;
}

size_t CFA632::write(const uint8_t *buffer, size_t size) {
  this->enable();

  size_t n = 0;
  while (size--) {
    this->transfer_byte(pgm_read_byte(buffer++));
    n++;
  }

  this->disable();

  return n;
}

}  // namespace cfa632
}  // namespace esphome
