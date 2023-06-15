#include "font.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace display {

static const char *const TAG = "display";

bool Glyph::get_pixel(int x, int y) const {
  const int x_data = x - this->glyph_data_->offset_x;
  const int y_data = y - this->glyph_data_->offset_y;
  if (x_data < 0 || x_data >= this->glyph_data_->width || y_data < 0 || y_data >= this->glyph_data_->height)
    return false;
  const uint32_t width_8 = ((this->glyph_data_->width + 7u) / 8u) * 8u;
  const uint32_t pos = x_data + y_data * width_8;
  return progmem_read_byte(this->glyph_data_->data + (pos / 8u)) & (0x80 >> (pos % 8u));
}
void Glyph::draw(int x_at, int y_start, DisplayBuffer *display, Color color) const {
  int scan_x1, scan_y1, scan_width, scan_height;
  this->scan_area(&scan_x1, &scan_y1, &scan_width, &scan_height);

  const int glyph_x_max = scan_x1 + scan_width;
  const int glyph_y_max = scan_y1 + scan_height;
  for (int glyph_x = scan_x1; glyph_x < glyph_x_max; glyph_x++) {
    for (int glyph_y = scan_y1; glyph_y < glyph_y_max; glyph_y++) {
      if (this->get_pixel(glyph_x, glyph_y)) {
        display->draw_pixel_at(glyph_x + x_at, glyph_y + y_start, color);
      }
    }
  }
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

Font::Font(const GlyphData *data, int data_nr, int baseline, int height) : baseline_(baseline), height_(height) {
  glyphs_.reserve(data_nr);
  for (int i = 0; i < data_nr; ++i)
    glyphs_.emplace_back(&data[i]);
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
void Font::get_text_bounds(int x, int y, const char *text, TextAlign align, int *x1, int *y1, int *width, int *height) {
  int x_offset, baseline;
  this->measure(text, width, &x_offset, &baseline, height);

  auto x_align = TextAlign(int(align) & 0x18);
  auto y_align = TextAlign(int(align) & 0x07);

  switch (x_align) {
    case TextAlign::RIGHT:
      *x1 = x - *width;
      break;
    case TextAlign::CENTER_HORIZONTAL:
      *x1 = x - (*width) / 2;
      break;
    case TextAlign::LEFT:
    default:
      // LEFT
      *x1 = x;
      break;
  }

  switch (y_align) {
    case TextAlign::BOTTOM:
      *y1 = y - *height;
      break;
    case TextAlign::BASELINE:
      *y1 = y - baseline;
      break;
    case TextAlign::CENTER_VERTICAL:
      *y1 = y - (*height) / 2;
      break;
    case TextAlign::TOP:
    default:
      *y1 = y;
      break;
  }
}
void Font::print(int x_start, int y_start, DisplayBuffer *display, Color color, const char *text) {
  int i = 0;
  int x_at = x_start;
  while (text[i] != '\0') {
    int match_length;
    int glyph_n = this->match_next_glyph(text + i, &match_length);
    if (glyph_n < 0) {
      // Unknown char, skip
      ESP_LOGW(TAG, "Encountered character without representation in font: '%c'", text[i]);
      if (!this->get_glyphs().empty()) {
        uint8_t glyph_width = this->get_glyphs()[0].glyph_data_->width;
        display->filled_rectangle(x_at, y_start, glyph_width, this->height_, color);
        x_at += glyph_width;
      }

      i++;
      continue;
    }

    const Glyph &glyph = this->get_glyphs()[glyph_n];
    glyph.draw(x_at, y_start, display, color);
    x_at += glyph.glyph_data_->width + glyph.glyph_data_->offset_x;

    i += match_length;
  }
}

}  // namespace display
}  // namespace esphome
