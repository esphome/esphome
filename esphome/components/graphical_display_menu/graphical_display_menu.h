#pragma once

#include "esphome/components/display_menu_base/display_menu_base.h"
#include "esphome/components/display/display_buffer.h"
#include <stdlib.h>

namespace esphome {
namespace graphical_display_menu {
struct Position {
  int x;
  int y;
};

struct Dimension {
  int width;
  int height;
};

class GraphicalDisplayMenu : public display_menu_base::DisplayMenuComponent {
 public:
  void setup() override;
  void dump_config() override;

  void set_display_buffer(display::DisplayBuffer *display);
  void set_display_updater(PollingComponent *display_updater);
  void set_font(display::Font *font);

 protected:
  virtual void draw_menu();
  virtual void draw_item(const display_menu_base::MenuItem *item, uint8_t row, bool selected);
  virtual Dimension measure_item(const display_menu_base::MenuItem *item, bool selected);
  virtual void draw_item(const display_menu_base::MenuItem *item, const Position *position,
                         const Dimension *measured_dimensions, bool selected);
  virtual void update();

  display::DisplayBuffer *display_buffer_{nullptr};
  PollingComponent *display_updater_{nullptr};
  display::Font *font_{nullptr};
};

}  // namespace graphical_display_menu
}  // namespace esphome
