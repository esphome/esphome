#pragma once

#include "epaper_spi_spectra_e6.h"

namespace esphome::epaper_spi {

class EPaper7p3InSpectraE6 : public EPaperSpectraE6 {
  static constexpr const uint16_t WIDTH = 800;
  static constexpr const uint16_t HEIGHT = 480;
  // clang-format off

  // Command, data length, data
  static constexpr uint8_t INIT_SEQUENCE[] = {
    0xAA, 6, 0x49, 0x55, 0x20, 0x08, 0x09, 0x18,
    0x01, 1, 0x3F,
    0x00, 2, 0x5F, 0x69,
    0x03, 4, 0x00, 0x54, 0x00, 0x44,
    0x05, 4, 0x40, 0x1F, 0x1F, 0x2C,
    0x06, 4, 0x6F, 0x1F, 0x17, 0x49,
    0x08, 4, 0x6F, 0x1F, 0x1F, 0x22,
    0x30, 1, 0x03,
    0x50, 1, 0x3F,
    0x60, 2, 0x02, 0x00,
    0x61, 4, WIDTH / 256, WIDTH % 256, HEIGHT / 256, HEIGHT % 256,
    0x84, 1, 0x01,
    0xE3, 1, 0x2F,
  };
  // clang-format on

 public:
  EPaper7p3InSpectraE6() : EPaperSpectraE6(INIT_SEQUENCE, sizeof(INIT_SEQUENCE)) {}

  void dump_config() override;

 protected:
  int get_width_internal() override { return WIDTH; };
  int get_height_internal() override { return HEIGHT; };

  void refresh_screen() override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
};

}  // namespace esphome::epaper_spi
