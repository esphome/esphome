#pragma once
#include "esphome/core/component.h"

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/display_color_utils.h"

#include "epd_driver.h"
#include "epd_highlevel.h"

namespace esphome {
namespace lilygo_t5_47_display {

// LilyGo-EPD47
class LilygoT547Display : public PollingComponent, public display::DisplayBuffer {
 protected:
  EpdiyHighlevelState hl;
  // ambient temperature around device
  uint8_t *fb;
  enum EpdDrawError err;

 public:
  float get_setup_priority() const override;

  void set_clear_screen(bool clear);
  void set_landscape(bool landscape);
  void set_temperature(uint32_t temperature);

  int get_width_internal();

  int get_height_internal();

  void setup() override;

  void update() override;

  void clear();
  // Was screen cleared at leat once
  bool was_cleared();
  void flush_screen_changes();
  void set_all_white();
  void poweron();
  void poweroff();
  void on_shutdown() override;

 protected:
  void HOT draw_absolute_pixel_internal(int x, int y, Color color) override;

  bool clear_;
  bool was_cleared_ = false;
  bool temperature_;
  bool landscape_;
};

}  // namespace lilygo_t5_47_display
}  // namespace esphome
