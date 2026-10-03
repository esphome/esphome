#include "m5stack_8angle_light.h"

#include "esphome/core/log.h"

namespace esphome::m5stack_8angle {

static const char *const TAG = "m5stack_8angle.light";

void M5Stack8AngleLightOutput::setup() {
  RAMAllocator<uint8_t> allocator;
  if (!this->buffer_.allocate_and_setup(&allocator)) {
    ESP_LOGE(TAG, "Cannot allocate color buffer");
    this->mark_failed();
    return;
  }
  memset(this->buffer_.get_led_data(), 0xFF, M5STACK_8ANGLE_NUM_LEDS * M5STACK_8ANGLE_BYTES_PER_LED);
}

void M5Stack8AngleLightOutput::write_state(light::LightState *state) {
  if (!this->is_ready()) {
    return;
  }
  for (int i = 0; i < M5STACK_8ANGLE_NUM_LEDS;
       i++) {  // write one LED at a time, otherwise the message will be truncated
    this->parent_->write_register(M5STACK_8ANGLE_REGISTER_RGB_24B + i * M5STACK_8ANGLE_BYTES_PER_LED,
                                  this->buffer_.get_led_data() + i * M5STACK_8ANGLE_BYTES_PER_LED,
                                  M5STACK_8ANGLE_BYTES_PER_LED);
  }
}

}  // namespace esphome::m5stack_8angle
