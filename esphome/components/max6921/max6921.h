#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace max6921 {

class MAX6921Component;

using max6921_writer_t = std::function<void(MAX6921Component &)>;


typedef struct {
  uint num_digits;
  uint8_t *out_buf_;
  size_t out_buf_size_;
  uint seg_out_smallest;
  char *current_text;
  uint current_pos;
}display_t;


class MAX6921Component : public PollingComponent,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
  void display();
  void dump_config() override;
  float get_setup_priority() const override;
  uint8_t print(uint8_t pos, const char *str);
  uint8_t print(const char *str);
  void set_blank_pin(GPIOPin *blank) { this->blank_pin_ = blank; }
  void set_load_pin(GPIOPin *load) { this->load_pin_ = load; }
  void set_num_digits(uint8_t num_digits) { this->display_.num_digits = num_digits; }
  void set_writer(max6921_writer_t &&writer);
  void setup() override;
  void write_data(uint8_t *ptr, size_t length);
  void update() override;

 protected:
  GPIOPin *load_pin_{};
  GPIOPin *blank_pin_{};
  display_t display_;
  uint8_t *ascii_out_data_;
  void enable_load() { this->load_pin_->digital_write(true); }
  void disable_load() { this->load_pin_->digital_write(false); }
  void enable_blank() { this->blank_pin_->digital_write(true); }    // display off
  void disable_blank() { this->blank_pin_->digital_write(false); }  // display on
  optional<max6921_writer_t> writer_{};

 private:
  void init_display_(void);
  void init_font_(void);
};

}  // namespace max6921
}  // namespace esphome
