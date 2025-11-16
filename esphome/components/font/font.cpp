#include "font.h"

#include "esphome/core/color.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace font {

static const char *const TAG = "font";

#ifdef USE_LVGL_FONT
const uint8_t *Font::get_glyph_bitmap(const lv_font_t *font, uint32_t unicode_letter) {
  auto *fe = (Font *) font->dsc;
  const auto *gd = fe->get_glyph_data_(unicode_letter);
  if (gd == nullptr) {
    return nullptr;
  }
  return gd->data;
}

bool Font::get_glyph_dsc_cb(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t unicode_letter, uint32_t next) {
  auto *fe = (Font *) font->dsc;
  const auto *gd = fe->get_glyph_data_(unicode_letter);
  if (gd == nullptr) {
    return false;
  }
  dsc->adv_w = gd->advance;
  dsc->ofs_x = gd->offset_x;
  dsc->ofs_y = fe->height_ - gd->height - gd->offset_y - fe->lv_font_.base_line;
  dsc->box_w = gd->width;
  dsc->box_h = gd->height;
  dsc->is_placeholder = 0;
  dsc->bpp = fe->get_bpp();
  return true;
}
const Glyph *Font::get_glyph_data_(uint32_t unicode_letter) {
  if (unicode_letter == this->last_letter_)
    return this->last_data_;
  uint8_t unicode[5]{};
  if (unicode_letter > 0xFFFF) {
    unicode[0] = 0xF0 + ((unicode_letter >> 18) & 0x7);
    unicode[1] = 0x80 + ((unicode_letter >> 12) & 0x3F);
    unicode[2] = 0x80 + ((unicode_letter >> 6) & 0x3F);
    unicode[3] = 0x80 + (unicode_letter & 0x3F);
  } else if (unicode_letter > 0x7FF) {
    unicode[0] = 0xE0 + ((unicode_letter >> 12) & 0xF);
    unicode[1] = 0x80 + ((unicode_letter >> 6) & 0x3F);
    unicode[2] = 0x80 + (unicode_letter & 0x3F);
  } else if (unicode_letter > 0x7F) {
    unicode[0] = 0xC0 + ((unicode_letter >> 6) & 0x1F);
    unicode[1] = 0x80 + (unicode_letter & 0x3F);
  } else {
    unicode[0] = unicode_letter;
  }
  int match_length;
  int glyph_n = this->match_next_glyph(unicode, &match_length);
  if (glyph_n < 0) {
    return nullptr;
  }
  this->last_data_ = &this->get_glyphs()[glyph_n];
  this->last_letter_ = unicode_letter;
  return this->last_data_;
}
#endif

// Compare the char at the string position with this char.
// Return true if this char is less than or equal the other.
bool Glyph::compare_to(const uint8_t *str) const {
  // 1 -> this->char_
  // 2 -> str
  for (uint32_t i = 0;; i++) {
    if (this->a_char[i] == '\0')
      return true;
    if (str[i] == '\0')
      return false;
    if (this->a_char[i] > str[i])
      return false;
    if (this->a_char[i] < str[i])
      return true;
  }
}

int Glyph::match_length(const uint8_t *str) const {
  for (uint32_t i = 0;; i++) {
    if (this->a_char[i] == '\0')
      return i;
    if (str[i] != this->a_char[i])
      return 0;
  }
}

Font::Font(const Glyph *data, int data_nr, int baseline, int height, int descender, int xheight, int capheight,
           uint8_t bpp)
    : glyphs_(ConstVector(data, data_nr)),
      baseline_(baseline),
      height_(height),
      descender_(descender),
      linegap_(height - baseline - descender),
      xheight_(xheight),
      capheight_(capheight),
      bpp_(bpp) {}
int Font::match_next_glyph(const uint8_t *str, int *match_length) const {
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
#ifdef USE_DISPLAY
void Font::measure(const char *str, int *width, int *x_offset, int *baseline, int *height) {
  *baseline = this->baseline_;
  *height = this->height_;
  int i = 0;
  int min_x = 0;
  bool has_char = false;
  int x = 0;
  while (str[i] != '\0') {
    int match_length;
    int glyph_n = this->match_next_glyph((const uint8_t *) str + i, &match_length);
    if (glyph_n < 0) {
      // Unknown char, skip
      if (!this->get_glyphs().empty())
        x += this->get_glyphs()[0].advance;
      i++;
      continue;
    }

    const Glyph &glyph = this->glyphs_[glyph_n];
    if (!has_char) {
      min_x = glyph.offset_x;
    } else {
      min_x = std::min(min_x, x + glyph.offset_x);
    }
    x += glyph.advance;

    i += match_length;
    has_char = true;
  }
  *x_offset = min_x;
  *width = x - min_x;
}
void Font::print(int x_start, int y_start, display::Display *display, Color color, const char *text, Color background) {
  int i = 0;
  int x_at = x_start;
  while (text[i] != '\0') {
    int match_length;
    int glyph_n = this->match_next_glyph((const uint8_t *) text + i, &match_length);
    if (glyph_n < 0) {
      // Unknown char, skip
      ESP_LOGW(TAG, "Encountered character without representation in font: '%c'", text[i]);
      if (!this->get_glyphs().empty()) {
        uint8_t glyph_width = this->get_glyphs()[0].advance;
        display->filled_rectangle(x_at, y_start, glyph_width, this->height_, color);
        x_at += glyph_width;
      }

      i++;
      continue;
    }

    const Glyph &glyph = this->get_glyphs()[glyph_n];

    const uint8_t *data = glyph.data;
    const int max_x = x_at + glyph.offset_x + glyph.width;
    const int max_y = y_start + glyph.offset_y + glyph.height;

    uint8_t bitmask = 0;
    uint8_t pixel_data = 0;
    uint8_t bpp_max = (1 << this->bpp_) - 1;
    auto diff_r = (float) color.r - (float) background.r;
    auto diff_g = (float) color.g - (float) background.g;
    auto diff_b = (float) color.b - (float) background.b;
    auto diff_w = (float) color.w - (float) background.w;
    auto b_r = (float) background.r;
    auto b_g = (float) background.g;
    auto b_b = (float) background.b;
    auto b_w = (float) background.w;
    for (int glyph_y = y_start + glyph.offset_y; glyph_y != max_y; glyph_y++) {
      for (int glyph_x = x_at + glyph.offset_x; glyph_x != max_x; glyph_x++) {
        uint8_t pixel = 0;
        for (uint8_t bit_num = 0; bit_num != this->bpp_; bit_num++) {
          if (bitmask == 0) {
            pixel_data = progmem_read_byte(data++);
            bitmask = 0x80;
          }
          pixel <<= 1;
          if ((pixel_data & bitmask) != 0)
            pixel |= 1;
          bitmask >>= 1;
        }
        if (pixel == bpp_max) {
          display->draw_pixel_at(glyph_x, glyph_y, color);
        } else if (pixel != 0) {
          auto on = (float) pixel / (float) bpp_max;
          auto blended = Color((uint8_t) (diff_r * on + b_r), (uint8_t) (diff_g * on + b_g),
                               (uint8_t) (diff_b * on + b_b), (uint8_t) (diff_w * on + b_w));
          display->draw_pixel_at(glyph_x, glyph_y, blended);
        }
      }
    }
    x_at += glyph.advance;

    i += match_length;
  }
}
#endif

}  // namespace font
}  // namespace esphome
