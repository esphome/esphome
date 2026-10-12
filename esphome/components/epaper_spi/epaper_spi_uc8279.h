#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Monochrome e-paper displays using the UltraChip UC8279 controller, such as the Xteink X4 Pro.
 *
 * Buffer layout: 1 bit per pixel, 1=white, 0=black (the base class default).
 *
 * The controller scans gate_offset + height gates; the panel is bonded to the last height of them,
 * so every image plane is written for all gates, white outside the panel.
 *
 * A full refresh writes the new image to DTM2 and white to DTM1. A partial refresh writes only DTM2
 * and drives the pixels that differ from DTM1, which therefore has to hold the image on the panel:
 * while partial updates are in use it is rewritten after every refresh, in time slices like the
 * image transfer. The panel loses both planes in its own deep sleep, so it then waits in standby
 * between updates instead.
 */
class EPaperUC8279 : public EPaperBase {
 public:
  EPaperUC8279(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
               size_t init_sequence_length, uint16_t gate_offset)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY),
        gate_offset_(gate_offset) {
    this->buffer_length_ = this->row_width_ * height;
  }

  void loop() override;
  void on_safe_shutdown() override;

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override;

  uint16_t gate_count_() const { return this->gate_offset_ + this->height_; }
  // Writes one gate's row of the image, or white for a gate outside the panel or when white is true
  void write_gate_(size_t gate, bool white);
  // Continues writing the image to DTM1; true once all gates are written
  bool write_previous_image_();

  uint16_t gate_offset_;
  size_t previous_image_gate_{};
  bool partial_{};                 // the update in progress is a partial one
  bool writing_previous_image_{};  // DTM1 is being rewritten after a refresh
};

}  // namespace esphome::epaper_spi
