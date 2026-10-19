#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::st7305 {

enum Orientation : uint8_t {
  ORIENTATION_LANDSCAPE = 0,
  ORIENTATION_PORTRAIT = 1,
};

/// Sitronix ST7305/ST7306 reflective monochrome LCD, 1 bit per pixel.
///
/// The controller RAM is addressed in 12-column x 2-row blocks (3 bytes per address). With a
/// landscape panel one byte covers 2 pixels in x and 4 in y; with a portrait panel it covers
/// 4 pixels in x and 2 in y.
class ST7305 : public display::DisplayBuffer,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_10MHZ> {
 public:
  ST7305(const char *model, int width, int height, Orientation orientation, uint8_t column_start, uint8_t column_end,
         uint8_t row_start, uint8_t row_end, const uint8_t *init_sequence, size_t init_sequence_length)
      : model_(model),
        width_(width),
        height_(height),
        orientation_(orientation),
        column_start_(column_start),
        column_end_(column_end),
        row_start_(row_start),
        row_end_(row_end),
        buffer_length_(static_cast<size_t>(width) * static_cast<size_t>(height) / 8),
        init_sequence_(init_sequence),
        init_sequence_length_(init_sequence_length) {}

  void setup() override;
  void update() override;
  void dump_config() override;

  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }

  void fill(Color color) override;
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

 protected:
  /// An addressable write window. RASET addresses are the outer index (x pairs in landscape,
  /// y pairs in portrait); CASET addresses are 12-pixel bands of the inner index.
  struct Window {
    uint16_t outer_start;
    uint16_t outer_end;
    uint16_t block_start;
    uint16_t block_end;
    uint16_t stride;
    uint8_t caset_start;
    uint8_t caset_end;

    size_t data_length() const {
      return static_cast<size_t>(this->outer_end - this->outer_start + 1) *
             static_cast<size_t>(this->block_end - this->block_start + 1);
    }
  };

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }

  bool init_buffer_();
  void reset_();
  void send_init_sequence_();
  void write_full_frame_();
  void write_window_(const Window &window);
  Window compute_window_() const;

  void send_command_(uint8_t command) {
    this->dc_pin_->digital_write(false);
    this->write_byte(command);
  }
  void send_data_(const uint8_t *data, size_t length) {
    this->dc_pin_->digital_write(true);
    this->write_array(data, length);
  }

  bool is_dirty_() const { return this->x_high_ >= this->x_low_ && this->y_high_ >= this->y_low_; }
  void clear_dirty_() {
    this->x_low_ = this->width_;
    this->x_high_ = -1;
    this->y_low_ = this->height_;
    this->y_high_ = -1;
  }
  void extend_dirty_(int x0, int y0, int x1, int y1);

  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};

  const char *model_;
  int width_;
  int height_;
  Orientation orientation_;
  uint8_t column_start_;
  uint8_t column_end_;
  uint8_t row_start_;
  uint8_t row_end_;
  size_t buffer_length_;
  const uint8_t *init_sequence_;
  size_t init_sequence_length_;

  int x_low_{0};
  int x_high_{-1};
  int y_low_{0};
  int y_high_{-1};
};

}  // namespace esphome::st7305
