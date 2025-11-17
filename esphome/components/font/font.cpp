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
  if (unicode_letter == this->last_letter_ && this->last_letter_ != 0)
    return this->last_data_;
  auto *glyph = this->find_glyph(unicode_letter);
  if (glyph == nullptr) {
    return nullptr;
  }
  this->last_data_ = glyph;
  this->last_letter_ = unicode_letter;
  return glyph;
}
#endif

/**
 *  Attempt to extract a 32 bit Unicode codepoint from a UTF-8 string.
 *  If successful, return the codepoint and set the length to the number of bytes read.
 *  If the end of the string has been reached and a valid codepoint has not been found, return 0 and set the length to
 * 0.
 *
 * @param utf8_str The input string
 * @param length Pointer to length storage
 * @return The extracted code point
 */
static uint32_t extract_unicode_codepoint(const char *utf8_str, size_t *length) {
  // Safely cast to uint8_t* for correct bitwise operations on bytes
  const uint8_t *current = reinterpret_cast<const uint8_t *>(utf8_str);
  uint32_t code_point = 0;
  uint8_t c1 = *current++;

  // check for end of string
  if (c1 == 0) {
    *length = 0;
    return 0;
  }

  // --- 1-Byte Sequence: 0xxxxxxx (ASCII) ---
  if (c1 < 0x80) {
    // Valid ASCII byte.
    code_point = c1;
    // Optimization: No need to check for continuation bytes.
  }
  // --- 2-Byte Sequence: 110xxxxx 10xxxxxx ---
  else if ((c1 & 0xE0) == 0xC0) {
    uint8_t c2 = *current++;

    // Error Check 1: Check if c2 is a valid continuation byte (10xxxxxx)
    if ((c2 & 0xC0) != 0x80) {
      *length = 0;
      return 0;
    }

    code_point = (c1 & 0x1F) << 6;
    code_point |= (c2 & 0x3F);

    // Error Check 2: Overlong check (2-byte must be > 0x7F)
    if (code_point <= 0x7F) {
      *length = 0;
      return 0;
    }
  }
  // --- 3-Byte Sequence: 1110xxxx 10xxxxxx 10xxxxxx ---
  else if ((c1 & 0xF0) == 0xE0) {
    uint8_t c2 = *current++;
    uint8_t c3 = *current++;

    // Error Check 1: Check continuation bytes
    if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80)) {
      *length = 0;
      return 0;
    }

    code_point = (c1 & 0x0F) << 12;
    code_point |= (c2 & 0x3F) << 6;
    code_point |= (c3 & 0x3F);

    // Error Check 2: Overlong check (3-byte must be > 0x7FF)
    // Also check for surrogates (0xD800-0xDFFF)
    if (code_point <= 0x7FF || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
      *length = 0;
      return 0;
    }
  }
  // --- 4-Byte Sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx ---
  else if ((c1 & 0xF8) == 0xF0) {
    uint8_t c2 = *current++;
    uint8_t c3 = *current++;
    uint8_t c4 = *current++;

    // Error Check 1: Check continuation bytes
    if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80) || ((c4 & 0xC0) != 0x80)) {
      *length = 0;
      return 0;
    }

    code_point = (c1 & 0x07) << 18;
    code_point |= (c2 & 0x3F) << 12;
    code_point |= (c3 & 0x3F) << 6;
    code_point |= (c4 & 0x3F);

    // Error Check 2: Overlong check (4-byte must be > 0xFFFF)
    // Also check for valid Unicode range (must be <= 0x10FFFF)
    if (code_point <= 0xFFFF || code_point > 0x10FFFF) {
      *length = 0;
      return 0;
    }
  }
  // --- Invalid leading byte (e.g., 10xxxxxx or 11111xxx) ---
  else {
    *length = 0;
    return 0;
  }
  *length = current - reinterpret_cast<const uint8_t *>(utf8_str);
  return code_point;
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
      bpp_(bpp) {
#ifdef USE_LVGL_FONT
  this->lv_font_.dsc = this;
  this->lv_font_.line_height = this->get_height();
  this->lv_font_.base_line = this->lv_font_.line_height - this->get_baseline();
  this->lv_font_.get_glyph_dsc = get_glyph_dsc_cb;
  this->lv_font_.get_glyph_bitmap = get_glyph_bitmap;
  this->lv_font_.subpx = LV_FONT_SUBPX_NONE;
  this->lv_font_.underline_position = -1;
  this->lv_font_.underline_thickness = 1;
#endif
}

const Glyph *Font::find_glyph(uint32_t codepoint) const {
  int lo = 0;
  int hi = this->glyphs_.size() - 1;
  while (lo != hi) {
    int mid = (lo + hi + 1) / 2;
    if (this->glyphs_[mid].is_less_or_equal(codepoint)) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  auto *result = &this->glyphs_[lo];
  if (result->code_point == codepoint)
    return result;
  return nullptr;
}

#ifdef USE_DISPLAY
void Font::measure(const char *str, int *width, int *x_offset, int *baseline, int *height) {
  *baseline = this->baseline_;
  *height = this->height_;
  int min_x = 0;
  bool has_char = false;
  int x = 0;
  for (;;) {
    size_t length;
    auto code_point = extract_unicode_codepoint(str, &length);
    if (length == 0)
      break;
    str += length;
    auto *glyph = this->find_glyph(code_point);
    if (glyph == nullptr) {
      // Unknown char, skip
      if (!this->glyphs_.empty())
        x += this->glyphs_[0].advance;
      continue;
    }

    if (!has_char) {
      min_x = glyph->offset_x;
    } else {
      min_x = std::min(min_x, x + glyph->offset_x);
    }
    x += glyph->advance;

    has_char = true;
  }
  *x_offset = min_x;
  *width = x - min_x;
}

void Font::print(int x_start, int y_start, display::Display *display, Color color, const char *text, Color background) {
  int x_at = x_start;
  for (;;) {
    size_t length;
    auto code_point = extract_unicode_codepoint(text, &length);
    if (length == 0)
      break;
    text += length;
    auto *glyph = this->find_glyph(code_point);
    if (glyph == nullptr) {
      // Unknown char, skip
      ESP_LOGW(TAG, "Codepoint 0x%08" PRIx32 " not found in font", code_point);
      if (!this->glyphs_.empty()) {
        uint8_t glyph_width = this->glyphs_[0].advance;
        display->rectangle(x_at, y_start, glyph_width, this->height_, color);
        x_at += glyph_width;
      }
      continue;
    }

    const uint8_t *data = glyph->data;
    const int max_x = x_at + glyph->offset_x + glyph->width;
    const int max_y = y_start + glyph->offset_y + glyph->height;

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
    for (int glyph_y = y_start + glyph->offset_y; glyph_y != max_y; glyph_y++) {
      for (int glyph_x = x_at + glyph->offset_x; glyph_x != max_x; glyph_x++) {
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
    x_at += glyph->advance;
  }
}
#endif
}  // namespace font
}  // namespace esphome
