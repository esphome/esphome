#include "xpt2046.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace xpt2046 {

static const char *TAG = "xpt2046";

const int16_t Z_THRESHOLD = 400;

void XPT2046Component::setup() {
  if (this->tirq_pin_ != nullptr) {
    this->tirq_pin_->setup();
  }

  spi_setup();
}

void XPT2046Component::update() {
  int16_t data[6];
  bool touched;
  unsigned long now = millis();

  enable();

  int16_t z1 = read_adc_(0xB1 /* Z1 */);
  int16_t z2 = read_adc_(0xC1 /* Z2 */);

  this->z_raw_ = z1 + 4095 - z2;

  touched = (this->z_raw_ >= Z_THRESHOLD);
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

    int16_t x = (this->transform_ & (1 << (int) SWAP_X_Y)) ? y_raw_norm : x_raw_norm;
    int16_t y = (this->transform_ & (1 << (int) SWAP_X_Y)) ? x_raw_norm : y_raw_norm;

    if (this->transform_ & (1 << (int) INVERT_X)) {
      x = 0x7fff - x;
    }

    if (this->transform_ & (1 << (int) INVERT_Y)) {
      y = 0x7fff - y;
    }

    x = (int16_t)((int) x * this->x_dim_ / 0x7fff);
    y = (int16_t)((int) y * this->y_dim_ / 0x7fff);

    if (!touched_out_ ||
        ((x != this->x_out_ || y != this->y_out_) && (now - this->last_pos_ms_) >= this->report_millis_)) {
      ESP_LOGD(TAG, "Raw [x, y] = [%d, %d], transformed = [%d, %d]", this->x_raw_, this->y_raw_, x, y);

      if (this->x_sensor_ != nullptr) {
        this->x_sensor_->publish_state(x);
      }

      if (this->y_sensor_ != nullptr) {
        this->y_sensor_->publish_state(y);
      }

      this->x_out_ = x;
      this->y_out_ = y;

      this->last_pos_ms_ = now;
    }

    if (!this->touched_out_) {
      if (this->touched_sensor_ != nullptr) {
        this->touched_sensor_->publish_state(1);
      }
      this->touched_out_ = true;
    }
  } else {
    if (this->touched_out_) {
      if (this->touched_sensor_ != nullptr)
        this->touched_sensor_->publish_state(0);

      this->touched_out_ = false;
    }
  }
}

void XPT2046Component::dump_config() {
  ESP_LOGCONFIG(TAG, "XPT2046:");

  ESP_LOGCONFIG(TAG, "  X min: %d", this->x_raw_min_);
  ESP_LOGCONFIG(TAG, "  X max: %d", this->x_raw_max_);
  ESP_LOGCONFIG(TAG, "  Y min: %d", this->y_raw_min_);
  ESP_LOGCONFIG(TAG, "  Y max: %d", this->y_raw_max_);
  ESP_LOGCONFIG(TAG, "  X dim: %d", this->x_dim_);
  ESP_LOGCONFIG(TAG, "  Y dim: %d", this->y_dim_);
  ESP_LOGCONFIG(TAG, "  Report interval: %u", this->report_millis_);

  if (this->transform_) {
    ESP_LOGCONFIG(TAG, "  Transform: %s %s %s", (this->transform_ & (1 << SWAP_X_Y)) ? "swap_x_y" : "",
                  (this->transform_ & (1 << INVERT_X)) ? "invert_x" : "",
                  (this->transform_ & (1 << INVERT_Y)) ? "invert_y" : "");
  }

  LOG_PIN("  TIRQ Pin: ", this->tirq_pin_);
  LOG_SENSOR("  ", "Touched", this->touched_sensor_);
  LOG_SENSOR("  ", "X Coord", this->x_sensor_);
  LOG_SENSOR("  ", "Y Coord", this->y_sensor_);

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

}  // namespace xpt2046
}  // namespace esphome
