#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Waveshare 3.97" / SSD1677 driver
 *
 * Supports:
 * - 1bpp black/white
 * - full-frame transfer
 * - full refresh
 *
 * Not supported yet:
 * - partial refresh
 * - fast refresh
 * - 4-gray mode
 */
class EPaperSSD1677 final : public EPaperBase {
 public:
  EPaperSSD1677(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {
    this->buffer_length_ = this->row_width_ * height;
  }

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override {}
  void power_off() override {}
  void deep_sleep() override;

  enum class FSMState : uint8_t {
    NONE = 0,
    RESET_STEP0_H,
    RESET_STEP1_L,
    RESET_STEP2_IDLECHECK,
    INIT_STEP0_SWRESET,
    INIT_STEP1_REGULARINIT,
  };

  FSMState step_{FSMState::NONE};

  static constexpr uint16_t RESET_HIGH_MS_0 = 20;
  static constexpr uint16_t RESET_LOW_MS = 2;
  static constexpr uint16_t RESET_HIGH_MS_1 = 20;
};

}  // namespace esphome::epaper_spi
