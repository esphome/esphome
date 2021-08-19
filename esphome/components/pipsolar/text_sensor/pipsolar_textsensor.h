#pragma once

#include "../pipsolar.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace pipsolar {
class Pipsolar;
class PipsolarTextSensor : public Component, public text_sensor::TextSensor {
 public:
  void set_parent(Pipsolar *parent) { this->parent_ = parent; };
  void dump_config() override;

 protected:
  Pipsolar *parent_;
};

}  // namespace pipsolar
}  // namespace esphome
