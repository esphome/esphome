#pragma once

#include <utility>

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/components/font/font.h"

namespace esphome {
namespace graphical_layout {

/* See display.h for original declaration */
using display_writer_t = std::function<void(display::Display &)>;

/** The DisplayRenderingPanel is a UI item that renders a custom lambda to the display whilst
 * participating in the layout process
 */
class DisplayRenderingPanel : public LayoutItem {
 public:
  display::Rect measure_item(display::Display *display) override;
  void render(display::Display *display, display::Rect bounds) override;
  void dump_config(int indent_depth, int additional_level_depth) override;

  void set_width(int width) { this->width_ = width; };
  void set_height(int height) { this->height_ = height; };
  void set_lambda(display_writer_t lambda) { this->lambda_ = std::move(lambda); };

 protected:
  int width_{0};
  int height_{0};
  display_writer_t lambda_{nullptr};
};

}  // namespace graphical_layout
}  // namespace esphome
