#pragma once

#include "esphome/core/hal.h"
#include "esphome/components/lcd_base/lcd_display.h"

namespace esphome {
namespace lcd_gpio {

class GPIOLCDDisplay : public lcd_base::LCDDisplay {
 public:
  void set_writer(std::function<void(GPIOLCDDisplay &)> &&writer) { this->writer_ = std::move(writer); }
  void setup() override;
  void set_data_pins(GPIOPin *d0, GPIOPin *d1, GPIOPin *d2, GPIOPin *d3) {
    this->data_pins_[0] = d0;
    this->data_pins_[1] = d1;
    this->data_pins_[2] = d2;
    this->data_pins_[3] = d3;
  }
  void set_data_pins(GPIOPin *d0, GPIOPin *d1, GPIOPin *d2, GPIOPin *d3, GPIOPin *d4, GPIOPin *d5, GPIOPin *d6,
                     GPIOPin *d7) {
    this->data_pins_[0] = d0;
    this->data_pins_[1] = d1;
    this->data_pins_[2] = d2;
    this->data_pins_[3] = d3;
    this->data_pins_[4] = d4;
    this->data_pins_[5] = d5;
    this->data_pins_[6] = d6;
    this->data_pins_[7] = d7;
  }
  void set_enable_pin(GPIOPin *enable) { this->enable_pin_ = enable; }
  void set_rs_pin(GPIOPin *rs) { this->rs_pin_ = rs; }
  void set_rw_pin(GPIOPin *rw) { this->rw_pin_ = rw; }
  void dump_config() override;

 protected:
  bool is_four_bit_mode() override { return this->data_pins_[4] == nullptr; }
  void write_n_bits(uint8_t value, uint8_t n) override;
  void send(uint8_t value, bool rs) override;

  void call_writer() override { this->writer_(*this); }

  GPIOPin *rs_pin_{nullptr};
  GPIOPin *rw_pin_{nullptr};
  GPIOPin *enable_pin_{nullptr};
  GPIOPin *data_pins_[8]{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  std::function<void(GPIOLCDDisplay &)> writer_;
};

}  // namespace lcd_gpio
}  // namespace esphome
