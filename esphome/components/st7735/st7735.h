#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "st7735_defines.h"

namespace esphome {
namespace st7735 {

enum ST7735Model {
  ST7735_INITR_GREENTAB = INITR_GREENTAB,
  ST7735_INITR_REDTAB = INITR_REDTAB,
  ST7735_INITR_BLACKTAB = INITR_BLACKTAB,
  ST7735_INITR_MINI_160X80 = INITR_MINI_160X80,
  ST7735_INITR_18BLACKTAB = INITR_18BLACKTAB,
  ST7735_INITR_18REDTAB = INITR_18REDTAB
};

class ST7735 : public PollingComponent,
               public display::DisplayBuffer,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_8MHZ> {
  // TSCYCW = 66ns so 1/66ns = ~ 15MHZ
 public:
  ST7735(ST7735Model model, int width, int height, int colstart, int rowstart, boolean usebgr);
  void dump_config() override;
  void setup() override;
  void update() override;

  void display();
  void set_model(ST7735Model model) { this->model_ = model; }
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void set_reset_pin(GPIOPin *value) { this->reset_pin_ = value; }
  void set_dc_pin(GPIOPin *value) { dc_pin_ = value; }

 protected:
  uint16_t pixel_count_ = 0;
  bool usebgr_ = false;
  bool driver_right_bit_aligned_ = false;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void fill(Color color) override;
  int get_width_internal() override;
  int get_height_internal() override;
  void display_clear() override{};

  void sendcommand_(uint8_t cmd, const uint8_t *data_bytes, uint8_t num_data_bytes);
  void senddata_(const uint8_t *data_bytes, uint8_t num_data_bytes);
  void start_data_();
  void end_data_();
  void writecommand_(uint8_t value);
  void writedata_(uint8_t value);
  void fill_internal_(Color color);
  void display_buffer_();
  void init_reset_();
  void display_init_(const uint8_t *addr);
  void spi_master_write_addr_(uint16_t addr1, uint16_t addr2);
  void set_addr_window_(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h);
  bool is_18bit_();
  const char *model_str_();

  ST7735Model model_{ST7735_INITR_18BLACKTAB};
  uint8_t colstart_ = 0, rowstart_ = 0;
  int16_t width_ = 80, height_ = 80;  // Watch heap size

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
};

}  // namespace st7735
}  // namespace esphome
