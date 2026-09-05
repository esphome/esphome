#pragma once

#include "comp_climate.h"
#include "esphome/components/select/select.h"

namespace esphome::sharp_ac {

class VaneSelectHorizontal : public select::Select, public Parented<SharpAc> {
 public:
  VaneSelectHorizontal() = default;

  void set_val(SwingHorizontal val) {
    switch (val) {
      case SwingHorizontal::SWING:
        this->publish_state("swing");
        break;
      case SwingHorizontal::LEFT:
        this->publish_state("left");
        break;
      case SwingHorizontal::MIDDLE:
        this->publish_state("center");
        break;
      case SwingHorizontal::RIGHT:
        this->publish_state("right");
        break;
    }
  }

  void control(const std::string &value) override {
    SwingHorizontal pos = SwingHorizontal::MIDDLE;
    if (value == "swing") {
      pos = SwingHorizontal::SWING;
    } else if (value == "left") {
      pos = SwingHorizontal::LEFT;
    } else if (value == "center") {
      pos = SwingHorizontal::MIDDLE;
    } else if (value == "right") {
      pos = SwingHorizontal::RIGHT;
    }
    this->parent_->set_vane_horizontal(pos);
  }
};

}  // namespace esphome::sharp_ac
