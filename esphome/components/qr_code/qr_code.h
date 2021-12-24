#pragma once
#include "esphome/core/component.h"
#include "esphome/core/color.h"
#include <stdbool.h>
#include <stdint.h>

// The definition of these macro must be temporarily disabled to avoid conflicts
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#undef HIGH
#undef LOW
#endif

#include "cpp/qrcodegen.hpp"

namespace esphome {
// forward declare DisplayBuffer
namespace display {
class DisplayBuffer;
}  // namespace display

namespace qr_code {

enum Ecc {
  ECC_LOW = 0,
  ECC_MEDIUM = 1,
  ECC_QUARTILE = 2,
  ECC_HIGH = 3,
};

class QrCode : public Component {
 public:
  void draw(display::DisplayBuffer *buff, uint16_t x_offset, uint16_t y_offset, Color color);

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

  void set_value(const std::string &value);
  void set_scale(int scale);
  void set_ecc(Ecc ecc);

  void generate_qr_code();

 protected:
  qrcodegen::QrCode::Ecc error_correction_level_;
  bool needs_update_ = true;
  qrcodegen::QrCode qr_ = qrcodegen::QrCode::encodeText("", qrcodegen::QrCode::Ecc::LOW);
  int scale_;
  std::string value_;
};
}  // namespace qr_code
}  // namespace esphome

// Redefine the macros to their original values
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#define HIGH 0x1
#define LOW 0x0
#endif
