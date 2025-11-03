#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome {
namespace demo {

class DemoSelect : public select::Select, public Component {
 protected:
  void control(size_t index) override { this->publish_state(index); }
};

}  // namespace demo
}  // namespace esphome
