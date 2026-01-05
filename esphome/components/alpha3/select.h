#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "alpha3.h"

#ifdef USE_ESP32

namespace esphome {
namespace alpha3 {

class Alpha3Select : public select::Select, public Component {
 public:
  void set_parent(Alpha3 *parent) { this->parent_ = parent; }

 protected:
  void control(const std::string &value) override;
  Alpha3 *parent_{nullptr};
};

}  // namespace alpha3
}  // namespace esphome

#endif
