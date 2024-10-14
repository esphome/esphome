#include "graphical_layout.h"

#include "esphome/components/display/display.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "rootlayoutcomponent";

void RootLayoutComponent::setup() {
  ESP_LOGD(TAG, "Calling setup_complete on layout_root");
  this->layout_root_->setup_complete();
}

void RootLayoutComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Graphical Layout");
  this->layout_root_->dump_config(2, 2);
}

void RootLayoutComponent::render_at(display::Display *display, int x, int y) {
  display->set_local_coordinate(x, y);

  display::Rect layout_rect = this->layout_root_->measure_item(display);
  display::Rect clipping_rect = display::Rect(x, y, layout_rect.w, layout_rect.h);

  // TODO: Should clipping be relative to local?
  display->start_clipping(clipping_rect);

  // Render everything
  this->layout_root_->render(display, layout_rect);

  display->pop_local_coordinates();
  display->shrink_clipping(clipping_rect);
}

}  // namespace graphical_layout
}  // namespace esphome
