#pragma once

#include "esphome/components/display/display.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/split_buffer/split_buffer.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::pixoo {

// The Pixoo's main board (where ESPHome runs) talks to a separate LED-driver board (a GD32/AT32
// MCU) over SPI using Divoom's packet protocol:
//   0xAA, len_lo, len_hi, cmd, <data...>, 0xBB
// The image is sent as a DATA (0x00) packet carrying width*height*3 bytes of RGB888; brightness is
// a separate LIGHT (0x01) command; the LED current is set once via SET_RGB_IOUT (0x22). Command
// packets are padded out to the LED board's 240-byte DMA chunk with an UNUSED (0x21) packet.
// The model selects the (square) panel side length.
enum PixooModel : uint8_t {
  PIXOO_64 = 64,
};

class Pixoo : public display::Display,
              public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                    spi::DATA_RATE_8MHZ> {
 public:
  explicit Pixoo(PixooModel model) : model_(model) {}

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  // Brightness is controlled exclusively via the light platform: send a LIGHT command to the LED
  // board (brightness 0..1 -> 0..100%).
  void set_panel_brightness(float brightness);

  display::DisplayType get_display_type() override { return display::DISPLAY_TYPE_COLOR; }

  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;

 protected:
  int get_width_internal() override { return static_cast<int>(this->model_); }
  int get_height_internal() override { return static_cast<int>(this->model_); }

  void set_pixel_(uint32_t index, Color color);
  void send_command_(uint8_t cmd, const uint8_t *data, uint16_t len);

  // Size of the LED board's SPI DMA chunk; the command scratch buffer is one chunk.
  static constexpr size_t DMA_CHUNK = 240;

  PixooModel model_;

  size_t data_size_{0};   // RGB888 image bytes: model^2 * 3
  size_t frame_size_{0};  // full SPI frame: DATA packet + trailing UNUSED packet

  split_buffer::SplitBuffer buffer_{};
  uint8_t *frame_buffer_{nullptr};
  uint8_t cmd_buffer_[DMA_CHUNK]{};
};

}  // namespace esphome::pixoo
