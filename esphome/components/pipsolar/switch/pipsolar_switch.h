#pragma once

#include "../pipsolar.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace pipsolar {
class Pipsolar;
class PipsolarSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(Pipsolar *parent) { this->parent_ = parent; }
  void set_on_command(const char *command) { this->on_command_ = command; }
  void set_off_command(const char *command) { this->off_command_ = command; }
  /// Prevent accidental use of std::string which would dangle
  void set_on_command(const std::string &command) = delete;
  void set_off_command(const std::string &command) = delete;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  const char *on_command_{nullptr};
  const char *off_command_{nullptr};
  Pipsolar *parent_;
};

}  // namespace pipsolar
}  // namespace esphome
