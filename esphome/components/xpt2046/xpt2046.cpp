#include "xpt2046.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <algorithm>

namespace esphome {
namespace xpt2046 {

static const char *TAG = "xpt2046";

void XPT2046Component::setup() { spi_setup(); }

void XPT2046Component::update() {
  int16_t data[6];
  bool touched;
  unsigned long now = millis();

  enable();

  int16_t z1 = read_adc_(0xB1 /* Z1 */);
  int16_t z2 = read_adc_(0xC1 /* Z2 */);

  this->z_raw_ = z1 + 4095 - z2;

  touched = (this->z_raw_ >= this->threshold_);
  if (touched) {
    read_adc_(0x91 /* Y */);  // dummy Y measure, 1st is always noisy
    data[0] = read_adc_(0xD1 /* X */);
    data[1] = read_adc_(0x91 /* Y */);  // make 3 x-y measurements
    data[2] = read_adc_(0xD1 /* X */);
    data[3] = read_adc_(0x91 /* Y */);
    data[4] = read_adc_(0xD1 /* X */);
  }

  data[5] = read_adc_(0x90 /* Y */);  // Last Y touch power down

  disable();

  if (touched) {
    this->x_raw_ = best_two_avg(data[0], data[2], data[4]);
    this->y_raw_ = best_two_avg(data[1], data[3], data[5]);
  } else {
    this->x_raw_ = this->y_raw_ = 0;
  }

  ESP_LOGV(TAG, "Update [x, y] = [%d, %d], z = %d%s", x_raw_, y_raw_, z_raw_, (touched ? " touched" : ""));

  if (touched) {
    // Normalize raw data according to calibration min and max

    int16_t x_raw_norm = normalize(this->x_raw_, this->x_raw_min_, this->x_raw_max_);
    int16_t y_raw_norm = normalize(this->y_raw_, this->y_raw_min_, this->y_raw_max_);

    int16_t x = this->swap_x_y_ ? y_raw_norm : x_raw_norm;
    int16_t y = this->swap_x_y_ ? x_raw_norm : y_raw_norm;

    if (this->invert_x_) {
      x = 0x7fff - x;
    }

    if (this->invert_y_) {
      y = 0x7fff - y;
    }

    x = (int16_t)((int) x * this->x_dim_ / 0x7fff);
    y = (int16_t)((int) y * this->y_dim_ / 0x7fff);

    if (!touched_out_ || (this->report_millis_ != 0 && (now - this->last_pos_ms_) >= this->report_millis_)) {
      ESP_LOGD(TAG, "Raw [x, y] = [%d, %d], transformed = [%d, %d]", this->x_raw_, this->y_raw_, x, y);

      this->on_state_trigger_->process(x, y, true);

      this->x_out_ = x;
      this->y_out_ = y;
      this->touched_out_ = true;
      this->last_pos_ms_ = now;
    }
  } else {
    if (this->touched_out_) {
      this->on_state_trigger_->process(this->x_out_, this->y_out_, false);

      this->touched_out_ = false;
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

}  // namespace xpt2046
}  // namespace esphome
