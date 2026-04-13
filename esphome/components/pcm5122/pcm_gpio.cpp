#include "pcm_gpio.h"

#include "esphome/core/log.h"

namespace esphome {
namespace pcm5122 {

static const char *const TAG = "pcm5122.gpio";

void PCMGPIOPin::setup() {
  // Configure input/output mode in the GPIO enable register
  uint8_t curr = this->parent_->reg(PCM5122_REG_GPIO_ENABLE).get();
  if (this->flags_ & gpio::FLAG_INPUT) {
    this->parent_->reg(PCM5122_REG_GPIO_ENABLE) = curr & ~(1 << (this->pin_ - 1));
  } else if (this->flags_ & gpio::FLAG_OUTPUT) {
    this->parent_->reg(PCM5122_REG_GPIO_ENABLE) = curr | (1 << (this->pin_ - 1));
    // Set pin to GPIO output mode
    this->parent_->reg(PCM5122_REG_GPIO_OUTPUT_SELECT + (this->pin_ - 1)) = 0x02;
    // Configure inversion
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

std::string PCMGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "PCM5122 GPIO%u", this->pin_);
  return buffer;
}

}  // namespace pcm5122
}  // namespace esphome
