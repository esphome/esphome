#pragma once
#include "esphome/core/component.h"
#include "esphome/core/color.h"

#include <cstdint>

#include "qrcodegen.h"

namespace esphome {
// forward declare DisplayBuffer
namespace display {
class Display;
}  // namespace display

namespace qr_code {
class QrCode : public Component {
 public:
  void draw(display::Display *buff, uint16_t x_offset, uint16_t y_offset, Color color, int scale);

  void dump_config() override;

  void set_value(const std::string &value);
  void set_ecc(qrcodegen_Ecc ecc);

  void generate_qr_code();

 protected:
  std::string value_;
  qrcodegen_Ecc ecc_;
  bool needs_update_ = true;
  uint8_t qr_[qrcodegen_BUFFER_LEN_MAX];
};
}  // namespace qr_code
}  // namespace esphome
