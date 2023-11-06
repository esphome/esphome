#include "xpt2046.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <cinttypes>

namespace esphome {
namespace xpt2046 {

static const char *const TAG = "xpt2046";

void XPT2046TouchscreenStore::gpio_intr(XPT2046TouchscreenStore *store) { store->touch = true; }

void XPT2046Component::setup() {
  if (this->irq_pin_ != nullptr) {
    // The pin reports a touch with a falling edge. Unfortunately the pin goes also changes state
    // while the channels are read and wiring it as an interrupt is not straightforward and would
    // need careful masking. A GPIO poll is cheap so we'll just use that.

    this->irq_pin_->setup();  // INPUT
    this->irq_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->irq_pin_->setup();
    this->irq_pin_->attach_interrupt(XPT2046TouchscreenStore::gpio_intr, &this->store_, gpio::INTERRUPT_FALLING_EDGE);
  }
  spi_setup();
  read_adc_(0xD0);  // ADC powerdown, enable PENIRQ pin
}

void XPT2046Component::loop() {
  if ((this->irq_pin_ != nullptr) && (this->store_.touch || this->touched)) {
    this->store_.touch = false;
    check_touch_();
  }
}

void XPT2046Component::update() {
  if (this->irq_pin_ == nullptr)
    check_touch_();
}

void XPT2046Component::check_touch_() {
  int16_t data[6];
  bool touch = false;
  uint32_t now = millis();

  enable();

  int16_t touch_pressure_1 = read_adc_(0xB1 /* touch_pressure_1 */);
  int16_t touch_pressure_2 = read_adc_(0xC1 /* touch_pressure_2 */);

  this->z_raw = touch_pressure_1 + 0Xfff - touch_pressure_2;

  touch = (this->z_raw >= this->threshold_);
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
    this->x_raw = best_two_avg(data[1], data[3], data[5]);
    this->y_raw = best_two_avg(data[0], data[2], data[4]);

    ESP_LOGVV(TAG, "Update [x, y] = [%d, %d], z = %d", this->x_raw, this->y_raw, this->z_raw);

    TouchPoint touchpoint;

    touchpoint.x = normalize(this->x_raw, this->x_raw_min_, this->x_raw_max_);
    touchpoint.y = normalize(this->y_raw, this->y_raw_min_, this->y_raw_max_);

    if (this->swap_x_y_) {
      std::swap(touchpoint.x, touchpoint.y);
    }

    if (this->invert_x_) {
      touchpoint.x = 0xfff - touchpoint.x;
    }

    if (this->invert_y_) {
      touchpoint.y = 0xfff - touchpoint.y;
    }

    switch (static_cast<TouchRotation>(this->display_->get_rotation())) {
      case ROTATE_0_DEGREES:
        break;
      case ROTATE_90_DEGREES:
        std::swap(touchpoint.x, touchpoint.y);
        touchpoint.y = 0xfff - touchpoint.y;
        break;
      case ROTATE_180_DEGREES:
        touchpoint.x = 0xfff - touchpoint.x;
        touchpoint.y = 0xfff - touchpoint.y;
        break;
      case ROTATE_270_DEGREES:
        std::swap(touchpoint.x, touchpoint.y);
        touchpoint.x = 0xfff - touchpoint.x;
        break;
    }

    touchpoint.x = (int16_t) ((int) touchpoint.x * this->display_->get_width() / 0xfff);
    touchpoint.y = (int16_t) ((int) touchpoint.y * this->display_->get_height() / 0xfff);

    if (!this->touched || (now - this->last_pos_ms_) >= this->report_millis_) {
      ESP_LOGV(TAG, "Touching at [%03X, %03X] => [%3d, %3d]", this->x_raw, this->y_raw, touchpoint.x, touchpoint.y);

      this->defer([this, touchpoint]() { this->send_touch_(touchpoint); });

      this->x = touchpoint.x;
      this->y = touchpoint.y;
      this->touched = true;
      this->last_pos_ms_ = now;
    }
  }

  if (!touch && this->touched) {
    this->x_raw = this->y_raw = this->z_raw = 0;
    ESP_LOGV(TAG, "Released [%d, %d]", this->x, this->y);
    this->touched = false;
    for (auto *listener : this->touch_listeners_)
      listener->release();
  }
}

void XPT2046Component::set_calibration(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max) {  // NOLINT
  this->x_raw_min_ = std::min(x_min, x_max);
  this->x_raw_max_ = std::max(x_min, x_max);
  this->y_raw_min_ = std::min(y_min, y_max);
  this->y_raw_max_ = std::max(y_min, y_max);
  this->invert_x_ = (x_min > x_max);
  this->invert_y_ = (y_min > y_max);
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
  ESP_LOGCONFIG(TAG, "  Report interval: %" PRIu32, this->report_millis_);

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

int16_t XPT2046Component::normalize(int16_t val, int16_t min_val, int16_t max_val) {
  int16_t ret;

  if (val <= min_val) {
    ret = 0;
  } else if (val >= max_val) {
    ret = 0xfff;
  } else {
    ret = (int16_t) ((int) 0xfff * (val - min_val) / (max_val - min_val));
  }

  return ret;
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
