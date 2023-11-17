#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sharp_memory_lcd {

class SharpMemoryLCD;

using sharp_memory_lcd_writer_t = std::function<void(SharpMemoryLCD &)>;

class SharpMemoryLCD : public PollingComponent,
                       public display::DisplayBuffer,
                       public spi::SPIDevice<spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void set_writer(sharp_memory_lcd_writer_t &&writer) { this->writer_local_ = writer; }
  void set_height(uint16_t height) { this->height_ = height; }
  void set_width(uint16_t width) { this->width_ = width; }
  void set_extmode_pin(GPIOPin *extmode) { extmode_ = extmode; }
  void set_extcomin_pin(GPIOPin *extcomin) { extcomin_ = extcomin; }
  void set_disp_pin(GPIOPin *disp) { disp_ = disp; }
  void set_invert_color(bool invert_color) { this->invert_color_ = invert_color; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void update() override;
  void fill(Color color) override;
  void write_display_data();

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();
  void display_init_();

  uint16_t width_;
  uint16_t height_;
  GPIOPin *extmode_;
  GPIOPin *extcomin_;
  GPIOPin *disp_;
  bool invert_color_;

  optional<sharp_memory_lcd_writer_t> writer_local_{};

  uint8_t sharpmem_vcom_;
};

}  // namespace sharp_memory_lcd
}  // namespace esphome
