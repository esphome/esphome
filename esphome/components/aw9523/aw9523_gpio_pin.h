#pragma once

#include "esphome/components/aw9523/aw9523.h"

namespace esphome {
namespace aw9523 {

class AW9523Component;

class AW9523GPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  gpio::Flags get_flags() const override;
  std::string dump_summary() const override;
  bool digital_read() override;
  void digital_write(bool value) override;
  void set_parent(AW9523Component *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

 protected:
  AW9523Component *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace aw9523
}  // namespace esphome
