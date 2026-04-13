#pragma once

#include <string>

#include "esphome/core/gpio.h"

#include "pcm5122.h"

namespace esphome {
namespace pcm5122 {

class PCMGPIOPin : public GPIOPin, public Parented<PCM5122> {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override {}
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }
  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{gpio::FLAG_NONE};
  bool value_{false};
};

}  // namespace pcm5122
}  // namespace esphome
