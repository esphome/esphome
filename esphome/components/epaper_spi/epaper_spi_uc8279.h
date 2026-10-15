#pragma once

#include "epaper_spi_ultrachip.h"

namespace esphome::epaper_spi {

/**
 * Monochrome e-paper displays using the UltraChip UC8279 controller, such as the Xteink X4 Pro.
 *
 * Buffer layout: 1 bit per pixel, 1=white, 0=black (the base class default).
 *
 * The controller scans offset_height + height gates; the panel is bonded to the last height of them,
 * so every image plane is written for all gates, white outside the panel.
 *
 * A full refresh writes the new image to DTM2 and white to DTM1. A partial refresh writes only DTM2
 * and drives the pixels that differ from DTM1, which therefore has to hold the image on the panel:
 * while partial updates are in use it is rewritten after every refresh, in time slices like the
 * image transfer. The panel loses both planes in its own deep sleep, so it then waits in standby
 * between updates instead.
 */
class EPaperUC8279 : public EPaperUltraChip {
 public:
  EPaperUC8279(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
               size_t init_sequence_length, uint16_t offset_height)
      : EPaperUltraChip(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY),
        offset_height_(offset_height) {}

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  bool after_refresh() override;

  uint16_t gate_count_() const { return this->offset_height_ + this->height_; }
  // Writes one gate's row of the image, or white for a gate outside the panel or when white is true
  void write_gate_(size_t gate, bool white);
  // Writes one plane in time slices, sending `command` at its first gate; true once all gates are written
  bool write_plane_(uint8_t command, bool white, size_t base);

  uint16_t offset_height_;
};

}  // namespace esphome::epaper_spi
