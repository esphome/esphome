#include "font.h"

#include "esphome/core/hal.h"

namespace esphome {
namespace display {

bool Glyph::get_pixel(int x, int y) const {
  const int x_data = x - this->glyph_data_->offset_x;
  const int y_data = y - this->glyph_data_->offset_y;
  if (x_data < 0 || x_data >= this->glyph_data_->width || y_data < 0 || y_data >= this->glyph_data_->height)
    return false;
  const uint32_t width_8 = ((this->glyph_data_->width + 7u) / 8u) * 8u;
  const uint32_t pos = x_data + y_data * width_8;
  return progmem_read_byte(this->glyph_data_->data + (pos / 8u)) & (0x80 >> (pos % 8u));
}
const char *Glyph::get_char() const { return this->glyph_data_->a_char; }
bool Glyph::compare_to(const char *str) const {
  // 1 -> this->char_
  // 2 -> str
  for (uint32_t i = 0;; i++) {
    if (this->glyph_data_->a_char[i] == '\0')
      return true;
    if (str[i] == '\0')
      return false;
    if (this->glyph_data_->a_char[i] > str[i])
      return false;
    if (this->glyph_data_->a_char[i] < str[i])
      return true;
  }
  // this should not happen
  return false;
}
int Glyph::match_length(const char *str) const {
  for (uint32_t i = 0;; i++) {
    if (this->glyph_data_->a_char[i] == '\0')
      return i;
    if (str[i] != this->glyph_data_->a_char[i])
      return 0;
  }
  // this should not happen
  return 0;
}
void Glyph::scan_area(int *x1, int *y1, int *width, int *height) const {
  *x1 = this->glyph_data_->offset_x;
  *y1 = this->glyph_data_->offset_y;
  *width = this->glyph_data_->width;
  *height = this->glyph_data_->height;
}
int Font::match_next_glyph(const char *str, int *match_length) {
  int lo = 0;
  int hi = this->glyphs_.size() - 1;
  while (lo != hi) {
    int mid = (lo + hi + 1) / 2;
    if (this->glyphs_[mid].compare_to(str)) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  *match_length = this->glyphs_[lo].match_length(str);
  if (*match_length <= 0)
    return -1;
  return lo;
}
void Font::measure(const char *str, int *width, int *x_offset, int *baseline, int *height) {
  *baseline = this->baseline_;
  *height = this->height_;
  int i = 0;
  int min_x = 0;
  bool has_char = false;
  int x = 0;
  while (str[i] != '\0') {
    int match_length;
    int glyph_n = this->match_next_glyph(str + i, &match_length);
    if (glyph_n < 0) {
      // Unknown char, skip
      if (!this->get_glyphs().empty())
        x += this->get_glyphs()[0].glyph_data_->width;
      i++;
      continue;
    }

    const Glyph &glyph = this->glyphs_[glyph_n];
    if (!has_char) {
      min_x = glyph.glyph_data_->offset_x;
    } else {
      min_x = std::min(min_x, x + glyph.glyph_data_->offset_x);
    }
    x += glyph.glyph_data_->width + glyph.glyph_data_->offset_x;

    i += match_length;
    has_char = true;
  }
  *x_offset = min_x;
  *width = x - min_x;
}
Font::Font(const GlyphData *data, int data_nr, int baseline, int height) : baseline_(baseline), height_(height) {
  glyphs_.reserve(data_nr);
  for (int i = 0; i < data_nr; ++i)
    glyphs_.emplace_back(&data[i]);
}

}  // namespace display
}  // namespace esphome
