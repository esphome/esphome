#pragma once

#include "epaper_spi_ultrachip.h"

namespace esphome::epaper_spi {

/**
 * Monochrome e-paper displays using the UltraChip UC8151 controller, such as the Good Display GDEW029T5.
 *
 * Buffer layout: 1 bit per pixel, 1=white, 0=black (the base class default), sent to the panel unchanged.
 *
 * A full refresh uses the waveform stored in the panel's OTP, set up by the model's init sequence, and writes
 * only the new image to DTM2.
 *
 * A partial refresh loads a partial waveform into the LUT registers, enters partial mode with a full-screen window
 * and writes the new image to DTM2; the controller drives the pixels that differ from DTM1. DTM1 therefore has to
 * hold the image on the panel: while partial updates are in use it is rewritten after every refresh, in time slices
 * like the image transfer, and the panel waits in standby between updates instead of its deep sleep, which would
 * lose it. A partial update skips the hardware reset for the same reason.
 */
class EPaperUC8151 : public EPaperUltraChip {
 public:
  EPaperUC8151(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
               size_t init_sequence_length)
      : EPaperUltraChip(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {}

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  bool after_refresh() override;
  void power_off() override;
  // The UC8151 window has one byte for each horizontal bound
  void set_partial_window_(uint16_t x_start, uint16_t x_end, uint16_t y_start, uint16_t y_end) override;

  // Loads the partial-refresh waveform and the settings it needs into the controller registers
  void send_partial_settings_();
  // Writes the buffer to one plane in time slices, sending `command` first; true once the whole plane is written
  bool write_plane_(uint8_t command);
};

}  // namespace esphome::epaper_spi
