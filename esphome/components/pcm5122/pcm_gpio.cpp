#include "pcm_gpio.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pcm5122 {

static const char *const TAG = "pcm5122.gpio";

void PCMGPIOPin::setup() { this->pin_mode(this->flags_); }

void PCMGPIOPin::pin_mode(gpio::Flags flags) {
  this->flags_ = flags;
  uint8_t curr = this->parent_->reg(PCM5122_REG_GPIO_ENABLE).get();
  if (flags & gpio::FLAG_INPUT) {
    this->parent_->reg(PCM5122_REG_GPIO_ENABLE) = curr & ~(1 << (this->pin_ - 1));
  } else if (flags & gpio::FLAG_OUTPUT) {
    this->parent_->reg(PCM5122_REG_GPIO_ENABLE) = curr | (1 << (this->pin_ - 1));
    this->parent_->reg(PCM5122_REG_GPIO_OUTPUT_SELECT + (this->pin_ - 1)) = 0x02;
    curr = this->parent_->reg(PCM5122_REG_GPIO_INVERT).get();
    if (this->inverted_) {
      this->parent_->reg(PCM5122_REG_GPIO_INVERT) = curr | (1 << (this->pin_ - 1));
    } else {
      this->parent_->reg(PCM5122_REG_GPIO_INVERT) = curr & ~(1 << (this->pin_ - 1));
    }
  }
}

void PCMGPIOPin::digital_write(bool value) {
  uint8_t curr = this->parent_->reg(PCM5122_REG_GPIO_OUTPUT).get();
  if (value) {
    this->parent_->reg(PCM5122_REG_GPIO_OUTPUT) = curr | (1 << (this->pin_ - 1));
  } else {
    this->parent_->reg(PCM5122_REG_GPIO_OUTPUT) = curr & ~(1 << (this->pin_ - 1));
  }
}

bool PCMGPIOPin::digital_read() {
  optional<uint8_t> read = this->parent_->read_byte(PCM5122_REG_GPIO_INPUT);
  if (read.has_value()) {
    this->value_ = !!(read.value() & (1 << (this->pin_ - 1))) != this->inverted_;
  }
  return this->value_;
}

size_t PCMGPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "PCM5122 GPIO%u", this->pin_);
}

}  // namespace pcm5122
}  // namespace esphome
