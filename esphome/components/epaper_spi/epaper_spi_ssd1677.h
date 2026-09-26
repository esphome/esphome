#pragma once

#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {

/**
 * Monochrome SSD1677 with partial refreshes that leave unchanged pixels alone.
 *
 * A partial refresh drives each pixel from the pair (RAM 0x26 = the image on the panel,
 * RAM 0x24 = the new image) across the whole panel; the RAM window only scopes a write.
 * EPaperMono writes 0x26 once and afterwards only the changed window of 0x24, which relies on
 * the controller's RAM being unchanged from one update to the next. On this controller it is not:
 * the hardware reset at the start of each update loses it, and even without resets, keeping 0x26
 * in step one window at a time left unchanged areas alternating between older frames. Either way
 * unchanged pixels get driven on every partial and wash out.
 *
 * So before every refresh this class writes both planes over the whole panel: 0x26 from a copy of
 * the frame last sent, 0x24 from the buffer. GxEPD2 likewise rewrites both planes after each
 * partial on this controller. The copy is taken as the data goes out, not from the buffer, which
 * may already hold the next frame by the time the refresh completes.
 */
class EPaperSSD1677 : public EPaperMono {
 public:
  EPaperSSD1677(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length) {}

  void setup() override;

 protected:
  bool reset() override;
  bool transfer_data() override;

  split_buffer::SplitBuffer sent_{};  // the frame last sent to 0x24, i.e. what the panel shows
  uint8_t plane_{0};                  // 0 while sending 0x26, 1 while sending 0x24
};

}  // namespace esphome::epaper_spi
