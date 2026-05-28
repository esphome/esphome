#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * T133A01-based 6-color e-paper display driver.
 *
 * The T133A01 controller uses a dual-CS SPI architecture:
 *   - CS  (primary):   Controls the first half of pixel data transfer
 *   - CS1 (secondary): Controls panel commands (init, power, refresh) and
 *                      the second half of pixel data transfer
 *
 * Color depth: 4 bits per pixel, supporting 6 colors:
 *   White, Green, Red, Yellow, Blue, Black
 *
 * Buffer layout: 2 pixels per byte (4bpp packed), total buffer size
 * is width * height / 2 bytes.
 */
class EPaperT133A01 : public EPaperBase {
 public:
  EPaperT133A01(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    this->buffer_length_ = (size_t) width * height / 2;  // 2 pixels per byte at 4bpp
  }

  void set_cs1_pin(GPIOPin *pin) { this->cs1_pin_ = pin; }
  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }

  void fill(Color color) override;
  void clear() override;

  void dump_config() override;

 protected:
  void setup_pins() const override;
  bool reset() override;
  bool initialise(bool partial) override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
  void draw_pixel_at(int x, int y, Color color) override;

  bool transfer_data() override;

  /// Send a command+data to the CS1 device
  void cmd_data_cs1_(uint8_t command, const uint8_t *data, size_t length);
  void cmd_data_cs1_(uint8_t command, std::initializer_list<uint8_t> data) {
    this->cmd_data_cs1_(command, data.begin(), data.size());
  }
  void command_cs1_(uint8_t value);

  /// Convert Color to 4-bit T133A01 color index
  static uint8_t color_to_index(Color color);

  /// Apply COLOR_GET remap table to translate sprite indices to hardware values
  static uint8_t remap_color(uint8_t index);

  GPIOPin *cs1_pin_{nullptr};
  GPIOPin *enable_pin_{nullptr};
};

}  // namespace esphome::epaper_spi
