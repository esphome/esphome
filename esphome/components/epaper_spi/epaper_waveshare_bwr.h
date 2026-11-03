#pragma once

#include "epaper_spi_bwr.h"

namespace esphome::epaper_spi {

/**
 * 3-color e-paper displays using the UC8179 and UC8151D controllers.
 * Supports: Waveshare 7.5" V2 BWR (EPD_7in5b_V2), 800x480 pixels; Waveshare 2.13" B V3 (EPD_2in13b_V3),
 * 104x212 pixels; XSRUPB DEPG0750RWF86BF30 7.5" BWR, 800x480 pixels.
 *
 * Buffer layout and data transfer: see EPaperBWR.
 * The red plane is sent as is (1=red) unless invert_red is set; on the UC8151D this relies on the 0x50 value in
 * uc8151.py, and the XSRUPB panel's init sequence sets DDX=11, which needs invert_red.
 *
 * The init sequence (INITIALISE state) sends the power and panel configuration.
 * Power-on (0x04) is sent in the POWER_ON state after data transfer;
 * the state machine then busy-waits before triggering REFRESH_SCREEN (0x12).
 */
class EPaperWaveshareBWR : public EPaperBWR {
 public:
  EPaperWaveshareBWR(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                     size_t init_sequence_length, bool invert_red = false)
      : EPaperBWR(name, width, height, init_sequence, init_sequence_length, invert_red) {}

 protected:
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
};

}  // namespace esphome::epaper_spi
