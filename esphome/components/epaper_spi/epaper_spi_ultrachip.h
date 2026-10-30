#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Shared base for controllers with the UltraChip style command set (UC8179, UC8279, SSD2677).
 *
 * It holds the shared commands and the deep-sleep and partial-window handling, which a driver uses
 * as far as its controller needs them; the concrete drivers differ in how each refresh is configured
 * and how the image is sent.
 */
class EPaperUltraChip : public EPaperBase {
 public:
  using EPaperBase::EPaperBase;

  // Nothing carries the image across a restart or the controller's deep sleep, so sleep the panel rather than
  // leaving it waiting in standby
  void on_safe_shutdown() override;

 protected:
  static constexpr uint8_t CMD_POWER_OFF = 0x02;
  static constexpr uint8_t CMD_POWER_ON = 0x04;
  static constexpr uint8_t CMD_DEEP_SLEEP = 0x07;
  static constexpr uint8_t CMD_DTM1 = 0x10;
  static constexpr uint8_t CMD_REFRESH = 0x12;
  static constexpr uint8_t CMD_DTM2 = 0x13;
  static constexpr uint8_t CMD_CDI = 0x50;
  static constexpr uint8_t CMD_PARTIAL_WINDOW = 0x90;
  static constexpr uint8_t CMD_PARTIAL_IN = 0x91;
  static constexpr uint8_t CMD_PARTIAL_OUT = 0x92;
  static constexpr uint8_t CMD_CCSET = 0xE0;
  static constexpr uint8_t CMD_TSSET = 0xE5;
  static constexpr uint8_t DEEP_SLEEP_CHECK = 0xA5;

  void deep_sleep() override;
  // Sends the deep-sleep command unconditionally; deep_sleep() skips it while partial updates are in use
  void send_deep_sleep_();
  // Enters partial mode and sets the refresh window; x is rounded out to whole bytes
  void set_partial_window_(uint16_t x_start, uint16_t x_end, uint16_t y_start, uint16_t y_end);
};

}  // namespace esphome::epaper_spi
