#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace gc9a01 {

class GC9A01 : public PollingComponent,
               public display::DisplayBuffer,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_8MHZ> {
 public:
  GC9A01(int width, int height, int colstart, int rowstart, bool eightbitcolor);
  void dump_config() override;
  void setup() override;

  void display();

  void update() override;

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_reset_pin(GPIOPin *value) { this->reset_pin_ = value; }
  void set_dc_pin(GPIOPin *value) { dc_pin_ = value; }
  size_t get_buffer_length();

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  void sendcommand_(uint8_t cmd, const uint8_t *data_bytes, uint8_t num_data_bytes);
  void senddata_(const uint8_t *data_bytes, uint8_t num_data_bytes);

  void writecommand_(uint8_t value);
  void writedata_(uint8_t value);

  void write_display_data_();

  void init_reset_();
  void display_init_(const uint8_t *addr);
  void set_addr_window_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void spi_master_write_addr_(uint16_t addr1, uint16_t addr2);
  void spi_master_write_color_(uint16_t color, uint16_t size);

  int get_width_internal() override;
  int get_height_internal() override;

  uint8_t colstart_ = 0, rowstart_ = 0;
  bool eightbitcolor_ = true;
  int16_t width_ = 240, height_ = 240;  // Watch heap size

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
};

}  // namespace gc9a01
}  // namespace esphome
