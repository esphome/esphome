#pragma once

namespace esphome {
namespace panel_driver {

class PanelDriver {

 public:
  virtual void draw_pixels_in_window() = 0;

};

} // namespace panel_driver
} // namespace esphome
