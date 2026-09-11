#include "esp_color_buffer.h"

namespace esphome::light {

void ESPColorBuffer::shift_left(int32_t amount) {
  const int32_t size = this->size();
  if (amount > 0) {
    if (amount >= size)
      return;
    this->range(0, size - amount) = this->range(amount, size);
  } else {
    amount = -amount;
    if (amount >= size)
      return;
    this->range(amount, size) = this->range(0, size - amount);
  }
}

bool InterleavedColorBuffer::allocate_and_setup(RAMAllocator<uint8_t> *allocator) {
  uint8_t *led_data = allocator->allocate(this->get_led_data_bytes());
  if (led_data != nullptr) {
    uint8_t *effect_data = allocator->allocate(this->num_leds_);
    if (effect_data != nullptr) {
      this->setup(led_data, effect_data);
      return true;
    }
    allocator->deallocate(led_data, this->get_led_data_bytes());
  }
  return false;
}

void InterleavedColorBuffer::setup(uint8_t *led_data, uint8_t *effect_data) {
  memset(led_data, 0, this->get_led_data_bytes());
  memset(effect_data, 0, this->num_leds_);
  this->led_data_after_leading_bytes_ = led_data + this->layout_.leading_bytes;
  this->effect_data_ = effect_data;
}

bool InterleavedColorBuffer::is_all_black() const {
  return this->led_data_after_leading_bytes_ == nullptr ||
         is_all_black_internal(this->led_data_after_leading_bytes_, this->num_leds_, this->layout_.channel_colors,
                               this->layout_.bytes_per_led);
}

void InterleavedColorBuffer::clear_effect_data() {
  if (this->effect_data_ != nullptr) {
    clear_effect_data_internal(this->effect_data_, this->num_leds_);
  }
}

ESPColorView InterleavedColorBuffer::get_color_view(size_t index) {
  if (this->led_data_after_leading_bytes_ != nullptr) {  // implies effect_data_ is also not null
    return get_color_view_internal(index, this->led_data_after_leading_bytes_, this->effect_data_,
                                   this->layout_.channel_colors, this->layout_.bytes_per_led, this->color_correction_);
  }
  return ESPColorView{this->color_correction_};
}

}  // namespace esphome::light
