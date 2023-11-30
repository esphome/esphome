#pragma once
#include "esphome/core/component.h"


namespace esphome {
namespace panel_driver {

// for now, assume big-endian to match typical displays.
enum ColorMode : uint8_t { COLOR_MODE_RGB, COLOR_MODE_BGR, COLOR_MODE_GRB, COLOR_MODE_MONO };

/**
 * A PanelDriver is a platform representing an interface to 2D display panel, typically an LCD, AMOLED or ePaper screen.
 * The implementation is provided by another component implementing the PanelDriver platform.
 */
class PanelDriver: public esphome::Component {
 public:
  PanelDriver(){};
  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override {};
  /**
   * Draw pixels from the given buffer to the display. The format of the pixels must conform that that implemented
   * by this PanelDriver. The data at src_ptr need not be preserved after the call.
   * @param x Where to draw on the display
   * @param y Where to draw on the display
   * @param width Number of pixels wide to draw
   * @param height Number of pixels high
   * @param src_ptr The data to be drawn. Pixels must be sequential within this buffer.
   */
  virtual void draw_pixels_at(size_t x, size_t y, size_t width, size_t height, const void * src_ptr) = 0;
  /**
   * @return The driver color mode. Not settable here since the driver may have rigid ideas about it.
   */
  virtual ColorMode get_color_mode() = 0;

  /**
   * @return The number of bits representing a pixel. 16 is likely to be the most common choice.
   */
  virtual size_t get_pixel_bits() { return 16; }


  void set_swap_xy(bool swap_xy) { this->swap_xy_ = swap_xy; }
  void set_mirror_x(bool mirror_x) { this->mirror_x_ = mirror_x; }
  void set_mirror_y(bool mirror_y) { this->mirror_y_ = mirror_y; }
  void set_width(size_t width) { this->width_ = width; }
  void set_offset_width(size_t offset_width) { this->offset_width_ = offset_width; }
  void set_height(size_t height) { this->height_ = height; }
  void set_offset_height(size_t offset_height) { this->offset_height_ = offset_height; }

 protected:
  bool mirror_x_{};
  bool mirror_y_{};
  bool swap_xy_{};
  size_t width_{};
  size_t offset_width_{};
  size_t height_{};
  size_t offset_height_{};
};

} // namespace panel_driver
} // namespace esphome
