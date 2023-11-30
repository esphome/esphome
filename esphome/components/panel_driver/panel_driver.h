#pragma once
#include "esphome/core/component.h"


namespace esphome {
namespace panel_driver {

class PanelDriver: esphome::Component {

 public:
  virtual void draw_pixels_at() = 0;

};

} // namespace panel_driver
} // namespace esphome
