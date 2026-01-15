#include "qr_code.h"
#include "esphome/components/display/display.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qr_code {

static const char *const TAG = "qr_code";

void QrCode::dump_config() {
  ESP_LOGCONFIG(TAG,
                "QR code:\n"
                "  Value: '%s'",
                this->value_.c_str());
}

void QrCode::set_value(const std::string &value) {
  this->value_ = value;
  this->needs_update_ = true;
}

void QrCode::set_ecc(qrcodegen_Ecc ecc) {
  this->ecc_ = ecc;
  this->needs_update_ = true;
}

void QrCode::generate_qr_code() {
  ESP_LOGV(TAG, "Generating QR code");

#ifdef USE_ESP32
  // ESP32 has 8KB stack, safe to allocate ~4KB buffer on stack
  uint8_t tempbuffer[qrcodegen_BUFFER_LEN_MAX];
#else
  // Other platforms (ESP8266: 4KB, RP2040: 2KB, LibreTiny: ~4KB) have smaller stacks
  // Allocate buffer on heap to avoid stack overflow
  auto tempbuffer_owner = std::make_unique<uint8_t[]>(qrcodegen_BUFFER_LEN_MAX);
  uint8_t *tempbuffer = tempbuffer_owner.get();
#endif

  if (!qrcodegen_encodeText(this->value_.c_str(), tempbuffer, this->qr_, this->ecc_, qrcodegen_VERSION_MIN,
                            qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true)) {
    ESP_LOGE(TAG, "Failed to generate QR code");
  }
}

void QrCode::draw(display::Display *buff, uint16_t x_offset, uint16_t y_offset, Color color, int scale) {
  ESP_LOGV(TAG, "Drawing QR code at (%d, %d)", x_offset, y_offset);

  if (this->needs_update_) {
    this->generate_qr_code();
    this->needs_update_ = false;
  }

  uint8_t qrcode_width = qrcodegen_getSize(this->qr_);

  for (int y = 0; y < qrcode_width * scale; y++) {
    for (int x = 0; x < qrcode_width * scale; x++) {
      if (qrcodegen_getModule(this->qr_, x / scale, y / scale)) {
        buff->draw_pixel_at(x_offset + x, y_offset + y, color);
      }
    }
  }
}

uint8_t QrCode::get_size() {
  if (this->needs_update_) {
    this->generate_qr_code();
    this->needs_update_ = false;
  }

  uint8_t size = qrcodegen_getSize(this->qr_);

  return size;
}

}  // namespace qr_code
}  // namespace esphome
