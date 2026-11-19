#pragma once

#include "colorconv.h"
#include "epaper_spi_uc8179.h"

namespace esphome::epaper_spi {

/**
 * Four grey levels on UC8179 panels, with waveforms supplied by the driver.
 *
 * The controller has no notion of grey. Each pixel's bits in its two RAM planes (DTM1 and DTM2)
 * choose one of four waveforms, which in black-and-white use mean "from this colour to that one".
 * Here the four drive to white, light grey, dark grey and black instead.
 *
 * Panels sold from late 2024 hold such a set in their OTP; earlier ones do not. So that both work,
 * the panel is set to take its waveforms from registers, and they are sent before every refresh.
 * The tables follow those of Waveshare's EPD_7in5_V2_old example, with the same pushes but one
 * round of clearing where it has two, and without its idle frames. Every table drives the pixel
 * to white and then to black, which leaves it black whatever it showed before, and then pushes it
 * towards white: all the way, part of it for a grey, or not at all. The opening push is as long as
 * the closing one and opposite, so each table applies as much positive as negative voltage.
 *
 * The voltages and frame rate also come from registers in this mode; they are those of the same
 * example. As with the fast and partial modes, which are set by a forced temperature, no
 * temperature adjustment is applied.
 *
 * The framebuffer packs 2 bits per pixel (4 per byte, most significant pixel first), 0 = black
 * and 3 = white.
 *
 * Every update is a full refresh: with both planes used for the level, nothing tells the
 * controller what a pixel shows now.
 */
class EPaperUC8179Gray4 : public EPaperUC8179 {
 public:
  EPaperUC8179Gray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                    size_t init_sequence_length)
      : EPaperUC8179(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_GRAYSCALE, 2) {}

  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  bool transfer_data() override;
  void send_waveforms_();
  void send_waveform_(uint8_t command, const uint8_t *waveform);
};

}  // namespace esphome::epaper_spi
