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
/// @brief Helper class for efficient buffer allocation - uses stack for small sizes, heap for large
template<size_t STACK_SIZE> class SmallBufferWithHeapFallback {
 public:
  uint8_t *get(size_t size) {
    if (size <= STACK_SIZE) {
      return this->stack_buffer_;
    }
    this->heap_buffer_ = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    return this->heap_buffer_.get();
  }

 private:
  uint8_t stack_buffer_[STACK_SIZE];
  std::unique_ptr<uint8_t[]> heap_buffer_;
};

class QrCode : public Component {
 public:
  void draw(display::Display *buff, uint16_t x_offset, uint16_t y_offset, Color color, int scale);

  void dump_config() override;

  void set_value(const std::string &value);
  void set_ecc(qrcodegen_Ecc ecc);

  void generate_qr_code();

  uint8_t get_size();

 protected:
  std::string value_;
  qrcodegen_Ecc ecc_;
  bool needs_update_ = true;
  uint8_t qr_[qrcodegen_BUFFER_LEN_MAX];
};
}  // namespace qr_code
}  // namespace esphome
