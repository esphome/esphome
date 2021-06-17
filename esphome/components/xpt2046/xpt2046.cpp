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
  }
  spi_setup();
  read_adc_(0xD0);  // ADC powerdown, enable PENIRQ pin
}

void XPT2046Component::loop() {
  if (this->irq_pin_ != nullptr) {
    // Force immediate update if a falling edge (= touched is seen) Ignore if still active
    // (that would mean that we missed the release because of a too long update interval)
    bool val = this->irq_pin_->digital_read();
    if (!val && this->last_irq_ && !this->touched) {
      ESP_LOGD(TAG, "Falling penirq edge, forcing update");
      update();
    }
    this->last_irq_ = val;
  }
}

void XPT2046Component::update() {
  int16_t data[6];
  bool touch = false;
  uint32_t now = millis();

  this->z_raw = 0;

  // In case the penirq pin is present only do the SPI transaction if it reports a touch (is low).
  // The touch has to be also confirmed with checking the pressure over threshold
  if (this->irq_pin_ == nullptr || !this->irq_pin_->digital_read()) {
    enable();

    int16_t z1 = read_adc_(0xB1 /* Z1 */);
    int16_t z2 = read_adc_(0xC1 /* Z2 */);

    this->z_raw = z1 + 4095 - z2;

    touch = (this->z_raw >= this->threshold_);
    if (touch) {
      read_adc_(0x91 /* Y */);  // dummy Y measure, 1st is always noisy
      data[0] = read_adc_(0xD1 /* X */);
      data[1] = read_adc_(0x91 /* Y */);  // make 3 x-y measurements
      data[2] = read_adc_(0xD1 /* X */);
      data[3] = read_adc_(0x91 /* Y */);
      data[4] = read_adc_(0xD1 /* X */);
    }

    data[5] = read_adc_(0x90 /* Y */);  // Last Y touch power down

    disable();
  }

  if (touch) {
    this->x_raw = best_two_avg(data[0], data[2], data[4]);
    this->y_raw = best_two_avg(data[1], data[3], data[5]);
  } else {
    this->x_raw = this->y_raw = 0;
  }

  ESP_LOGV(TAG, "Update [x, y] = [%d, %d], z = %d%s", this->x_raw, this->y_raw, this->z_raw, (touch ? " touched" : ""));

  if (touch) {
    // Normalize raw data according to calibration min and max

    int16_t x_val = normalize(this->x_raw, this->x_raw_min_, this->x_raw_max_);
    int16_t y_val = normalize(this->y_raw, this->y_raw_min_, this->y_raw_max_);

    if (this->swap_x_y_) {
      std::swap(x_val, y_val);
    }

    if (this->invert_x_) {
      x_val = 0x7fff - x_val;
    }

    if (this->invert_y_) {
      y_val = 0x7fff - y_val;
    }

    x_val = (int16_t)((int) x_val * this->x_dim_ / 0x7fff);
    y_val = (int16_t)((int) y_val * this->y_dim_ / 0x7fff);

    if (!this->touched || (now - this->last_pos_ms_) >= this->report_millis_) {
      ESP_LOGD(TAG, "Raw [x, y] = [%d, %d], transformed = [%d, %d]", this->x_raw, this->y_raw, x_val, y_val);

      this->x = x_val;
      this->y = y_val;
      this->touched = true;
      this->last_pos_ms_ = now;

      this->on_state_trigger_->process(this->x, this->y, true);
      for (auto *button : this->buttons_)
        button->touch(this->x, this->y);
    }
  } else {
    if (this->touched) {
      ESP_LOGD(TAG, "Released [%d, %d]", this->x, this->y);

      this->touched = false;

      this->on_state_trigger_->process(this->x, this->y, false);
      for (auto *button : this->buttons_)
        button->release();
    }
  }
}

void XPT2046Component::set_calibration(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max) {
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
  ESP_LOGCONFIG(TAG, "  X dim: %d", this->x_dim_);
  ESP_LOGCONFIG(TAG, "  Y dim: %d", this->y_dim_);
  if (this->swap_x_y_) {
    ESP_LOGCONFIG(TAG, "  Swap X/Y");
  }
  ESP_LOGCONFIG(TAG, "  threshold: %d", this->threshold_);
  ESP_LOGCONFIG(TAG, "  Report interval: %u", this->report_millis_);

  LOG_UPDATE_INTERVAL(this);
}

float XPT2046Component::get_setup_priority() const { return setup_priority::DATA; }

int16_t XPT2046Component::best_two_avg(int16_t x, int16_t y, int16_t z) {
  int16_t da, db, dc;
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
    ret = 0x7fff;
  } else {
    ret = (int16_t)((int) 0x7fff * (val - min_val) / (max_val - min_val));
  }

  return ret;
}

int16_t XPT2046Component::read_adc_(uint8_t ctrl) {
  uint8_t data[2];

  write_byte(ctrl);
  data[0] = read_byte();
  data[1] = read_byte();

  return ((data[0] << 8) | data[1]) >> 3;
}

void XPT2046OnStateTrigger::process(int x, int y, bool touched) { this->trigger(x, y, touched); }

void XPT2046Button::touch(int16_t x, int16_t y) {
  bool touched = (x >= this->x_min_ && x <= this->x_max_ && y >= this->y_min_ && y <= this->y_max_);

  if (touched) {
    this->publish_state(true);
    this->state_ = true;
  } else {
    release();
  }
}

void XPT2046Button::release() {
  if (this->state_) {
    this->publish_state(false);
    this->state_ = false;
  }
}

}  // namespace xpt2046
}  // namespace esphome
