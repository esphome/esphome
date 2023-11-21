#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"

#include <vector>

namespace esphome {
namespace max7219digit {

enum ChipLinesStyle {
  ZIGZAG = 0,
  SNAKE,
};

enum ScrollMode {
  CONTINUOUS = 0,
  STOP,
};

class MAX7219Component;

using max7219_writer_t = std::function<void(MAX7219Component &)>;

class MAX7219Component : public PollingComponent,
                         public display::DisplayBuffer,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void set_writer(max7219_writer_t &&writer) { this->writer_local_ = writer; };

  void setup() override;

  void loop() override;

  void dump_config() override;

  void update() override;

  float get_setup_priority() const override;

  void display();

  void invert_on_off(bool on_off);
  void invert_on_off();

  void turn_on_off(bool on_off);

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

  void send_char(uint8_t chip, uint8_t data);
  void send64pixels(uint8_t chip, const uint8_t pixels[8]);

  void scroll_left();
  void scroll(bool on_off, ScrollMode mode, uint16_t speed, uint16_t delay, uint16_t dwell);
  void scroll(bool on_off, ScrollMode mode);
  void scroll(bool on_off);
  void intensity(uint8_t intensity);

  /// Evaluate the printf-format and print the result at the given position.
  uint8_t printdigitf(uint8_t pos, const char *format, ...) __attribute__((format(printf, 3, 4)));
  /// Evaluate the printf-format and print the result at position 0.
  uint8_t printdigitf(const char *format, ...) __attribute__((format(printf, 2, 3)));

  /// Print `str` at the given position.
  uint8_t printdigit(uint8_t pos, const char *str);
  /// Print `str` at position 0.
  uint8_t printdigit(const char *str);

  /// Evaluate the strftime-format and print the result at the given position.
  uint8_t strftimedigit(uint8_t pos, const char *format, ESPTime time) __attribute__((format(strftime, 3, 0)));

  /// Evaluate the strftime-format and print the result at position 0.
  uint8_t strftimedigit(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

 protected:
  void send_byte_(uint8_t a_register, uint8_t data);
  void send_to_all_(uint8_t a_register, uint8_t data);
  uint8_t orientation_180_();

  uint8_t intensity_;  /// Intensity of the display from 0 to 15 (most)
  uint8_t num_chips_;
  uint8_t num_chip_lines_;
  ChipLinesStyle chip_lines_style_;
  bool scroll_;
  bool reverse_;
  bool flip_x_;
  bool update_{false};
  uint16_t scroll_speed_;
  uint16_t scroll_delay_;
  uint16_t scroll_dwell_;
  uint16_t old_buffer_size_ = 0;
  ScrollMode scroll_mode_;
  bool invert_ = false;
  uint8_t orientation_;
  uint8_t bckgrnd_ = 0x0;
  std::vector<std::vector<uint8_t>> max_displaybuffer_;
  uint32_t last_scroll_ = 0;
  uint16_t stepsleft_;
  size_t get_buffer_length_();
  optional<max7219_writer_t> writer_local_{};
};

}  // namespace max7219digit
}  // namespace esphome
