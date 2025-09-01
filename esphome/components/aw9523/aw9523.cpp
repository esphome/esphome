#include "aw9523.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace aw9523 {

static const char *const TAG = "aw9523";

void AW9523Component::setup() {
  ESP_LOGD(TAG, "Setting up AW9523...");
  if (this->reg(AW9523_REG_CHIPID).get() != 0x23) {
    this->mark_failed();
    return;
  }
  // reset
  this->reg(AW9523_REG_SOFTRESET) = 0x00;
  // initialise all inputs
  this->reg(AW9523_REG_CONFIG0) = 0xff;
  this->reg(AW9523_REG_CONFIG1) = 0xff;
  // initialise all GPIO modes
  this->reg(AW9523_REG_LEDMODE0) = 0xff;
  this->reg(AW9523_REG_LEDMODE1) = 0xff;
  // set push pull
  this->reg(AW9523_REG_GCR) |= 0b00010000;
  // set divider
  this->reg(AW9523_REG_GCR) |= (this->divider_ & 0x03);
  // no interupt
  this->reg(AW9523_REG_INTENABLE0) = 0xff;
  this->reg(AW9523_REG_INTENABLE1) = 0xff;
}

void AW9523Component::loop() {
  if (this->is_failed() || !this->latch_inputs_)
    return;
  auto input0 = this->reg(AW9523_REG_INPUT0).get();
  auto input1 = this->reg(AW9523_REG_INPUT1).get();
  this->value_ = input0 | (input1 << 8);
}

void AW9523Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AW9523:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setting up AW9523 failed!");
  }
  LOG_I2C_DEVICE(this)
  ESP_LOGCONFIG(TAG, "  Divider: %d", this->divider_);
  ESP_LOGCONFIG(TAG, "  Max current: %.2f", this->get_max_current());
  ESP_LOGCONFIG(TAG, "  GCR  %s", format_bin((uint8_t) this->reg(AW9523_REG_GCR)).c_str());
  ESP_LOGCONFIG(TAG, "  CFG %s%s", format_bin((uint8_t) this->reg(AW9523_REG_CONFIG1)).c_str(),
                format_bin((uint8_t) this->reg(AW9523_REG_CONFIG0)).c_str());
  ESP_LOGCONFIG(TAG, "  LED %s%s", format_bin((uint8_t) this->reg(AW9523_REG_LEDMODE1)).c_str(),
                format_bin((uint8_t) this->reg(AW9523_REG_LEDMODE0)).c_str());
  ESP_LOGCONFIG(TAG, "  INT %s%s", format_bin((uint8_t) this->reg(AW9523_REG_INTENABLE1)).c_str(),
                format_bin((uint8_t) this->reg(AW9523_REG_INTENABLE0)).c_str());
}

float AW9523Component::get_max_current() { return (37.0 / 4) * (4 - this->divider_); }

void AW9523Component::set_divider(uint8_t divider) { this->divider_ = divider; }

uint8_t AW9523Component::get_divider() { return this->divider_; }

void AW9523Component::set_pin_value(uint8_t pin, uint8_t val) {
  if (this->is_failed())
    return;
  uint8_t reg{};
  // See Table 13. 256 step dimming control register
  if ((pin >= 0) && (pin <= 7)) {
    reg = 0x24 + pin;
  }
  if ((pin >= 8) && (pin <= 11)) {
    reg = 0x20 + pin - 8;
  }
  if ((pin >= 12) && (pin <= 15)) {
    reg = 0x2C + pin - 12;
  }
  this->reg(reg) = val;
}

void AW9523Component::led_driver(uint8_t pin) {
  if (this->is_failed())
    return;
  if (pin < 8) {
    this->reg(AW9523_REG_CONFIG0) &= ~(1 << pin);
    this->reg(AW9523_REG_LEDMODE0) &= ~(1 << pin);
  } else if (pin < 16) {
    this->reg(AW9523_REG_CONFIG1) &= ~(1 << (pin - 8));
    this->reg(AW9523_REG_LEDMODE1) &= ~(1 << (pin - 8));
  }
  this->set_pin_value(pin, 0x00);
}

void AW9523Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (this->is_failed())
    return;
  if (pin < 8) {
    if (flags == gpio::Flags::FLAG_OUTPUT)
      this->reg(AW9523_REG_CONFIG0) &= ~(1 << pin);
    else
      this->reg(AW9523_REG_CONFIG0) |= (1 << pin);
    this->reg(AW9523_REG_LEDMODE0) |= (1 << pin);
  } else if (pin < 16) {
    if (flags == gpio::Flags::FLAG_OUTPUT)
      this->reg(AW9523_REG_CONFIG1) &= ~(1 << (pin - 8));
    else
      this->reg(AW9523_REG_CONFIG1) |= (1 << (pin - 8));
    this->reg(AW9523_REG_LEDMODE1) |= (1 << (pin - 8));
  }
}

void AW9523Component::digital_write(uint8_t pin, bool bit_value) {
  if (this->is_failed())
    return;
  if (pin < 8) {
    uint8_t value = (1 << pin);
    if (bit_value)
      this->reg(AW9523_REG_OUTPUT0) |= value;
    else
      this->reg(AW9523_REG_OUTPUT0) &= ~value;
  } else if (pin < 16) {
    uint8_t value = (1 << (pin - 8));
    if (bit_value)
      this->reg(AW9523_REG_OUTPUT1) |= value;
    else
      this->reg(AW9523_REG_OUTPUT1) &= ~value;
  }
}

bool AW9523Component::digital_read(uint8_t pin) {
  if (this->latch_inputs_) {
    uint16_t value = (1 << pin);
    return this->value_ & value;
  } else {
    if (!this->is_failed()) {
      if (pin < 8) {
        uint8_t value = (1 << pin);
        return this->reg(AW9523_REG_INPUT0).get() & value;
      } else if (pin < 16) {
        uint8_t value = (1 << (pin - 8));
        return this->reg(AW9523_REG_INPUT1).get() & value;
      }
    }
    return false;
  }
}

}  // namespace aw9523
}  // namespace esphome
