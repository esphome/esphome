#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace copy {

class CopyButton : public button::Button, public Component {
 public:
  void set_source(button::Button *source) { source_ = source; }
  void dump_config() override;

 protected:
  void press_action() override;

  button::Button *source_;
};

}  // namespace copy
}  // namespace esphome
