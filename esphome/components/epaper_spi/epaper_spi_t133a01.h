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

  void set_cs_pins(GPIOPin *cs, GPIOPin *cs1) {
    this->cs_pin_ = cs;
    this->cs1_pin_ = cs1;
  }

  void fill(Color color) override;

  void setup() override;
  void dump_config() override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

  bool transfer_data() override;

  /**
   * Send a command (and optional data) selecting one or both controllers.
   * Both chip-selects are active-low and managed directly by this driver.
   * @param command The command byte to send
   * @param data Optional pointer to data bytes to send after the command
   * @param length Number of data bytes to send after the command
   * @param use_cs  assert CS (left controller) for this transaction
   * @param use_cs1 assert CS1 (right controller) for this transaction
   */
  void write_command_(uint8_t command, const uint8_t *data, size_t length, bool use_cs, bool use_cs1);
  void write_command_(uint8_t command, std::initializer_list<uint8_t> data, bool use_cs, bool use_cs1) {
    this->write_command_(command, data.begin(), data.size(), use_cs, use_cs1);
  }
  void write_command_(uint8_t command, bool use_cs, bool use_cs1) {
    this->write_command_(command, nullptr, 0, use_cs, use_cs1);
  }

  /// Convert Color to 4-bit T133A01 color index
  static uint8_t color_to_index(Color color);

  /// Apply COLOR_GET remap table to translate sprite indices to hardware values
  static uint8_t remap_color(uint8_t index);

  GPIOPin *cs_pin_{nullptr};
  GPIOPin *cs1_pin_{nullptr};
};

}  // namespace esphome::epaper_spi
