#pragma once

#include "epaper_spi.h"
#include "esphome/components/split_buffer/split_buffer.h"

namespace esphome::epaper_spi {

/**
 * UC8179 4-level-grayscale driver with hybrid differential partial refresh.
 *
 * Tested with: Waveshare 7.5" V2 (800x480) — model waveshare-7.5in-v2-4gray.
 *
 * Buffer layout: 2 bits per pixel (4 px/byte, MSB first), gray levels 0..3
 * where 3 is white. The controller wants two 1-bit planes per refresh
 * (DTM1/0x10 and DTM2/0x13); the per-level plane bits follow Waveshare's
 * EPD_7in5_V2 4-gray sample.
 *
 * Refresh modes, chosen per update:
 * - FULL: true 4-gray refresh via the OTP waveform (0xE0=02 / 0xE5=5F).
 *   Restores grays and clears ghosting; ~2 s of waveform.
 * - PARTIAL (when full_update_every > 1 and a previous frame is stored):
 *   1-bit differential refresh using register LUTs whose WW/KK slots are
 *   no-op waveforms — only pixels whose 1-bit plane changed get driven, so
 *   unchanged pixels INCLUDING GRAYS physically hold their level. DTM1
 *   carries the previous plane, DTM2 the current one. ~0.5 s, no flash.
 * - FAST FLIP (after request_fast_flip()): 1-bit full refresh with the OTP
 *   fast waveform (0xE5=5A, ~300 ms). Ghost-free but monochrome until the
 *   next full; for page changes where latency beats gray fidelity.
 *
 * The previous frame's plane lives host-side (width*height/8 bytes in a
 * SplitBuffer); controller RAM contents are never relied on, so the panel is
 * deep-slept after every update in every mode. If the plane cannot be
 * allocated, partial refresh is permanently disabled and every update is a
 * FULL refresh; the configured full_update_every is left untouched.
 */
class EPaperUC8179G4 : public EPaperBase {
 public:
  EPaperUC8179G4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                 size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_GRAYSCALE) {
    this->row_width_ = (width + 3) / 4;  // 2 bpp instead of the base's 1 bpp
    this->buffer_length_ = static_cast<size_t>(this->row_width_) * height;
  }

  void setup() override;
  void fill(Color color) override;

  /// Make the NEXT update a full 4-gray refresh regardless of cadence. Call
  /// before an update that replaces most of the screen: the partial waveform
  /// is tuned for small deltas and leaves ghosting on a full-screen swap.
  void force_full_refresh() { this->update_count_ = 0; }

  /// Make the NEXT update a fast 1-bit full refresh (~300 ms vs ~2 s).
  /// Clean and ghost-free but monochrome; grays return on the next scheduled
  /// full refresh. Ideal for page flips.
  void request_fast_flip() { this->fast_flip_ = true; }

 protected:
  enum class Mode : uint8_t { FULL, PARTIAL, FAST_FLIP };

  void draw_pixel_at(int x, int y, Color color) override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

  static uint8_t luminance_to_level_(Color color) {
    return static_cast<uint8_t>((color.r + color.g + color.b) / 3) >> 6;
  }

  /// Pack 8 pixels (2 buffer bytes at even buffer index i) into one wire
  /// byte, MSB first, taking each pixel's bit from bit_for_level.
  uint8_t pack_byte_(size_t i, const uint8_t bit_for_level[4]);

  /// 1-bit plane of the previously displayed frame, used as DTM1 ("old
  /// data") by the differential partial refresh.
  split_buffer::SplitBuffer prev_plane_{};
  /// Whether prev_plane_ holds the plane of the frame actually on the panel.
  bool prev_valid_{false};
  bool fast_flip_{false};
  Mode mode_{Mode::FULL};

  static constexpr uint8_t CMD_PANEL_SETTING = 0x00;
  static constexpr uint8_t CMD_POWER_OFF = 0x02;
  static constexpr uint8_t CMD_POWER_ON = 0x04;
  static constexpr uint8_t CMD_DEEP_SLEEP = 0x07;
  static constexpr uint8_t CMD_DTM1 = 0x10;
  static constexpr uint8_t CMD_REFRESH = 0x12;
  static constexpr uint8_t CMD_DTM2 = 0x13;
  static constexpr uint8_t CMD_VCOM_INTERVAL = 0x50;
  static constexpr uint8_t CMD_PARTIAL_WINDOW = 0x90;
  static constexpr uint8_t CMD_PARTIAL_IN = 0x91;
  static constexpr uint8_t CMD_WAVEFORM_CTRL = 0xE5;
};

}  // namespace esphome::epaper_spi
