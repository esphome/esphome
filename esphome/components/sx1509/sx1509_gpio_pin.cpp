#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sx1509_gpio_pin.h"

namespace esphome {
namespace sx1509 {

static const char *TAG = "sx1509_gpio_pin";

SX1509GPIOPin::SX1509GPIOPin(SX1509Component *parent, uint8_t pin, uint8_t mode, bool inverted, uint16_t t_on,
                             uint16_t t_off, uint8_t on_intensity, uint8_t off_intensity, uint16_t t_rise,
                             uint16_t t_fall)
    : GPIOPin(pin, mode, inverted), parent_(parent) {
  if (this->mode_ == SX1509_BREATHE_OUTPUT || this->mode_ == SX1509_BLINK_OUTPUT) {
    this->t_on_ = calculate_led_t_register(t_on);
    this->t_off_ = calculate_led_t_register(t_off);
    this->t_rise_ = calculate_slope_register(t_rise, on_intensity, off_intensity);
    this->t_fall_ = calculate_slope_register(t_fall, on_intensity, off_intensity);
    this->on_intensity_ = on_intensity;
    this->off_intensity_ = off_intensity;
  }
}

void SX1509GPIOPin::setup() {
  ESP_LOGD(TAG, "setup pin %d", this->pin_);
  ESP_LOGD(TAG, "values: tOn:%02x tOff:%02x tRise:%02x tFall:%02x", this->t_on_, this->t_off_, this->t_rise_,
           this->t_fall_);
  this->pin_mode(this->mode_);
  if (this->mode_ == SX1509_BREATHE_OUTPUT || this->mode_ == SX1509_BLINK_OUTPUT)
    this->parent_->setup_blink(this->pin_, this->t_on_, this->t_off_, this->on_intensity_, this->off_intensity_,
                               this->t_rise_, this->t_fall_);
}

void SX1509GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
bool SX1509GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void SX1509GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

uint8_t SX1509GPIOPin::calculate_led_t_register(uint16_t ms) {
  uint16_t reg_on_1, reg_on_2;
  float time_on_1, time_on_2;
  reg_on_1 = (float) (ms / 1000.0) / (64.0 * 255.0 / (float) parent_->clk_x_);
  reg_on_2 = reg_on_1 / 8;
  reg_on_1 = constrain(reg_on_1, 1, 15);
  reg_on_2 = constrain(reg_on_2, 16, 31);
  time_on_1 = 64.0 * reg_on_1 * 255.0 / parent_->clk_x_ * 1000.0;
  time_on_2 = 512.0 * reg_on_2 * 255.0 / parent_->clk_x_ * 1000.0;
  if (abs(time_on_1 - ms) < abs(time_on_2 - ms))
    return reg_on_1;
  else
    return reg_on_2;
}

uint8_t SX1509GPIOPin::calculate_slope_register(uint16_t ms, uint8_t on_intensity, uint8_t off_intensity) {
  uint16_t reg_slope_1, reg_slope_2;
  float reg_time_1, reg_time_2;
  float t_factor = ((float) on_intensity - (4.0 * (float) off_intensity)) * 255.0 / (float) parent_->clk_x_;
  float time_s = float(ms) / 1000.0;
  reg_slope_1 = time_s / t_factor;
  reg_slope_2 = reg_slope_1 / 16;
  reg_slope_1 = constrain(reg_slope_1, 1, 15);
  reg_slope_2 = constrain(reg_slope_2, 16, 31);
  reg_time_1 = reg_slope_1 * t_factor * 1000.0;
  reg_time_2 = 16 * reg_time_1;
  if (abs(reg_time_1 - ms) < abs(reg_time_2 - ms))
    return reg_slope_1;
  else
    return reg_slope_2;
}

}  // namespace sx1509
}  // namespace esphome