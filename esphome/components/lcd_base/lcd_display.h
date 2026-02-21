#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/components/text_display/text_display.h"

#include <map>
#include <vector>

namespace esphome {
namespace lcd_base {

class LCDDisplay;

class LCDDisplay : public text_display::TextDisplay {
 public:
  void set_user_defined_char(uint8_t pos, const std::vector<uint8_t> &data) { this->user_defined_chars_[pos] = data; }

  void setup() override;
  float get_setup_priority() const override;
  void display() override;

  /// Load custom char to given location
  void loadchar(uint8_t location, uint8_t charmap[]);

 protected:
  virtual bool is_four_bit_mode() = 0;
  virtual void write_n_bits(uint8_t value, uint8_t n) = 0;
  virtual void send(uint8_t value, bool rs) = 0;

  void command_(uint8_t value);

  std::map<uint8_t, std::vector<uint8_t> > user_defined_chars_;
};

}  // namespace lcd_base
}  // namespace esphome
