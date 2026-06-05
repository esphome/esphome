#include "pcm_gpio.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::pcm5122 {

static const char *const TAG = "pcm5122.gpio";

void PCMGPIOPin::setup() { this->pin_mode(this->flags_); }

void PCMGPIOPin::pin_mode(gpio::Flags flags) {
  this->flags_ = flags;
  if (!this->parent_->select_page(0)) {
    ESP_LOGE(TAG, "Failed to select page 0");
    return;
  }
  optional<uint8_t> curr = this->parent_->read_byte(PCM5122_REG_GPIO_ENABLE);
  if (!curr.has_value()) {
    ESP_LOGE(TAG, "Failed to read GPIO_ENABLE");
    return;
  }
  if (flags & gpio::FLAG_INPUT) {
    this->parent_->reg(PCM5122_REG_GPIO_ENABLE) = curr.value() & ~(1 << (this->pin_ - 1));
  } else if (flags & gpio::FLAG_OUTPUT) {
    this->parent_->reg(PCM5122_REG_GPIO_ENABLE) = curr.value() | (1 << (this->pin_ - 1));
    this->parent_->reg(PCM5122_REG_GPIO_OUTPUT_SELECT + (this->pin_ - 1)) = PCM5122_GPIO_OUTPUT_SELECT_REGISTER;
    optional<uint8_t> invert = this->parent_->read_byte(PCM5122_REG_GPIO_INVERT);
    if (!invert.has_value()) {
      ESP_LOGE(TAG, "Failed to read GPIO_INVERT");
      return;
    }
    if (this->inverted_) {
      this->parent_->reg(PCM5122_REG_GPIO_INVERT) = invert.value() | (1 << (this->pin_ - 1));
    } else {
      this->parent_->reg(PCM5122_REG_GPIO_INVERT) = invert.value() & ~(1 << (this->pin_ - 1));
    }
  }
}

void PCMGPIOPin::digital_write(bool value) {
  if (!this->parent_->select_page(0))
    return;
  optional<uint8_t> curr = this->parent_->read_byte(PCM5122_REG_GPIO_OUTPUT);
  if (!curr.has_value())
    return;
  if (value) {
    this->parent_->reg(PCM5122_REG_GPIO_OUTPUT) = curr.value() | (1 << (this->pin_ - 1));
  } else {
    this->parent_->reg(PCM5122_REG_GPIO_OUTPUT) = curr.value() & ~(1 << (this->pin_ - 1));
  }
}

bool PCMGPIOPin::digital_read() {
  if (!this->parent_->select_page(0))
    return this->value_;
  optional<uint8_t> read = this->parent_->read_byte(PCM5122_REG_GPIO_INPUT);
  if (read.has_value()) {
    // GPIO input register has RSV at bit 0; GPIN_N is at bit N (unlike other GPIO registers)
    this->value_ = !!(read.value() & (1 << this->pin_)) != this->inverted_;
  }
  return this->value_;
}

size_t PCMGPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "PCM5122 GPIO%u", this->pin_);
}

}  // namespace esphome::pcm5122
