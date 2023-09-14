#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../econet.h"

namespace esphome {
namespace econet {

class EconetNumber : public number::Number, public Component, public EconetClient {
 public:
  void setup() override;
  void dump_config() override;
  void set_number_id(const std::string &number_id) { this->number_id_ = number_id; }

 protected:
  void control(float value) override;

  std::string number_id_{""};
  EconetDatapointType type_{};
};

}  // namespace econet
}  // namespace esphome
