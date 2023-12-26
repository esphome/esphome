#pragma once

#include <utility>

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/components/font/font.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace graphical_layout {

/* See display.h for original declaration */
using display_writer_t = std::function<void(display::Display &)>;

/** The DisplayRenderingPanel is a UI item that renders a custom lambda to the display whilst
 * participating in the layout process
 */
class DisplayRenderingPanel : public LayoutItem {
 public:
  display::Rect measure_item_internal(display::Display *display) override;
  void render_internal(display::Display *display, display::Rect bounds) override;
  void dump_config(int indent_depth, int additional_level_depth) override;

  template<typename V> void set_width(V width) { this->width_ = width; };
  template<typename V> void set_height(V height) { this->height_ = height; };
  void set_lambda(display_writer_t lambda) { this->lambda_ = std::move(lambda); };

 protected:
  TemplatableValue<int> width_{0};
  TemplatableValue<int> height_{0};
  display_writer_t lambda_{nullptr};
};

}  // namespace graphical_layout
}  // namespace esphome
