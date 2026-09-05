#pragma once

#include "comp_climate.h"
#include "esphome/components/select/select.h"

namespace esphome::sharp_ac {

class VaneSelectVertical : public select::Select, public Parented<SharpAc> {
 public:
  VaneSelectVertical() = default;

  void control(const std::string &value) override {
    SwingVertical pos = SwingVertical::AUTO_POSITION;
    if (value == "auto") {
      pos = SwingVertical::AUTO_POSITION;
    } else if (value == "swing") {
      pos = SwingVertical::SWING;
    } else if (value == "up") {
      pos = SwingVertical::HIGHEST;
    } else if (value == "up_center") {
      pos = SwingVertical::HIGH_POS;
    } else if (value == "center") {
      pos = SwingVertical::MID;
    } else if (value == "down_center") {
      pos = SwingVertical::LOW_POS;
    } else if (value == "down") {
      pos = SwingVertical::LOWEST;
    }
    this->parent_->set_vane_vertical(pos);
  }

  void set_val(SwingVertical val) {
    switch (val) {
      case SwingVertical::AUTO_POSITION:
        this->publish_state("auto");
        break;
      case SwingVertical::SWING:
        this->publish_state("swing");
        break;
      case SwingVertical::HIGHEST:
        this->publish_state("up");
        break;
      case SwingVertical::HIGH_POS:
        this->publish_state("up_center");
        break;
      case SwingVertical::MID:
        this->publish_state("center");
        break;
      case SwingVertical::LOW_POS:
        this->publish_state("down_center");
        break;
      case SwingVertical::LOWEST:
        this->publish_state("down");
        break;
    }
  }
};

}  // namespace esphome::sharp_ac
