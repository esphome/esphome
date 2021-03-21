#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "pipsolar.h"


namespace esphome {
namespace pipsolar {
class Pipsolar;
class PipsolarSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(Pipsolar *parent) {this->parent_ = parent;};
  void set_on_command(String command) {this->on_command_ = command;};
  void set_off_command(String command) {this->off_command_ = command;};
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  String on_command_;
  String off_command_;
  Pipsolar *parent_;
};

}  // namespace pipsolar
}  // namespace esphome
