#pragma once

#include "../pipsolar.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace pipsolar {
class Pipsolar;
class PipsolarSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(Pipsolar *parent) { this->parent_ = parent; };
  void set_on_command(std::string command) { this->on_command_ = std::move(command); };
  void set_off_command(std::string command) { this->off_command_ = std::move(command); };
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  std::string on_command_;
  std::string off_command_;
  Pipsolar *parent_;
};

}  // namespace pipsolar
}  // namespace esphome
