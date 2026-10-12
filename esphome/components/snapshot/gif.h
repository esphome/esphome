#pragma once

#ifdef USE_HOST
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace esphome::snapshot {

/// Writes an animated GIF a frame at a time, so a long recording is never held in memory.
///
/// A GIF frame can use at most 256 colours. Each frame gets its own colour table, chosen from what
/// is in that frame, so a frame with 256 colours or fewer is stored exactly.
class GifWriter {
 public:
  GifWriter(FILE *file, int width, int height);

  /// Write the start of the file. This must come first. The animation repeats forever.
  bool write_header();
  /// Add a frame from three bytes per pixel in blue, green, red order, topmost row first, with
  /// `row_stride` bytes from the start of one row to the start of the next.
  /// The frame is shown for `delay_centiseconds` hundredths of a second.
  bool write_frame(const uint8_t *bgr, size_t row_stride, unsigned delay_centiseconds);
  /// Write the end of the file. This must come last.
  bool write_trailer();

 protected:
  /// How many pixels of one colour there are in the frame, and the sum of their red, green and blue.
  struct ColorBin {
    uint64_t sum[3];
    uint32_t count;
  };

  /// Choose up to 256 colours for the frame in `bins_`, storing them as red, green, blue triples in
  /// `palette` and which one each colour is nearest in `palette_index_`. Returns how many were chosen.
  size_t build_palette_(uint8_t *palette);

  FILE *file_;
  int width_;
  int height_;
  std::vector<ColorBin> bins_;
  std::vector<uint16_t> used_colors_;
  std::vector<uint8_t> palette_index_;
  std::vector<uint8_t> indices_;
  std::vector<uint8_t> output_;
};

}  // namespace esphome::snapshot

#endif
