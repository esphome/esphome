#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/components/spi/spi.h"
#include <esp32-hal-gpio.h>

namespace esphome {
namespace max6921 {

class MAX6921Component;

using max6921_writer_t = std::function<void(MAX6921Component &)>;


enum demo_mode_t {
  DEMO_MODE_OFF,
  DEMO_MODE_SCROLL_FONT,
};

typedef struct {
  InternalGPIOPin *pwm_pin;
  uint config_value;                                                            // intensity from 0..16
  bool config_changed;
  uint32_t max_duty;
  uint32_t duty_quotient;
  uint8_t pwm_channel;
}display_intensity_t;

typedef struct {
  std::vector<uint8_t> seg_to_out_map;
  std::vector<uint8_t> pos_to_out_map;
  uint num_digits;
  display_intensity_t intensity;
  uint8_t *out_buf_;
  size_t out_buf_size_;
  uint seg_out_smallest;
  char *current_text;
  uint current_text_buf_size;
  uint current_pos;
  bool text_changed;
  uint32_t refresh_period_us;
  demo_mode_t demo_mode;
}display_t;


class MAX6921Component : public PollingComponent,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
  void dump_config() override;
  float get_setup_priority() const override;
  uint8_t print(uint8_t pos, const char *str);
  uint8_t print(const char *str);
  void set_blank_pin(InternalGPIOPin *pin) { this->display_.intensity.pwm_pin = pin; }
  void set_demo_mode(demo_mode_t mode) { this->display_.demo_mode = mode; }
  void set_intensity(uint8_t intensity);
  void set_load_pin(GPIOPin *load) { this->load_pin_ = load; }
  void set_num_digits(uint8_t num_digits) { this->display_.num_digits = num_digits; }
  void set_seg_to_out_pin_map(const std::vector<uint8_t> &pin_map) { this->display_.seg_to_out_map = pin_map; }
  void set_pos_to_out_pin_map(const std::vector<uint8_t> &pin_map) {
    this->display_.pos_to_out_map = pin_map;
    this->display_.num_digits = pin_map.size();
  }
  void set_writer(max6921_writer_t &&writer);
  void setup() override;
  uint8_t strftime(uint8_t pos, const char *format, ESPTime time) __attribute__((format(strftime, 3, 0)));
  uint8_t strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));
  void write_data(uint8_t *ptr, size_t length);
  void update() override;

 protected:
  GPIOPin *load_pin_{};
  bool setup_finished{false};
  display_t display_;
  uint8_t *ascii_out_data_;
  void clear_display(int pos=-1);
  void disable_blank() { digitalWrite(this->display_.intensity.pwm_pin->get_pin(), LOW); }  // display on
  void IRAM_ATTR HOT disable_load() { this->load_pin_->digital_write(false); }
  static void display_refresh_task(void *pv);
  void enable_blank() { digitalWrite(this->display_.intensity.pwm_pin->get_pin(), HIGH); }  // display off
  void IRAM_ATTR HOT enable_load() { this->load_pin_->digital_write(true); }
  demo_mode_t get_demo_mode(void) { return this->display_.demo_mode; }
  int set_display(uint8_t pos, const char *str);
  void update_demo_mode_scroll_font_(void);
  optional<max6921_writer_t> writer_{};

 private:
  void init_display_(void);
  void init_font_(void);
  void update_display_();
};

}  // namespace max6921
}  // namespace esphome
