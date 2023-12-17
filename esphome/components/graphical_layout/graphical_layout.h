#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/display/rect.h"
#include "esphome/components/graphical_layout/layout_item.h"

namespace esphome {
namespace display {
class Display;
class Rect;
}  // namespace display

namespace graphical_layout {

/** Component used for rendering the layout*/
class RootLayoutComponent : public Component {
public:
  void setup() override;
  void dump_config() override;

  /** Render the graphical layout to the screen
   *
   * param[in] display: Display that will be rendered to
   * param[in] x: x coordinate to render at
   * param[in] y: y coorindate to render at
   */
  void render_at(display::Display *display, int x, int y);
  
  void set_layout_root(LayoutItem *layout) { this->layout_root_ = layout; };

protected:
  LayoutItem *layout_root_{nullptr};
};

}  // namespace graphical_layout
}  // namespace esphome
