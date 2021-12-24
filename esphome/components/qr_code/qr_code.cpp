#include "qr_code.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include <Arduino.h>

// The definition of these macro must be temporarily disabled to avoid conflicts
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#undef HIGH
#undef LOW
#endif

namespace esphome {
namespace qr_code {

static const char *const TAG = "qr_code";

void QrCode::setup() { this->generate_qr_code(); }

void QrCode::dump_config() {
  ESP_LOGCONFIG(TAG, "QR code:");
  ESP_LOGCONFIG(TAG, "  Value: '%s'", this->value_.c_str());
  ESP_LOGCONFIG(TAG, "  Scale: %d", this->scale_);
}

void QrCode::set_value(const std::string &value) {
  this->value_ = value;
  this->needs_update_ = true;
}

void QrCode::set_scale(int scale) { this->scale_ = scale; }

void QrCode::set_ecc(Ecc ecc) {
  this->error_correction_level_ = (qrcodegen::QrCode::Ecc) ecc;
  this->needs_update_ = true;
}

void QrCode::generate_qr_code() {
  ESP_LOGV(TAG, "Generating QR code...");
  this->qr_ = qrcodegen::QrCode::encodeText(this->value_.c_str(), this->error_correction_level_);
}

void QrCode::draw(display::DisplayBuffer *buff, uint16_t x_offset, uint16_t y_offset, Color color) {
  ESP_LOGV(TAG, "Drawing QR code at (%d, %d)", x_offset, y_offset);

  if (this->needs_update_) {
    this->generate_qr_code();
    this->needs_update_ = false;
  }

  uint8_t qrcodeWidth = this->qr_.getSize();

  for (int y = 0; y < qrcodeWidth * this->scale_; y++) {
    for (int x = 0; x < qrcodeWidth * this->scale_; x++) {
      if (this->qr_.getModule(x / this->scale_, y / this->scale_)) {
        buff->draw_pixel_at(x_offset + x, y_offset + y, color);
      }
    }
  }
}
}  // namespace qr_code
}  // namespace esphome

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#define HIGH 0x1
#define LOW 0x0
#endif
