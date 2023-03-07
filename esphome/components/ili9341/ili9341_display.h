#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "ili9341_defines.h"
#include "ili9341_init.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ili9341 {

enum ILI9341Model {
  M5STACK = 0,
  TFT_24,
  TFT_24R,
};

enum ILI9341ColorMode {
  BITS_8,
  BITS_8_INDEXED,
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
  void set_palette(const uint8_t *palette) { this->palette_ = palette; }
  void set_buffer_color_mode(ILI9341ColorMode color_mode) { this->buffer_color_mode_ = color_mode; }

  void command(uint8_t value);
  void data(uint8_t value);
  void send_command(uint8_t command_byte, const uint8_t *data_bytes, uint8_t num_data_bytes);
  uint8_t read_command(uint8_t command_byte, uint8_t index);
  virtual void initialize() = 0;

  void update() override;

  void fill(Color color) override;

  void dump_config() override;
  void setup() override {
    this->setup_pins_();
    this->initialize();

    this->x_low_ = this->width_;
    this->y_low_ = this->height_;
    this->x_high_ = 0;
    this->y_high_ = 0;

    this->init_internal_(this->get_buffer_length_());
    this->fill_internal_(0x00);
  }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void setup_pins_();

  void init_lcd_(const uint8_t *init_cmd);
  void set_addr_window_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void invert_display_(bool invert);
  void reset_();
  void fill_internal_(uint8_t color);
  void display_();
  void rotate_my_(uint8_t m);

  ILI9341Model model_;
  int16_t width_{320};   ///< Display width as modified by current rotation
  int16_t height_{240};  ///< Display height as modified by current rotation
  uint16_t x_low_{0};
  uint16_t y_low_{0};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
  const uint8_t *palette_;

  ILI9341ColorMode buffer_color_mode_{BITS_8};

  uint32_t get_buffer_length_();
  int get_width_internal() override;
  int get_height_internal() override;

  void start_command_();
  void end_command_();
  void start_data_();
  void end_data_();

  uint8_t transfer_buffer_[64];

  uint32_t buffer_to_transfer_(uint32_t pos, uint32_t sz);

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

//-----------   ILI9341_24_TFT rotated display --------------
class ILI9341TFT24R : public ILI9341Display {
 public:
  void initialize() override;
};

}  // namespace ili9341
}  // namespace esphome
