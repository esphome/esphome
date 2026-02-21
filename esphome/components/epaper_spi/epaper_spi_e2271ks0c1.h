#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

// Pervasive Displays 2.7" E2271KS0C1 (264x176, fast partial updates)
// https://www.pervasivedisplays.com/product/2-71-e-ink-displays
class EPaperE2271KS0C1 : public EPaperBase {
 public:
  EPaperE2271KS0C1(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                   size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {
    this->buffer_length_ = width * height / 8;  // 1bpp, 8 pixels per byte
  }

  void set_temperature(float t) { this->temperature_ = t; }

 protected:
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override;

  static constexpr uint8_t CMD_PSR = 0x00;
  static constexpr uint8_t CMD_PWR_OFF = 0x02;
  static constexpr uint8_t CMD_PWR_ON = 0x04;
  static constexpr uint8_t CMD_FRAME1 = 0x10;
  static constexpr uint8_t CMD_REFRESH = 0x12;
  static constexpr uint8_t CMD_FRAME2 = 0x13;
  static constexpr uint8_t CMD_VCOM_CDI = 0x50;
  static constexpr uint8_t CMD_ACTIVE_TEMP = 0xE0;
  static constexpr uint8_t CMD_INPUT_TEMP = 0xE5;

  static constexpr uint8_t PSR_DEFAULT[2] = {0xCF, 0x8D};

  // Transfer state: 0=init, 1=frame1, 2=frame2
  uint8_t transfer_phase_{0};
  float temperature_{25.0f};
  std::vector<uint8_t> prev_;
};

}  // namespace esphome::epaper_spi
