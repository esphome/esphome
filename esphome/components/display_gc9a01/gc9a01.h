#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"

namespace esphome {
namespace gc9a01 {

class GC9A01 : public PollingComponent,
               public display::Display,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_40MHZ> {
 public:
  GC9A01(int width, int height, int colstart, int rowstart);
  void dump_config() override;
  void setup() override;

  void update() override;

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_reset_pin(GPIOPin *value) { this->reset_pin_ = value; }
  void set_dc_pin(GPIOPin *value) { dc_pin_ = value; }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width() override { return this->width_; }
  int get_height() override { return this->height_; }

  display::PixelFormat get_native_pixel_format() override { return display::PixelFormat::RGB565_BE; }
  bool draw_pixels_(int x, int y, int w, int h, const uint8_t *data, int data_line_size, int data_stride) override;
  void draw_pixel_at(int x, int y, Color color) override;

 public:
  void sendcommand_(uint8_t cmd, const uint8_t *data_bytes, uint8_t num_data_bytes);
  void senddata_(const uint8_t *data_bytes, uint8_t num_data_bytes);

  void writecommand_(uint8_t value);
  void writedata_(uint8_t value);

  void init_reset_();
  void display_init_(const uint8_t *addr);
  void set_addr_window_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void spi_master_write_addr_(uint16_t addr1, uint16_t addr2);

  uint8_t colstart_ = 0, rowstart_ = 0;
  int16_t width_ = 240, height_ = 240;

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
};

}  // namespace gc9a01
}  // namespace esphome
