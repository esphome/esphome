#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Driver for Pervasive Displays E2271KS0C1 2.7" e-paper display (264x176 pixels).
 * Supports full and fast (partial) update modes with temperature-compensated waveforms.
 */
class EPaperE2271KS0C1 : public EPaperBase {
 public:
  EPaperE2271KS0C1(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                   size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {
    this->buffer_length_ = (size_t) width * height / 8;  // 1bpp, 8 pixels per byte
  }

  void set_temperature_c(float t) { temperature_c_ = t; }

  // Force next update to be a full refresh (useful when changing display mode)
  void force_full_update() { this->update_count_ = 0; }

 protected:
  bool reset() override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override;

  // E2271KS0C1 busy pin is active-low (LOW when busy, HIGH when idle)
  // Override base class which assumes active-high
  bool is_idle_() const override;

 private:
  static constexpr uint8_t ADDR_INPUT_TEMP = 0xE5;
  static constexpr uint8_t ADDR_ACTIVE_TEMP = 0xE0;
  static constexpr uint8_t ADDR_PSR = 0x00;
  static constexpr uint8_t ADDR_FRAME1 = 0x10;
  static constexpr uint8_t ADDR_FRAME2 = 0x13;
  static constexpr uint8_t ADDR_PWR_ON = 0x04;
  static constexpr uint8_t ADDR_REFRESH = 0x12;
  static constexpr uint8_t ADDR_PWR_OFF = 0x02;
  static constexpr uint8_t ADDR_VCOM_CDI = 0x50;

  // Panel settings register values
  uint8_t psr_[2] = {0xCF, 0x8D};

  float temperature_c_{25.0f};
  std::vector<uint8_t> tx_;    //< Rotated buffer for panel transmission
  std::vector<uint8_t> prev_;  //< Previous frame for fast updates
  bool initialized_{false};    //< Track if hardware reset has been done
};

}  // namespace esphome::epaper_spi
