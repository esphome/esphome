#pragma once

#include "esphome/core/component.h"
#include "esphome/components/humidifier/humidifier.h"

namespace esphome {
namespace custom {

class CustomHumidifierConstructor {
 public:
  CustomHumidifierConstructor(const std::function<std::vector<humidifier::Humidifier *>()> &init) {
    this->humidifiers_ = init();
  }

  humidifier::Humidifier *get_humidifier(int i) { return this->humidifiers_[i]; }

 protected:
  std::vector<humidifier::Humidifier *> humidifiers_;
};

}  // namespace custom
}  // namespace esphome
