#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ht16k33 {

enum ChipLinesStyle {
  ZIGZAG = 0,
  SNAKE,
};

enum ScrollMode {
  CONTINUOUS = 0,
  STOP,
};

class HT16K33Component;

using ht16k33_writer_t = std::function<void(HT16K33Component &)>;

class HT16K33Component : public display::DisplayBuffer, public i2c::I2CDevice {
 public:
  static const Color CL;
  static const Color CH;
  static const Color CLH;

  void set_writer(ht16k33_writer_t &&writer) { this->writer_ = writer; }

  void setup() override;

  void loop() override;

  void dump_config() override;

  void update() override;

  float get_setup_priority() const override;

  void display();

  void invert_on_off(bool on_off);
  void invert_on_off();

  bool is_on();
  void turn_on();
  void turn_off();

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_height_internal() override;
  int get_width_internal() override;

  void set_intensity(uint8_t intensity) { this->intensity_ = intensity; };
  void set_num_chips(uint8_t num_chips) { this->num_chips_ = num_chips; };
  void set_num_chip_lines(uint8_t num_chip_lines) { this->num_chip_lines_ = num_chip_lines; };
  void set_chip_lines_style(ChipLinesStyle chip_lines_style) { this->chip_lines_style_ = chip_lines_style; };
  void set_chip_orientation(uint8_t rotate) { this->orientation_ = rotate; };
  void set_scroll_speed(uint16_t speed) { this->scroll_speed_ = speed; };
  void set_scroll_dwell(uint16_t dwell) { this->scroll_dwell_ = dwell; };
  void set_scroll_delay(uint16_t delay) { this->scroll_delay_ = delay; };
  void set_scroll(bool on_off) { this->scroll_ = on_off; };
  void set_scroll_mode(ScrollMode mode) { this->scroll_mode_ = mode; };
  void set_reverse(bool on_off) { this->reverse_ = on_off; };
  void set_flip_x(bool flip_x) { this->flip_x_ = flip_x; };
  void set_blink_rate(uint8_t b) { this->blink_rate_ = b; };

  void send64pixels(uint8_t chip, const uint16_t pixels[8]);

  void scroll_left();
  void scroll(bool on_off, ScrollMode mode, uint16_t speed, uint16_t delay, uint16_t dwell);
  void scroll(bool on_off, ScrollMode mode);
  void scroll(bool on_off);
  void intensity(uint8_t intensity);
  void blink_rate(uint8_t blink_rate);

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

  /*!
    @brief  Clear display.
  */
  void clear(void);

  void fill(Color color);

 protected:
  void init_reset_();
  uint8_t orientation_180_();
  void command_(uint8_t cmmand);
  void command_all_(uint8_t cmmand);
  uint16_t color_to_pixel(Color color);

  uint8_t intensity_{};  // Intensity of the display from 0 to 15 (most)
  uint8_t num_chips_;
  uint8_t num_chip_lines_;
  ChipLinesStyle chip_lines_style_;
  bool scroll_{};
  bool reverse_{};
  bool flip_x_{};
  bool update_{};
  uint16_t scroll_speed_;
  uint16_t scroll_delay_;
  uint16_t scroll_dwell_;
  uint16_t old_buffer_size_{0};
  ScrollMode scroll_mode_;
  bool invert_{};
  uint8_t orientation_;
  Color bckgrnd_{Color::BLACK};
  std::vector<std::vector<uint16_t>> max_displaybuffer_;
  uint32_t last_scroll_{0};
  uint16_t stepsleft_{0};
  optional<ht16k33_writer_t> writer_{};
  uint8_t blink_rate_{0};

  GPIOPin *reset_pin_{nullptr};
  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED } error_code_{NONE};

  // internal states
  uint8_t base_address_{0x70};
  bool is_on_{false};
  bool intensity_changed_{};  // True if we need to re-send the intensity
};

}  // namespace ht16k33
}  // namespace esphome
