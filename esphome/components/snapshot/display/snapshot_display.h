#pragma once

#ifdef USE_HOST
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/snapshot/snapshot.h"
#include "esphome/core/component.h"

namespace esphome::snapshot {

/// A display with nowhere to show anything: it keeps the picture in memory, where the snapshot
/// action can pick it up. That makes it a way to see what a configuration draws on a machine with
/// no screen, and to check the result in a test.
class SnapshotDisplay final : public display::DisplayBuffer, public Snapshot {
 public:
  void setup() override;
  void update() override { this->do_update_(); }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  display::DisplayType get_display_type() override { return display::DISPLAY_TYPE_COLOR; }

  void set_dimensions(uint16_t width, uint16_t height) {
    this->width_ = width;
    this->height_ = height;
  }

  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }

  int snapshot_width() override { return this->width_; }
  int snapshot_height() override { return this->height_; }
  bool capture_bgr(uint8_t *dest, size_t row_stride) override;

  /// The picture, one 16 bit RGB565 value per pixel, topmost row first. Owned by DisplayBuffer as
  /// a byte pointer; this is the same memory seen as what is actually stored in it.
  uint16_t *pixels_() { return reinterpret_cast<uint16_t *>(this->buffer_); }

  int width_{};
  int height_{};
};

}  // namespace esphome::snapshot

#endif
