#pragma once

#ifdef USE_ESP32_VARIANT_ESP32S3
#include "esphome/core/gpio.h"
#include "esphome/components/display/display.h"
#include "esp_lcd_panel_ops.h"
#ifdef USE_SPI
#include "esphome/components/spi/spi.h"
#endif

namespace esphome {
namespace mipi_rgb {

constexpr static const char *const TAG = "display.mipi_rgb";
const uint8_t SW_RESET_CMD = 0x01;
const uint8_t SLEEP_OUT = 0x11;
const uint8_t SDIR_CMD = 0xC7;
const uint8_t MADCTL_CMD = 0x36;
const uint8_t INVERT_OFF = 0x20;
const uint8_t INVERT_ON = 0x21;
const uint8_t DISPLAY_ON = 0x29;
const uint8_t CMD2_BKSEL = 0xFF;
const uint8_t CMD2_BK0[5] = {0x77, 0x01, 0x00, 0x00, 0x10};

class MipiRgb : public display::Display {
 public:
  MipiRgb(int width, int height) : width_(width), height_(height) {}
  void setup() override;
  void loop() override;
  void update() override;
  void fill(Color color);
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;
  void write_to_display_(int x_start, int y_start, int w, int h, const uint8_t *ptr, int x_offset, int y_offset,
                         int x_pad);
  bool check_buffer_();

  display::ColorOrder get_color_mode() { return this->color_mode_; }
  void set_color_mode(display::ColorOrder color_mode) { this->color_mode_ = color_mode; }
  void set_invert_colors(bool invert_colors) { this->invert_colors_ = invert_colors; }
  void set_madctl(uint8_t madctl) { this->madctl_ = madctl; }

  void add_data_pin(InternalGPIOPin *data_pin, size_t index) { this->data_pins_[index] = data_pin; };
  void set_de_pin(InternalGPIOPin *de_pin) { this->de_pin_ = de_pin; }
  void set_pclk_pin(InternalGPIOPin *pclk_pin) { this->pclk_pin_ = pclk_pin; }
  void set_vsync_pin(InternalGPIOPin *vsync_pin) { this->vsync_pin_ = vsync_pin; }
  void set_hsync_pin(InternalGPIOPin *hsync_pin) { this->hsync_pin_ = hsync_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_width(uint16_t width) { this->width_ = width; }
  void set_pclk_frequency(uint32_t pclk_frequency) { this->pclk_frequency_ = pclk_frequency; }
  void set_pclk_inverted(bool inverted) { this->pclk_inverted_ = inverted; }
  void set_model(const char *model) { this->model_ = model; }
  int get_width() override;
  int get_height() override;
  void set_hsync_back_porch(uint16_t hsync_back_porch) { this->hsync_back_porch_ = hsync_back_porch; }
  void set_hsync_front_porch(uint16_t hsync_front_porch) { this->hsync_front_porch_ = hsync_front_porch; }
  void set_hsync_pulse_width(uint16_t hsync_pulse_width) { this->hsync_pulse_width_ = hsync_pulse_width; }
  void set_vsync_pulse_width(uint16_t vsync_pulse_width) { this->vsync_pulse_width_ = vsync_pulse_width; }
  void set_vsync_back_porch(uint16_t vsync_back_porch) { this->vsync_back_porch_ = vsync_back_porch; }
  void set_vsync_front_porch(uint16_t vsync_front_porch) { this->vsync_front_porch_ = vsync_front_porch; }
  void set_enable_pins(std::vector<GPIOPin *> enable_pins) { this->enable_pins_ = std::move(enable_pins); }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  void dump_pins_(uint8_t start, uint8_t end, const char *name, uint8_t offset);
  void dump_config() override;
  void draw_pixel_at(int x, int y, Color color) override;

  // this will be horribly slow.
 protected:
  void setup_enables_();
  void common_setup_();
  InternalGPIOPin *de_pin_{nullptr};
  InternalGPIOPin *pclk_pin_{nullptr};
  InternalGPIOPin *hsync_pin_{nullptr};
  InternalGPIOPin *vsync_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  InternalGPIOPin *data_pins_[16] = {};
  uint16_t hsync_pulse_width_ = 10;
  uint16_t hsync_back_porch_ = 10;
  uint16_t hsync_front_porch_ = 20;
  uint16_t vsync_pulse_width_ = 10;
  uint16_t vsync_back_porch_ = 10;
  uint16_t vsync_front_porch_ = 10;
  uint32_t pclk_frequency_ = 16 * 1000 * 1000;
  bool pclk_inverted_{true};
  uint8_t madctl_{};
  const char *model_{"Unknown"};
  bool invert_colors_{};
  display::ColorOrder color_mode_{display::COLOR_ORDER_BGR};
  size_t width_;
  size_t height_;
  uint16_t *buffer_{nullptr};
  std::vector<GPIOPin *> enable_pins_{};
  uint16_t x_low_{1};
  uint16_t y_low_{1};
  uint16_t x_high_{0};
  uint16_t y_high_{0};

  esp_lcd_panel_handle_t handle_{};
};

#ifdef USE_SPI
class MipiRgbSpi : public MipiRgb,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_1MHZ> {
 public:
  MipiRgbSpi(int width, int height) : MipiRgb(width, height) {}

  void set_init_sequence(const std::vector<uint8_t> &init_sequence) { this->init_sequence_ = init_sequence; }
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void setup() override;

 protected:
  void write_command_(uint8_t value);
  void write_data_(uint8_t value);
  void write_init_sequence_();
  void dump_config();

  GPIOPin *dc_pin_{nullptr};
  std::vector<uint8_t> init_sequence_;
};
#endif

}  // namespace mipi_rgb
}  // namespace esphome
#endif
