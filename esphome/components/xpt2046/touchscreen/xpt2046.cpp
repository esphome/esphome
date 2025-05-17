#include "xpt2046.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <algorithm>

namespace esphome {
namespace xpt2046 {

static const char *const TAG = "xpt2046";

void XPT2046Component::setup() {
  if (this->irq_pin_ != nullptr) {
    // The pin reports a touch with a falling edge. Unfortunately the pin goes also changes state
    // while the channels are read and wiring it as an interrupt is not straightforward and would
    // need careful masking. A GPIO poll is cheap so we'll just use that.

    this->irq_pin_->setup();  // INPUT
    this->irq_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->irq_pin_->setup();
    this->attach_interrupt_(this->irq_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }
  this->spi_setup();
  this->read_adc_(0xD0);  // ADC powerdown, enable PENIRQ pin
}

void XPT2046Component::update_touches() {
  int16_t data[6], x_raw, y_raw, z_raw;
  bool touch = false;

  enable();

  int16_t touch_pressure_1 = this->read_adc_(0xB1 /* touch_pressure_1 */);
  int16_t touch_pressure_2 = this->read_adc_(0xC1 /* touch_pressure_2 */);
  z_raw = touch_pressure_1 + 0xfff - touch_pressure_2;
  ESP_LOGVV(TAG, "Touchscreen Update z = %d", z_raw);
  touch = (z_raw >= this->threshold_);
  if (touch) {
    read_adc_(0xD1 /* X */);  // dummy Y measure, 1st is always noisy
    data[0] = this->read_adc_(0x91 /* Y */);
    data[1] = this->read_adc_(0xD1 /* X */);  // make 3 x-y measurements
    data[2] = this->read_adc_(0x91 /* Y */);
    data[3] = this->read_adc_(0xD1 /* X */);
    data[4] = this->read_adc_(0x91 /* Y */);
  }

  data[5] = this->read_adc_(0xD0 /* X */);  // Last X touch power down

  disable();

  if (touch) {
    x_raw = best_two_avg(data[1], data[3], data[5]);
    y_raw = best_two_avg(data[0], data[2], data[4]);

    ESP_LOGD(TAG, "Touchscreen Update [%d, %d], z = %d", x_raw, y_raw, z_raw);

    this->add_raw_touch_position_(0, x_raw, y_raw, z_raw);
  }
}

void XPT2046Component::dump_config() {
  ESP_LOGCONFIG(TAG, "XPT2046:");

  LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  ESP_LOGCONFIG(TAG, "  X min: %d", this->x_raw_min_);
  ESP_LOGCONFIG(TAG, "  X max: %d", this->x_raw_max_);
  ESP_LOGCONFIG(TAG, "  Y min: %d", this->y_raw_min_);
  ESP_LOGCONFIG(TAG, "  Y max: %d", this->y_raw_max_);

  ESP_LOGCONFIG(TAG, "  Swap X/Y: %s", YESNO(this->swap_x_y_));
  ESP_LOGCONFIG(TAG, "  Invert X: %s", YESNO(this->invert_x_));
  ESP_LOGCONFIG(TAG, "  Invert Y: %s", YESNO(this->invert_y_));

  ESP_LOGCONFIG(TAG, "  threshold: %d", this->threshold_);

  LOG_UPDATE_INTERVAL(this);
}

// float XPT2046Component::get_setup_priority() const { return setup_priority::DATA; }

int16_t XPT2046Component::best_two_avg(int16_t value1, int16_t value2, int16_t value3) {
  int16_t delta_a, delta_b, delta_c;
  int16_t reta = 0;

  delta_a = (value1 > value2) ? value1 - value2 : value2 - value1;
  delta_b = (value1 > value3) ? value1 - value3 : value3 - value1;
  delta_c = (value3 > value2) ? value3 - value2 : value2 - value3;

  if (delta_a <= delta_b && delta_a <= delta_c) {
    reta = (value1 + value2) >> 1;
  } else if (delta_b <= delta_a && delta_b <= delta_c) {
    reta = (value1 + value3) >> 1;
  } else {
    reta = (value2 + value3) >> 1;
  }

  return reta;
}

int16_t XPT2046Component::read_adc_(uint8_t ctrl) {  // NOLINT
  uint8_t data[2];

  this->write_byte(ctrl);
  delay(1);
  data[0] = this->read_byte();
  data[1] = this->read_byte();

  return ((data[0] << 8) | data[1]) >> 3;
}

}  // namespace xpt2046
}  // namespace esphome
