#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "pipsolar.h"


namespace esphome {
namespace pipsolar {
class Pipsolar;
class PipsolarTextSensor : public Component, public text_sensor::TextSensor {
 public:
  void set_parent(Pipsolar *parent) {this->parent_ = parent;};
//  void set_command(String command) {this->command_ = command;};
  void dump_config() override;

 protected:
//  void write_state(bool state) override;
//  String command_;
  Pipsolar *parent_;
};

}  // namespace pipsolar
}  // namespace esphome
