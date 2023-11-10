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
  spi_setup();
  read_adc_(0xD0);  // ADC powerdown, enable PENIRQ pin
}

void XPT2046Component::update_touches_() {
  int16_t data[6], x_raw, y_raw, z_raw;
  bool touch = false;

  enable();

  int16_t touch_pressure_1 = read_adc_(0xB1 /* touch_pressure_1 */);
  int16_t touch_pressure_2 = read_adc_(0xC1 /* touch_pressure_2 */);
  ESP_LOGVV(TAG, "touch_pressure  %d, %d", touch_pressure_1, touch_pressure_2);
  z_raw = touch_pressure_1 + 0Xfff - touch_pressure_2;

  touch = (z_raw >= this->threshold_);
  if (touch) {
    read_adc_(0xD1 /* X */);  // dummy Y measure, 1st is always noisy
    data[0] = read_adc_(0x91 /* Y */);
    data[1] = read_adc_(0xD1 /* X */);  // make 3 x-y measurements
    data[2] = read_adc_(0x91 /* Y */);
    data[3] = read_adc_(0xD1 /* X */);
    data[4] = read_adc_(0x91 /* Y */);
  }

  data[5] = read_adc_(0xD0 /* X */);  // Last X touch power down

  disable();

  if (touch) {
    x_raw = best_two_avg(data[1], data[3], data[5]);
    y_raw = best_two_avg(data[0], data[2], data[4]);

    ESP_LOGV(TAG, "Touchscreen Update [%d, %d], z = %d", x_raw, y_raw, z_raw);

    set_raw_touch_posistion_(0, x_raw, y_raw, z_raw);
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
  // ESP_LOGCONFIG(TAG, "  Report interval: %u", this->report_millis_);

  LOG_UPDATE_INTERVAL(this);
}

float XPT2046Component::get_setup_priority() const { return setup_priority::DATA; }

int16_t XPT2046Component::best_two_avg(int16_t x, int16_t y, int16_t z) {  // NOLINT
  int16_t da, db, dc;                                                      // NOLINT
  int16_t reta = 0;

  da = (x > y) ? x - y : y - x;
  db = (x > z) ? x - z : z - x;
  dc = (z > y) ? z - y : y - z;

  if (da <= db && da <= dc) {
    reta = (x + y) >> 1;
  } else if (db <= da && db <= dc) {
    reta = (x + z) >> 1;
  } else {
    reta = (y + z) >> 1;
  }

  return reta;
}

int16_t XPT2046Component::read_adc_(uint8_t ctrl) {  // NOLINT
  uint8_t data[2];

  write_byte(ctrl);
  delay(1);
  data[0] = read_byte();
  data[1] = read_byte();

  return ((data[0] << 8) | data[1]) >> 3;
}

}  // namespace xpt2046
}  // namespace esphome
