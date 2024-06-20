#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/display/display_buffer.h"
#include "st7567_defines.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7567_base {

// easy to implement in future: ST7522_96x16, ST7565_128x64, ST7591_160x132
enum class ST7567Model {
  ST7567_128x64 = 0,  // standard version
  ST7570_128x128,     // standard version
  ST7570_102x102,     // implementation with 102x102 pixels, 128x128 memory
};

class ST7567 : public display::DisplayBuffer {
 public:
  //
  // Standard EspHome functions
  //
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void update() override;
  void dump_config() override;

  //
  // Standard DisplayBuffer functions
  //
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }
  void fill(Color color) override;

  //
  // Setters for device configuration
  // Not doing real device communication, just setting internal variables.
  //
  void set_model(ST7567Model model) { this->model_ = model; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_mirror_x(bool mirror_x) { this->mirror_x_ = mirror_x; }
  void set_mirror_y(bool mirror_y) { this->mirror_y_ = mirror_y; }
  void set_invert_colors(bool invert_colors) { this->invert_colors_ = invert_colors; }
  void set_contrast(uint8_t val) { this->contrast_ = val & ST7567_CONTRAST_MASK; }
  void set_brightness(uint8_t val) { this->brightness_ = val & ST7567_BRIGHTNESS_MASK; }

  //
  // Display interaction
  //
  void request_refresh();  // It is recommended to use the refresh sequence regularly in a specified interval
  void turn_on();
  void turn_off();
  void change_contrast(uint8_t val);       // 0..63, 27-30 normal
  void change_brightness(uint8_t val);     // 0..7, 5 normal
  void invert_colors(bool invert_colors);  // inversion of screen colors

  // test commands
  void enable_all_pixels_on(bool enable);  // turn on all pixels, this doesn't affect RAM
  void set_scroll(uint8_t line);           // set display start line: for screen scrolling w/o affecting RAM

 protected:
 public:
  virtual void command_(uint8_t value) = 0;
  virtual void write_display_data_() = 0;

  void setup_reset_pin_();
  void init_model_();
  void setup_lcd_();

  void reset_lcd_hw_();
  void reset_lcd_sw_();
  void perform_lcd_init_();
  void perform_display_refresh_();

  size_t get_framebuffer_size_();

  int get_height_internal() override;
  int get_width_internal() override;

  uint8_t get_visible_area_offset_x_();
  uint8_t get_visible_area_offset_y_();

  void command_set_start_line_();

  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  std::string model_str_();

  ST7567Model model_{ST7567Model::ST7567_128x64};
  GPIOPin *reset_pin_{nullptr};
  bool mirror_x_{true};
  bool mirror_y_{true};
  bool invert_colors_{false};
  bool all_pixels_on_{false};
  uint8_t contrast_{27};
  uint8_t brightness_{5};

  uint8_t start_line_{0};
  bool refresh_requested_{false};

  struct {
    const char *name;
    uint8_t memory_width{132};
    uint8_t memory_height{64};

    uint8_t visible_width{128};
    uint8_t visible_height{64};

    uint8_t visible_offset_x_normal{0};
    uint8_t visible_offset_x_mirror{0};

    uint8_t visible_offset_y_normal{0};
    uint8_t visible_offset_y_mirror{0};

    std::function<void()> init_procedure{nullptr};
    std::function<void(uint8_t)> command_set_start_line{nullptr};

    uint8_t wait_after_reset{5};
    uint8_t wait_between_power{10};

  } device_config_;

 public:
  // test functions
  void rotate_me() {
    switch (this->rotation_) {
      case esphome::display::DISPLAY_ROTATION_0_DEGREES:
        this->rotation_ = esphome::display::DISPLAY_ROTATION_90_DEGREES;
        break;
      case esphome::display::DISPLAY_ROTATION_90_DEGREES:
        this->rotation_ = esphome::display::DISPLAY_ROTATION_180_DEGREES;
        break;
      case esphome::display::DISPLAY_ROTATION_180_DEGREES:
        this->rotation_ = esphome::display::DISPLAY_ROTATION_270_DEGREES;
        break;
      case esphome::display::DISPLAY_ROTATION_270_DEGREES:
        this->rotation_ = esphome::display::DISPLAY_ROTATION_0_DEGREES;
        break;
    }
    ESP_LOGD("st7567", "Rotation: %d", (int) this->rotation_);
  };
};

}  // namespace st7567_base
}  // namespace esphome
