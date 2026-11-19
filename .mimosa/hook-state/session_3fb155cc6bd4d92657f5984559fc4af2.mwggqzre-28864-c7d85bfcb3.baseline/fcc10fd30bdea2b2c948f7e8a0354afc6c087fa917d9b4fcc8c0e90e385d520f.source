#include "m5stack_8angle_light.h"

#include "esphome/core/log.h"

namespace esphome::m5stack_8angle {

ESPHOME_LOG_TAG(TAG, "m5stack_8angle.light");

void M5Stack8AngleLightOutput::write_state(light::LightState *state) {
  for (int i = 0; i < M5STACK_8ANGLE_NUM_LEDS;
       i++) {  // write one LED at a time, otherwise the message will be truncated
    this->parent_->write_register(M5STACK_8ANGLE_REGISTER_RGB_24B + i * M5STACK_8ANGLE_BYTES_PER_LED,
                                  this->internal_accessor_.color_data + i * M5STACK_8ANGLE_BYTES_PER_LED,
                                  M5STACK_8ANGLE_BYTES_PER_LED);
  }
}

}  // namespace esphome::m5stack_8angle
