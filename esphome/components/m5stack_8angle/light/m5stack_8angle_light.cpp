#include "m5stack_8angle_light.h"

#include "esphome/core/log.h"

namespace esphome::m5stack_8angle {

ESPHOME_LOG_TAG(TAG, "m5stack_8angle.light");

void M5Stack8AngleLightOutput::setup() {
  RAMAllocator<uint8_t> allocator;
  this->buf_ = allocator.allocate(M5STACK_8ANGLE_NUM_LEDS * M5STACK_8ANGLE_BYTES_PER_LED);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer of size %u", M5STACK_8ANGLE_NUM_LEDS * M5STACK_8ANGLE_BYTES_PER_LED);
    this->mark_failed();
    return;
  };
  memset(this->buf_, 0xFF, M5STACK_8ANGLE_NUM_LEDS * M5STACK_8ANGLE_BYTES_PER_LED);

  this->effect_data_ = allocator.allocate(M5STACK_8ANGLE_NUM_LEDS);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate effect data of size %u", M5STACK_8ANGLE_NUM_LEDS);
    this->mark_failed();
    return;
  };
  memset(this->effect_data_, 0x00, M5STACK_8ANGLE_NUM_LEDS);

  this->accessor_ = new light::CorrectedBufferColorAccessor(
      this->buf_, this->effect_data_,
      light::ChannelColors{.r = 0, .g = 1, .b = 2, .w = light::ChannelColors::RESERVED_WHITE}, &this->correction_);
}

void M5Stack8AngleLightOutput::write_state(light::LightState *state) {
  for (int i = 0; i < M5STACK_8ANGLE_NUM_LEDS;
       i++) {  // write one LED at a time, otherwise the message will be truncated
    this->parent_->write_register(M5STACK_8ANGLE_REGISTER_RGB_24B + i * M5STACK_8ANGLE_BYTES_PER_LED,
                                  this->buf_ + i * M5STACK_8ANGLE_BYTES_PER_LED, M5STACK_8ANGLE_BYTES_PER_LED);
  }
}

}  // namespace esphome::m5stack_8angle
