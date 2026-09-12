#pragma once
// Shaped as it would be upstream: a generic intermediate base, plus a
// per-panel subclass supplying only the bits the datasheet cannot.
// Mirrors EPaper4bpp, which does the same job for the colour panels.

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Intermediate base for mono e-paper panels that can synthesise four gray
 * levels out of their two RAM planes.
 *
 * Owns buffer sizing, fill()/draw_pixel_at(), the plane split, the chunked
 * SPI transfer and the comparison frame that lets a partial refresh coexist
 * with grayscale. Concrete subclasses supply only their IC-specific refresh
 * trigger, and their plane command bytes if they are not SSD16xx.
 *
 * Two levels per pixel, four pixels per byte, most significant first:
 *   0 black, 1 dark gray, 2 light gray, 3 white
 *
 * FULL push:    both planes carry the split image (high bit -> new RAM,
 *               low bit -> old RAM) and refresh_gray_() runs the panel's
 *               four-level waveform.
 * PARTIAL push: the old RAM gets the frame currently ON THE GLASS and the
 *               new RAM the frame we want; the DU waveform is indexed by
 *               (old bit, new bit) and its 0->0 / 1->1 entries do not
 *               drive, so unchanged pixels - including gray ones - are
 *               left alone and only real changes move.
 *
 * That comparison frame is why this class keeps a shadow: after a
 * four-level push the panel's RAM holds two bitplanes of a gray image, not
 * a previous mono frame, so there is nothing in the controller for a
 * partial to diff against. On a plain mono driver the controller maintains
 * that itself, which is why none of them need this.
 *
 * It costs width*height/8 bytes and is only allocated once a partial is
 * actually asked for, so a display that only ever refreshes fully - or a
 * board too small to spare the memory - pays nothing for it.
 *
 * Which kind of push happens is the base class's existing decision:
 * refresh_screen() is handed partial = (update_count_ != 0), so
 * full_update_every and request_full_refresh() drive it exactly as they do
 * for every other model.
 */
class EPaperGray4 : public EPaperBase {
 public:
  EPaperGray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
              size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, display::DISPLAY_TYPE_GRAYSCALE) {
    this->row_width_ = (width + 3) / 4;  // four pixels per byte
    this->buffer_length_ = (size_t) this->row_width_ * height;
  }

  /// Make the next push a full four-level refresh, whatever the counter
  /// says. Belongs on EPaperBase upstream, so every model gets it.
  void request_full_refresh() { this->update_count_ = 0; }

  /// Put the panel into the deeper sleep, which drops its RAM, after the
  /// next push: for use just before the host itself sleeps. Between updates
  /// the panel sleeps too, but in the mode that keeps RAM - see deep_sleep().
  void sleep_panel_deeply_after_next_push() { this->sleep_panel_deep_ = true; }

  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  bool reset() override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void deep_sleep() override;
  void power_on() override {}
  void power_off() override {}

  /// Run the panel's four-level refresh. Both planes hold the split image.
  /// The one thing a model must supply: it is vendor waveform selection,
  /// not a datasheet register.
  virtual void refresh_gray_() = 0;

  /// Run a partial refresh. Old RAM holds the glass, new RAM the target.
  /// Defaults to the SSD16xx OTP display mode 2.
  virtual void refresh_partial_();

  /// RAM plane select. SSD16xx keeps the new frame in 0x24 and the
  /// comparison in 0x26; other families differ (UC8179 uses 0x13 / 0x10).
  virtual uint8_t plane_command_(bool new_frame) const { return new_frame ? 0x24 : 0x26; }

  /// Whether the four-level planes go out inverted. True for panels whose
  /// factory waveform reads 1 as white.
  virtual bool gray_planes_inverted_() const { return true; }

  void set_window_();
  /// True when the shadow already holds the frame on the glass, so a partial
  /// has something to compare against. Allocates it on first use and returns
  /// false that once: the push that allocates is the one that seeds it.
  bool shadow_ready_();
  uint8_t color_to_level_(Color color) const;
  uint8_t level_at_(int x, int y) const;

  uint8_t plane_{0};          // first or second pass of a push
  bool partial_push_{false};  // latched for the whole push
  bool sleep_panel_deep_{false};
  bool shadow_failed_{false};
  split_buffer::SplitBuffer shadow_{};  // 1bpp frame on the glass, lazily allocated
};

/**
 * Seeed reTerminal Sticky, 3.97" 800x480 at 235 ppi.
 *
 * Its four levels come from the panel's own OTP waveform, which is what the
 * vendor firmware uses. It is NOT the generic SSD1677 recipe of uploading a
 * 105-byte LUT with 0x32: that table is the opposite polarity to the OTP
 * path, and the vendor firmware never sends it.
 */
class EPaperStickyGray4 : public EPaperGray4 {
 public:
  EPaperStickyGray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                    size_t init_sequence_length)
      : EPaperGray4(name, width, height, init_sequence, init_sequence_length) {}

 protected:
  void refresh_gray_() override;
};

}  // namespace esphome::epaper_spi
