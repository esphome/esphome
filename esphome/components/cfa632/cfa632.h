#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace cfa632 {

enum CursorType { HIDDEN = 4, UNDERLINE = 5, BLOCK = 6, INVERTING_BLOCK = 7 };

class CFA632 : public Print,
               public PollingComponent,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_1KHZ> {
 public:
  CFA632(float brightness, float contrast, bool scroll_enabled, bool wrap_enabled, CursorType cursor_type);
  void setup() override;
  float get_setup_priority() const override;
  void update() override;
  void dump_config() override;

  void set_writer(std::function<void(CFA632 &)> &&writer) { this->writer_ = std::move(writer); }
  void set_cursor_position(uint8_t row, uint8_t col);
  void set_scroll_enabled(bool enabled);
  void set_wrap_enabled(bool enabled);
  void set_backlight_brightness(float percent);
  void set_contrast(float percent);
  void set_cursor_type(CursorType);

  using Print::write;
  virtual size_t write(uint8_t data) override;
  virtual size_t write(const uint8_t *buffer, size_t size) override;

 protected:
  std::function<void(CFA632 &)> writer_;
  bool scroll_enabled_;
  bool wrap_enabled_;
  float brightness_;
  float contrast_;
  CursorType cursor_type_;

  void init();
  void call_writer() { this->writer_(*this); }
  void clear();
};

}  // namespace cfa632
}  // namespace esphome
