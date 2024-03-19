#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/components/spi/spi.h"
#include <esp32-hal-gpio.h>


//#define MORE_DEBUG
#ifdef MORE_DEBUG
#define ESP_LOGD_MORE(...)  ESP_LOGD(__VA_ARGS__)
#else
#define ESP_LOGD_MORE(...)  {}
#endif

namespace esphome {
namespace max6921 {

class MAX6921Component;

using max6921_writer_t = std::function<void(MAX6921Component &)>;


typedef struct {
  InternalGPIOPin *pwm_pin;
  uint config_value;                                                            // intensity from 0..16
  bool config_changed;
  uint32_t max_duty;
  uint32_t duty_quotient;
  uint8_t pwm_channel;
}display_intensity_t;

typedef struct {
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
  void set_intensity(uint8_t intensity);
  void set_load_pin(GPIOPin *load) { this->load_pin_ = load; }
  void set_num_digits(uint8_t num_digits) { this->display_.num_digits = num_digits; }
  void set_writer(max6921_writer_t &&writer);
  void setup() override;
  uint8_t strftime(uint8_t pos, const char *format, ESPTime time) __attribute__((format(strftime, 3, 0)));
  uint8_t strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));
  void write_data(uint8_t *ptr, size_t length);
  void update() override;

 protected:
  GPIOPin *load_pin_{};
  display_t display_;
  uint8_t *ascii_out_data_;
  void IRAM_ATTR HOT enable_load() { this->load_pin_->digital_write(true); }
  void IRAM_ATTR HOT disable_load() { this->load_pin_->digital_write(false); }
  void enable_blank() { digitalWrite(this->display_.intensity.pwm_pin->get_pin(), HIGH); }  // display off
  void disable_blank() { digitalWrite(this->display_.intensity.pwm_pin->get_pin(), LOW); }  // display on
  optional<max6921_writer_t> writer_{};
  static void display_refresh_task(void *pv);

 private:
  void init_display_(void);
  void init_font_(void);
  void update_display_();
};

}  // namespace max6921
}  // namespace esphome
