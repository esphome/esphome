#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "PxMatrix.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pxmatrix_display {

enum DriverChips {
  SHIFT = driver_chips::SHIFT,
  FM6124 = driver_chips::FM6124,
  FM6126A = driver_chips::FM6126A,
};
enum ColorOrders {
  RRGGBB = color_orders::RRGGBB,
  RRBBGG = color_orders::RRBBGG,
  GGRRBB = color_orders::GGRRBB,
  GGBBRR = color_orders::GGBBRR,
  BBRRGG = color_orders::BBRRGG,
  BBGGRR = color_orders::BBGGRR,
};
enum ScanPatterns {
  LINE = scan_patterns::LINE,
  ZIGZAG = scan_patterns::ZIGZAG,
  VZAG = scan_patterns::VZAG,
  WZAGZIG = scan_patterns::WZAGZIG,
  ZAGGIZ = scan_patterns::ZAGGIZ,
  ZZAGG = scan_patterns::ZZAGG,
};
enum MuxPatterns {
  BINARY = mux_patterns::BINARY,
  STRAIGHT = mux_patterns::STRAIGHT,
};

class PxmatrixDisplay : public PollingComponent, public display::DisplayBuffer {
 public:
  void display() override;
  void setup() override;
  void update() override;
  void fill(Color color) override;

  void set_pin_latch(GPIOPin *pin_latch);
  void set_pin_a(GPIOPin *pin_a);
  void set_pin_b(GPIOPin *pin_b);
  void set_pin_c(GPIOPin *pin_c);
  void set_pin_d(GPIOPin *pin_d);
  void set_pin_e(GPIOPin *pin_e);
  void set_pin_oe(GPIOPin *pin_oe);
  void set_width(uint8_t width);
  void set_height(uint8_t height);
  void set_brightness(uint8_t brightness);
  void set_row_patter(uint8_t row_pattern);
  void set_driver_chips(DriverChips driver_chips);
  void set_color_orders(ColorOrders color_orders);
  void set_scan_patterns(ScanPatterns scan_patterns);
  void set_mux_patterns(MuxPatterns mux_patterns);

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override;
  int get_height_internal() override;

  PxMATRIX *pxMatrix;

  GPIOPin *pin_LATCH_{nullptr};
  GPIOPin *pin_A_{nullptr};
  GPIOPin *pin_B_{nullptr};
  GPIOPin *pin_C_{nullptr};
  GPIOPin *pin_D_{nullptr};
  GPIOPin *pin_E_{nullptr};
  GPIOPin *pin_OE_{nullptr};

  uint8_t width_ = 32;
  uint8_t height_ = 32;
  uint8_t brightness_ = 255;
  uint8_t row_pattern_ = 16;

  DriverChips driver_chips_ = DriverChips::FM6124;
  ColorOrders color_orders_ = ColorOrders::RRGGBB;
  ScanPatterns scan_patterns_ = ScanPatterns::LINE;
  MuxPatterns mux_patterns_ = MuxPatterns::BINARY;
  // block_patterns BLOCK_PATTERN = block_patterns::ABCD;
};

}  // namespace pxmatrix_display
}  // namespace esphome
