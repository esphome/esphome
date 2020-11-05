#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "PxMatrix.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pxmatrix_display {

class PxmatrixDisplay : public PollingComponent, public display::DisplayBuffer {
 public:
  void display();
  void update();
  void setup();
  void fill(int color);



 protected:
  void draw_absolute_pixel_internal(int x, int y, int color) override;
  int get_width_internal() override;
  int get_height_internal() override;

 private:
//  portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
  PxMATRIX pxMatrix;
  int display_loop = 0;
};
}  // namespace pxmatrix_display
}  // namespace esphome
