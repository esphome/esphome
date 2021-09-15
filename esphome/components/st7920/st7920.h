#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace st7920 {

class ST7920;

using st7920_writer_t = std::function<void(ST7920 &)>;

class ST7920 : public PollingComponent,
               public display::DisplayBuffer,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                     spi::DATA_RATE_1MHZ> {
 public:
  void set_writer(st7920_writer_t &&writer) { this->writer_local_ = writer; }
  void set_height(uint16_t height) { this->height_ = height; }
  void set_width(uint16_t width) { this->width_ = width; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void fill(Color color) override;
  void write_display_data();

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();
  void display_init_();
  void command_(uint8_t value);
  void data_(uint8_t value);
  void send_(uint8_t type, uint8_t value);
  void goto_xy_(uint16_t x, uint16_t y);
  void start_transaction_();
  void end_transaction_();

  int16_t width_ = 128, height_ = 64;
  optional<st7920_writer_t> writer_local_{};
};

}  // namespace st7920
}  // namespace esphome
