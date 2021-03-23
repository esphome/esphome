#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "ili9341_defines.h"
#include "ili9341_init.h"

namespace esphome {
namespace ili9341 {

enum ILI9341Model {
  M5STACK = 0,
  TFT_24,
};

class ILI9341Display : public PollingComponent,
                       public display::DisplayBuffer,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_40MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_led_pin(GPIOPin *led) { this->led_pin_ = led; }
  void set_model(ILI9341Model model) { this->model_ = model; }

  void command(uint8_t value);
  void data(uint8_t value);
  void send_command(uint8_t command_byte, const uint8_t *data_bytes, uint8_t num_data_bytes);

  virtual void initialize() = 0;

  void update() override;

  void dump_config() override;

  void set_device_width(uint16_t width) override {
    if (width == 0 || width > 240)
      width = 240;

    DisplayBuffer::set_device_width(width);
  }
  void set_device_height(uint16_t height) override {
    if (height == 0 || height > 320)
      height = 320;

    DisplayBuffer::set_device_height(height);
  }
  void set_row_start(uint16_t row_start) override {
    if (row_start > 239)  // At least one row
      row_start = 239;

    DisplayBuffer::set_row_start(row_start);
  }
  void set_col_start(uint16_t col_start) override {
    if (col_start > 319)  // At least one column
      col_start = 319;

    DisplayBuffer::set_col_start(col_start);
  }

  void setup() override {
    this->setup_pins_();
    this->initialize();
  }

 protected:
  // void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void setup_pins_();

  void init_lcd_(const uint8_t *init_cmd);
  void set_addr_window_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void invert_display_(bool invert);
  void reset_();
  void fill(Color color) override;
  void display_();
  uint16_t convert_to_16bit_color_(uint8_t color_8bit);
  uint8_t convert_to_8bit_color_(uint16_t color_16bit);

  ILI9341Model model_;
  // int16_t width_{320};   ///< Display width as modified by current rotation
  // int16_t height_{240};  ///< Display height as modified by current rotation

  // uint32_t get_buffer_length_();
  int get_width_internal() override;
  int get_height_internal() override;
  void display_clear() override;

  void start_command_();
  void end_command_();
  void start_data_();
  void end_data_();
  bool is_18bit_();
  bool driver_right_bit_aligned_ = false;

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *led_pin_{nullptr};
  GPIOPin *dc_pin_;
  GPIOPin *busy_pin_{nullptr};
};

//-----------   M5Stack display --------------
class ILI9341M5Stack : public ILI9341Display {
 public:
  void initialize() override;
};

//-----------   ILI9341_24_TFT display --------------
class ILI9341TFT24 : public ILI9341Display {
 public:
  void initialize() override;
};
}  // namespace ili9341
}  // namespace esphome
