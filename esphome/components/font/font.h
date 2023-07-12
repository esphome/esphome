#pragma once

#include "esphome/core/datatypes.h"
#include "esphome/core/color.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace font {

class Font;

struct GlyphData {
  const char *a_char;
  const uint8_t *data;
  int offset_x;
  int offset_y;
  int width;
  int height;
};

class Glyph {
 public:
  Glyph(const GlyphData *data) : glyph_data_(data) {}

  void draw(int x, int y, display::Display *display, Color color) const;

  const char *get_char() const;

  bool compare_to(const char *str) const;

  int match_length(const char *str) const;

  void scan_area(int *x1, int *y1, int *width, int *height) const;

 protected:
  friend Font;

  const GlyphData *glyph_data_;
};

class Font : public display::BaseFont {
 public:
  /** Construct the font with the given glyphs.
   *
   * @param glyphs A vector of glyphs, must be sorted lexicographically.
   * @param baseline The y-offset from the top of the text to the baseline.
   * @param bottom The y-offset from the top of the text to the bottom (i.e. height).
   */
  Font(const GlyphData *data, int data_nr, int baseline, int height);

  int match_next_glyph(const char *str, int *match_length);

  void print(int x_start, int y_start, display::Display *display, Color color, const char *text) override;
  void measure(const char *str, int *width, int *x_offset, int *baseline, int *height) override;
  inline int get_baseline() { return this->baseline_; }
  inline int get_height() { return this->height_; }

  const std::vector<Glyph, ExternalRAMAllocator<Glyph>> &get_glyphs() const { return glyphs_; }

 protected:
  std::vector<Glyph, ExternalRAMAllocator<Glyph>> glyphs_;
  int baseline_;
  int height_;
};

}  // namespace font
}  // namespace esphome
