#pragma once

#ifdef USE_ESP32

#include "esphome/components/fendt_caravan/caravan_component_base.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/fendt_caravan/variable.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"

namespace esphome::fendt_caravan {

using namespace std;

class FendtTextSensor : public CaravanComponentBase<std::string>, public text_sensor::TextSensor {
 public:
  void setup() override {
    if (this->key_name_.empty())
      return;
    auto *variable = static_cast<Variable<std::string> *>(this->get_parent()->get_variable(this->key_name_));
    if (variable != nullptr) {
      this->set_variable(variable);
    }
  }

 protected:
  void on_decoded(const std::string &value) override;
};
}  // namespace esphome::fendt_caravan
#endif
