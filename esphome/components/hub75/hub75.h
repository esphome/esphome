#pragma once

#include <utility>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

// --- VirtualMatrix additions (optional compile-time include) ---
#ifdef USE_VIRTUAL_PANEL
#include "ESP32-HUB75-VirtualMatrixPanel_T.hpp"
#ifndef VPANEL_CHAIN
#define VPANEL_CHAIN CHAIN_TOP_LEFT_DOWN
#endif
#ifndef VPANEL_SCAN
#define VPANEL_SCAN STANDARD_TWO_SCAN
#endif
using VPanelType = VirtualMatrixPanel_T<VPANEL_CHAIN, ScanTypeMapping<VPANEL_SCAN>, 1>;
#endif
// --- End VirtualMatrix additions ---

namespace esphome {
namespace hub75 {

using esphome::display::ColorBitness;
using esphome::display::ColorOrder;

class HUB75Display : public display::DisplayBuffer {
 public:
  void setup() override;

  void dump_config() override;

  void update() override;

  void set_double_buffer(bool double_buffer) { this->mxconfig_.double_buff = double_buffer; }
  void set_panel_height(int panel_height) { this->mxconfig_.mx_height = panel_height; }
  void set_panel_width(int panel_width) { this->mxconfig_.mx_width = panel_width; }
  void set_chain_length(int chain_length) { this->mxconfig_.chain_length = chain_length; }
  void set_initial_brightness(int brightness) { this->initial_brightness_ = brightness; }
  int get_initial_brightness() { return this->initial_brightness_; }

  void set_pins(InternalGPIOPin *r1_pin, InternalGPIOPin *g1_pin, InternalGPIOPin *b1_pin, InternalGPIOPin *r2_pin,
                InternalGPIOPin *g2_pin, InternalGPIOPin *b2_pin, InternalGPIOPin *a_pin, InternalGPIOPin *b_pin,
                InternalGPIOPin *c_pin, InternalGPIOPin *d_pin, InternalGPIOPin *e_pin, InternalGPIOPin *lat_pin,
                InternalGPIOPin *oe_pin, InternalGPIOPin *clk_pin) {
    this->mxconfig_.gpio = {
        static_cast<int8_t>(r1_pin->get_pin()), static_cast<int8_t>(g1_pin->get_pin()),
        static_cast<int8_t>(b1_pin->get_pin()), static_cast<int8_t>(r2_pin->get_pin()),
        static_cast<int8_t>(g2_pin->get_pin()), static_cast<int8_t>(b2_pin->get_pin()),
        static_cast<int8_t>(a_pin->get_pin()), static_cast<int8_t>(b_pin->get_pin()),
        static_cast<int8_t>(c_pin->get_pin()), static_cast<int8_t>(d_pin->get_pin()),
        static_cast<int8_t>(e_pin != nullptr ? e_pin->get_pin() : -1),  // Set the e pin to -1 as required by the
                                                                        // library if it is not used
        static_cast<int8_t>(lat_pin->get_pin()), static_cast<int8_t>(oe_pin->get_pin()),
        static_cast<int8_t>(clk_pin->get_pin())};
  }

  void set_driver(HUB75_I2S_CFG::shift_driver driver) { this->mxconfig_.driver = driver; }
  void set_i2sspeed(HUB75_I2S_CFG::clk_speed speed) { this->mxconfig_.i2sspeed = speed; }
  void set_latch_blanking(int latch_blanking) { this->mxconfig_.latch_blanking = latch_blanking; }
  void set_clock_phase(bool clock_phase) { this->mxconfig_.clkphase = clock_phase; }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

  void fill(Color color) override;
  void filled_rectangle(int x1, int y1, int width, int height, Color color = display::COLOR_ON);
  void set_brightness(int brightness);

#ifdef USE_VIRTUAL_PANEL
  void set_virtual_rows(int rows) { this->v_rows_ = (rows < 1) ? 1 : (rows > 255 ? 255 : rows); }
  void set_virtual_cols(int cols) { this->v_cols_ = (cols < 1) ? 1 : (cols > 255 ? 255 : cols); }
#endif

 protected:
  MatrixPanel_I2S_DMA *dma_display_{nullptr};

#ifdef USE_VIRTUAL_PANEL
  VPanelType *virtual_display_{nullptr};
  uint8_t v_rows_{1};
  uint8_t v_cols_{1};
#endif

  HUB75_I2S_CFG mxconfig_;
  int initial_brightness_{128};
  bool enabled_{false};

  int get_width_internal() override {
#ifdef USE_VIRTUAL_PANEL
    return this->mxconfig_.mx_width * static_cast<int>(this->v_cols_);
#else
    return this->mxconfig_.mx_width * this->mxconfig_.chain_length;
#endif
  };

  int get_height_internal() override {
#ifdef USE_VIRTUAL_PANEL
    return this->mxconfig_.mx_height * static_cast<int>(this->v_rows_);
#else
    return this->mxconfig_.mx_height;
#endif
  };

  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;

 private:
  inline void draw_pixel_rgb888_(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
#ifdef USE_VIRTUAL_PANEL
    if (this->virtual_display_) {
      this->virtual_display_->drawPixelRGB888(x, y, r, g, b);
      return;
    }
#endif
    this->dma_display_->drawPixelRGB888(x, y, r, g, b);
  }

  inline void fill_screen_rgb888_(uint8_t r, uint8_t g, uint8_t b) {
#ifdef USE_VIRTUAL_PANEL
    if (this->virtual_display_) {
      this->virtual_display_->fillScreenRGB888(r, g, b);
      return;
    }
#endif
    this->dma_display_->fillScreenRGB888(r, g, b);
  }
};

}  // namespace hub75
}  // namespace esphome
