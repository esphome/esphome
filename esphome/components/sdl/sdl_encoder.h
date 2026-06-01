#pragma once

#ifdef USE_HOST
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/rotary_encoder/rotary_encoder.h"
#include "sdl_esphome.h"

namespace esphome::sdl {

class SdlEncoder : public rotary_encoder::RotaryEncoderSensor {
 public:
  void setup() override;
  void dump_config() override;

  void set_parent(Sdl *parent) { this->parent_ = parent; }
  void set_wrap(bool wrap) { this->wrap_ = wrap; }

  void handle_wheel_event(int32_t wheel_delta);

 protected:
  Sdl *parent_{nullptr};
  bool wrap_{true};
};

}  // namespace esphome::sdl

#endif
