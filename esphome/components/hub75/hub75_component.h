#pragma once

#ifdef USE_ESP32

#include <utility>

#include "esphome/components/display/display_buffer.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "hub75.h"  // hub75 library

namespace esphome::hub75 {

using esphome::display::ColorBitness;
using esphome::display::ColorOrder;

class HUB75Display : public display::Display {
 public:
  // Constructor accepting config
  explicit HUB75Display(const Hub75Config &config);

  // Core Component methods
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // Display/PollingComponent methods
  void update() override;
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;

  // Brightness control (runtime mutable)
  void set_brightness(uint8_t brightness);

 protected:
  // Display internal methods
  int get_width_internal() override { return this->driver_ != nullptr ? this->driver_->get_width() : 0; }
  int get_height_internal() override { return this->driver_ != nullptr ? this->driver_->get_height() : 0; }

  // Member variables
  Hub75Driver *driver_{nullptr};
  Hub75Config config_;  // Immutable configuration

  // Runtime state (mutable)
  uint8_t brightness_{128};
  bool enabled_{false};
};

template<typename... Ts> class SetBrightnessAction : public Action<Ts...>, public Parented<HUB75Display> {
 public:
  TEMPLATABLE_VALUE(uint8_t, brightness)

  void play(const Ts &...x) override { this->parent_->set_brightness(this->brightness_.value(x...)); }
};

}  // namespace esphome::hub75

#endif
