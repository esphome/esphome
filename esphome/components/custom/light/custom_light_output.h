#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace custom {

class CustomLightOutputConstructor {
 public:
  CustomLightOutputConstructor(const std::function<std::vector<light::LightOutput *>()> &init) {
    this->outputs_ = init();
  }

  light::LightOutput *get_light(int i) { return this->outputs_[i]; }

 protected:
  std::vector<light::LightOutput *> outputs_;
};

}  // namespace custom
}  // namespace esphome
