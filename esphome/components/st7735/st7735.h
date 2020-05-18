#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace st7735 {

#define ST7735_TFTWIDTH_128 128   // for 1.44 and mini
#define ST7735_TFTWIDTH_80 80     // for mini
#define ST7735_TFTHEIGHT_128 128  // for 1.44" display
#define ST7735_TFTHEIGHT_160 160  // for 1.8" and mini display

// Some ready-made 16-bit ('565') color settings:
#define ST77XX_BLACK 0x0000
#define ST77XX_WHITE 0xFFFF
#define ST77XX_RED 0xF800
#define ST77XX_GREEN 0x07E0
#define ST77XX_BLUE 0x001F
#define ST77XX_CYAN 0x07FF
#define ST77XX_MAGENTA 0xF81F
#define ST77XX_YELLOW 0xFFE0
#define ST77XX_ORANGE 0xFC00

// Some ready-made 16-bit ('565') color settings:
#define ST7735_BLACK ST77XX_BLACK
#define ST7735_WHITE ST77XX_WHITE
#define ST7735_RED ST77XX_RED
#define ST7735_GREEN ST77XX_GREEN
#define ST7735_BLUE ST77XX_BLUE
#define ST7735_CYAN ST77XX_CYAN
#define ST7735_MAGENTA ST77XX_MAGENTA
#define ST7735_YELLOW ST77XX_YELLOW
#define ST7735_ORANGE ST77XX_ORANGE

// some flags for initR() :(
#define INITR_GREENTAB 0x00
#define INITR_REDTAB 0x01
#define INITR_BLACKTAB 0x02
#define INITR_18GREENTAB INITR_GREENTAB
#define INITR_18REDTAB INITR_REDTAB
#define INITR_18BLACKTAB INITR_BLACKTAB
#define INITR_144GREENTAB 0x01
#define INITR_MINI160x80 0x04
#define INITR_HALLOWING 0x05

enum ST7735Model {
  ST7735_INITR_GREENTAB = INITR_GREENTAB,
  ST7735_INITR_REDTAB = INITR_REDTAB,
  ST7735_INITR_BLACKTAB = INITR_BLACKTAB,
  S_T7735_INITR_MIN_I160X80 = INITR_MINI160x80,
  ST7735_INITR_18BLACKTAB = INITR_18BLACKTAB,
  ST7735_INITR_18REDTAB = INITR_18REDTAB
};

class ST7735 : public PollingComponent,
               public display::DisplayBuffer,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_8MHZ> {
 public:
  ST7735(ST7735Model model, int width, int height, int colstart, int rowstart, boolean eightbitcolor);
  void dump_config() override;
  void setup() override;

  void display();

  void update() override;

  void set_model(ST7735Model model) { this->model_ = model; }
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_reset_pin(GPIOPin *value) { this->reset_pin_ = value; }
  void set_dc_pin(GPIOPin *value) { dc_pin_ = value; }
  size_t get_buffer_length_();

 protected:
  void sendcommand_(uint8_t cmd, const uint8_t *data_bytes, uint8_t num_data_bytes);
  void senddata_(const uint8_t *data_bytes, uint8_t num_data_bytes);

  void writecommand_(uint8_t value);
  void writedata_(uint8_t value);

  void write_display_data_();

  void init_reset_();
  void display_init_(const uint8_t *addr);
  void set_addr_window_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void draw_absolute_pixel_internal(int x, int y, int color) override;
  void spi_master_write_addr_(uint16_t addr1, uint16_t addr2);
  void spi_master_write_color_(uint16_t color, uint16_t size);

  int get_width_internal() override;
  int get_height_internal() override;
  
  const char *model_str_();

  ST7735Model model_{ST7735_INITR_18BLACKTAB};
  uint8_t colstart_ = 0, rowstart_ = 0;
  boolean eightbitcolor_ = true;
  int16_t width_ = 80, height_ = 80;  // Watch heap size

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
};

}  // namespace st7735
}  // namespace esphome
