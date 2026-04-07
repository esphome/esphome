#include "font.h"

#include "esphome/core/color.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_FONT_PTE
namespace {
esphome::display::Display *g_pte_display;
esphome::Color g_pte_color;
esphome::Color g_pte_background;
uint8_t *g_pte_bitmap;
int g_pte_bitmap_width;
int g_pte_bitmap_height;
uint32_t g_pte_bitmap_stride;
bool g_pte_measure_bounds;
int g_pte_min_x;
int g_pte_min_y;
int g_pte_max_x;
int g_pte_max_y;
}

extern "C" void hw_blendPixel(int x, int y, int a, int col) {
  (void) col;

  if (g_pte_measure_bounds) {
    if (a <= 0 || x < 0 || y < 0 || x >= g_pte_bitmap_width || y >= g_pte_bitmap_height) {
      return;
    }
    g_pte_min_x = std::min(g_pte_min_x, x);
    g_pte_min_y = std::min(g_pte_min_y, y);
    g_pte_max_x = std::max(g_pte_max_x, x);
    g_pte_max_y = std::max(g_pte_max_y, y);
    return;
  }

  if (g_pte_bitmap != nullptr) {
    if (a <= 0 || x < 0 || y < 0 || x >= g_pte_bitmap_width || y >= g_pte_bitmap_height) {
      return;
    }
    if (a > 255) {
      a = 255;
    }
    uint8_t *pixel = g_pte_bitmap + y * g_pte_bitmap_stride + x;
    *pixel = static_cast<uint8_t>(a);
    return;
  }

  if (g_pte_display == nullptr || a <= 0) {
    return;
  }

  if (a >= 256) {
    g_pte_display->draw_pixel_at(x, y, g_pte_color);
    return;
  }

	int b = 256 - a;
  auto blended = esphome::Color(((g_pte_background.r * b) >> 8) + ((g_pte_color.r * a) >> 8),
                                ((g_pte_background.g * b) >> 8) + ((g_pte_color.g * a) >> 8),
                                ((g_pte_background.b * b) >> 8) + ((g_pte_color.b * a) >> 8),
                                ((g_pte_background.w * b) >> 8) + ((g_pte_color.w * a) >> 8));

  g_pte_display->draw_pixel_at(x, y, blended);
}
#endif

namespace esphome {
namespace font {

#ifdef USE_DISPLAY
namespace {

class PTEFontSizedVariant : public BaseFont {
 public:
  PTEFontSizedVariant(PTEFont *owner, int size) : owner_(owner), size_(size) {}

  void print(int x_start, int y_start, display::Display *display, Color color, const char *text,
             Color background) override {
    this->owner_->print(x_start, y_start, display, color, text, background, this->size_);
  }

  void measure(const char *str, int *width, int *x_offset, int *baseline, int *height) override {
    this->owner_->measure(str, width, x_offset, baseline, height, this->size_);
  }

  display::BaseFont *get_size_font(int size) override { return this->owner_->get_size_font(size); }

#ifdef USE_LVGL_FONT
  const lv_font_t *get_lv_font(int size = 0) const override {
    return this->owner_->get_lv_font(size > 0 ? size : this->size_);
  }
#endif

 protected:
  PTEFont *owner_;
  int size_;
};

}  // namespace
#endif
static const char *const TAG = "font";

#ifdef USE_LVGL_FONT
static const uint8_t OPA4_TABLE[16] = {0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255};

static const uint8_t OPA2_TABLE[4] = {0, 85, 170, 255};

const void *Font::get_glyph_bitmap(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *draw_buf) {
  const auto *font = dsc->resolved_font;
  auto *const fe = (Font *) font->dsc;

  const auto *gd = fe->get_glyph_data_(dsc->gid.index);
  if (gd == nullptr) {
    return nullptr;
  }

  const uint8_t *bitmap_in = gd->data;
  uint8_t *bitmap_out_tmp = draw_buf->data;
  int32_t i = 0;
  int32_t x, y;
  uint32_t stride = lv_draw_buf_width_to_stride(gd->width, LV_COLOR_FORMAT_A8);

  switch (fe->get_bpp()) {
    case 1: {
      uint8_t mask = 0;
      uint8_t byte = 0;
      for (y = 0; y != gd->height; y++) {
        for (x = 0; x != gd->width; x++) {
          if (mask == 0) {
            mask = 0x80;
            byte = *bitmap_in++;
          }
          bitmap_out_tmp[x] = byte & mask ? 255 : 0;
          mask >>= 1;
        }
        bitmap_out_tmp += stride;
      }
    } break;

    case 2:
      for (y = 0; y != gd->height; y++) {
        for (x = 0; x != gd->width; x++, i++) {
          switch (i & 0x3) {
            default:
              bitmap_out_tmp[x] = OPA2_TABLE[(*bitmap_in) >> 6];
              break;
            case 1:
              bitmap_out_tmp[x] = OPA2_TABLE[((*bitmap_in) >> 4) & 0x3];
              break;
            case 2:
              bitmap_out_tmp[x] = OPA2_TABLE[((*bitmap_in) >> 2) & 0x3];
              break;
            case 3:
              bitmap_out_tmp[x] = OPA2_TABLE[((*bitmap_in) >> 0) & 0x3];
              bitmap_in++;
          }
        }
        bitmap_out_tmp += stride;
      }
      break;

    case 4:
      for (y = 0; y != gd->height; y++) {
        for (x = 0; x != gd->width; x++, i++) {
          i = i & 0x1;
          if (i == 0) {
            bitmap_out_tmp[x] = OPA4_TABLE[(*bitmap_in) >> 4];
          } else if (i == 1) {
            bitmap_out_tmp[x] = OPA4_TABLE[(*bitmap_in) & 0xF];
            bitmap_in++;
          }
        }
        bitmap_out_tmp += stride;
      }
      break;

    case 8:
      memcpy(bitmap_out_tmp, bitmap_in, gd->width * gd->height);
      break;
    default:
      ESP_LOGD(TAG, "Unknown bpp: %d", fe->get_bpp());
      break;
  }
  return draw_buf;
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
  dsc->format = (lv_font_glyph_format_t) fe->get_bpp();
  dsc->gid.index = unicode_letter;
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

#ifdef USE_FONT_PTE
PTEFont::PTEFont(int sample_size, const uint8_t *data, const pte_glyph *glyphs, int glyph_count, const pte_kern *kerns,
                 int kern_count, int line_height, int baseline, int default_render_size)
    : default_render_size_(default_render_size) {
  this->base_font_.m_size = sample_size;
  this->base_font_.m_data = data;
  this->base_font_.m_number_glyphs = glyph_count;
  this->base_font_.m_gylphs = glyphs;
  this->base_font_.m_number_kerns = kern_count;
  this->base_font_.m_kerns = kerns;
  this->base_font_.m_line_height = line_height;
  this->base_font_.m_baseline = baseline;
}

int PTEFont::scale_(int value, int size) const { return (value * size) / this->base_font_.m_size; }

int PTEFont::scale_box_(int value, int size) const {
  if (value <= 0) {
    return 0;
  }
  return (value * size + this->base_font_.m_size - 1) / this->base_font_.m_size;
}

const pte_glyph *PTEFont::find_glyph_(uint32_t codepoint) const {
  int lo = 0;
  int hi = this->base_font_.m_number_glyphs - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    const auto &glyph = this->base_font_.m_gylphs[mid];
    if (glyph.code == codepoint) {
      return &glyph;
    }
    if (glyph.code < codepoint) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return nullptr;
}

const pte_kern *PTEFont::find_kern_(uint32_t first, uint32_t second) const {
  if (this->base_font_.m_kerns == nullptr || this->base_font_.m_number_kerns == 0) {
    return nullptr;
  }

  int lo = 0;
  int hi = this->base_font_.m_number_kerns - 1;
  int match = -1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    const auto &kern = this->base_font_.m_kerns[mid];
    if (kern.first < first) {
      lo = mid + 1;
    } else if (kern.first > first) {
      hi = mid - 1;
    } else {
      match = mid;
      hi = mid - 1;
    }
  }
  if (match == -1) {
    return nullptr;
  }

  for (int index = match; index < this->base_font_.m_number_kerns && this->base_font_.m_kerns[index].first == first;
       index++) {
    if (this->base_font_.m_kerns[index].second == second) {
      return &this->base_font_.m_kerns[index];
    }
  }
  return nullptr;
}

#ifdef USE_LVGL_FONT
const lv_font_t *PTEFont::get_lv_font(int size) const {
  if (size <= 0) {
    size = this->default_render_size_;
  }

  for (const auto &adapter : this->lv_fonts_) {
    if (adapter->size == size) {
      return &adapter->lv_font;
    }
  }

  auto adapter = std::make_unique<LVGLFontAdapter>();
  adapter->owner = this;
  adapter->size = size;
  const auto scaled = ::pte_getFont(&this->base_font_, size);
  adapter->lv_font.dsc = adapter.get();
  adapter->lv_font.line_height = scaled.m_line_height;
  adapter->lv_font.base_line = scaled.m_line_height - scaled.m_baseline;
  adapter->lv_font.get_glyph_dsc = get_lv_glyph_dsc_cb_;
  adapter->lv_font.get_glyph_bitmap = get_lv_glyph_bitmap_;
  adapter->lv_font.subpx = LV_FONT_SUBPX_NONE;
  adapter->lv_font.underline_position = -1;
  adapter->lv_font.underline_thickness = 1;
  const lv_font_t *result = &adapter->lv_font;
  this->lv_fonts_.push_back(std::move(adapter));
  return result;
}

const pte_glyph *PTEFont::get_lv_glyph_data_(const LVGLFontAdapter *adapter, uint32_t unicode_letter) const {
  if (unicode_letter == adapter->last_letter && adapter->last_letter != 0) {
    return adapter->last_data;
  }

  const auto *glyph = this->find_glyph_(unicode_letter);
  if (glyph == nullptr) {
    return nullptr;
  }

  adapter->last_letter = unicode_letter;
  adapter->last_data = glyph;
  return glyph;
}

size_t PTEFont::encode_utf8_(uint32_t codepoint, char *buffer) {
  if (codepoint <= 0x7F) {
    buffer[0] = static_cast<char>(codepoint);
    buffer[1] = '\0';
    return 1;
  }
  if (codepoint <= 0x7FF) {
    buffer[0] = static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
    buffer[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
    buffer[2] = '\0';
    return 2;
  }
  if (codepoint <= 0xFFFF) {
    buffer[0] = static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
    buffer[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    buffer[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
    buffer[3] = '\0';
    return 3;
  }

  buffer[0] = static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
  buffer[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
  buffer[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
  buffer[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
  buffer[4] = '\0';
  return 4;
}

bool PTEFont::measure_lv_glyph_bounds_(const LVGLFontAdapter *adapter, const pte_glyph *glyph, int *ofs_x,
                                       int *ofs_y, int *box_w, int *box_h) const {
  if (adapter->last_metrics_letter == glyph->code) {
    *ofs_x = adapter->last_metrics_ofs_x;
    *ofs_y = adapter->last_metrics_ofs_y;
    *box_w = adapter->last_metrics_box_w;
    *box_h = adapter->last_metrics_box_h;
    return (*box_w > 0 && *box_h > 0);
  }

  char utf8[5];
  const size_t length = encode_utf8_(glyph->code, utf8);
  auto scaled = ::pte_getFont(&this->base_font_, adapter->size);

  const int neg_xoffset = glyph->xoffset < 0 ? -glyph->xoffset : 0;
  const int origin_x = this->scale_box_(neg_xoffset + 2, adapter->size) + 4;
  const int origin_y = scaled.m_baseline + 4;
  const int canvas_w = std::max(origin_x + this->scale_box_(glyph->xadvance + glyph->width + 4, adapter->size) + 4, 8);
  const int canvas_h = std::max(scaled.m_line_height * 2 + 8, 8);

  g_pte_bitmap_width = canvas_w;
  g_pte_bitmap_height = canvas_h;
  g_pte_measure_bounds = true;
  g_pte_min_x = canvas_w;
  g_pte_min_y = canvas_h;
  g_pte_max_x = -1;
  g_pte_max_y = -1;
  g_pte_bitmap_width = canvas_w;
  g_pte_bitmap_height = canvas_h;
  ::pte_drawText(&scaled, origin_x, origin_y, 0, utf8, length, 0);
  g_pte_measure_bounds = false;
  g_pte_bitmap_width = 0;
  g_pte_bitmap_height = 0;
  const int min_x = g_pte_min_x;
  const int min_y = g_pte_min_y;
  const int max_x = g_pte_max_x;
  const int max_y = g_pte_max_y;

  if (max_x < min_x || max_y < min_y) {
    adapter->last_metrics_letter = glyph->code;
    adapter->last_metrics_ofs_x = 0;
    adapter->last_metrics_ofs_y = 0;
    adapter->last_metrics_box_w = 0;
    adapter->last_metrics_box_h = 0;
    *ofs_x = 0;
    *ofs_y = 0;
    *box_w = 0;
    *box_h = 0;
    return false;
  }

  adapter->last_metrics_letter = glyph->code;
  adapter->last_metrics_ofs_x = min_x - origin_x;
  adapter->last_metrics_ofs_y = min_y - origin_y - 1;
  adapter->last_metrics_box_w = max_x - min_x + 1;
  adapter->last_metrics_box_h = max_y - min_y + 2;
  *ofs_x = adapter->last_metrics_ofs_x;
  *ofs_y = adapter->last_metrics_ofs_y;
  *box_w = adapter->last_metrics_box_w;
  *box_h = adapter->last_metrics_box_h;
  return true;
}

void PTEFont::render_lv_glyph_bitmap_(const LVGLFontAdapter *adapter, const pte_glyph *glyph, lv_draw_buf_t *draw_buf) {
  int ofs_x;
  int top_rel_baseline;
  int box_w;
  int box_h;
  if (!adapter->owner->measure_lv_glyph_bounds_(adapter, glyph, &ofs_x, &top_rel_baseline, &box_w, &box_h)) {
    return;
  }

  uint8_t *bitmap = static_cast<uint8_t *>(draw_buf->data);
  const uint32_t stride = lv_draw_buf_width_to_stride(box_w, LV_COLOR_FORMAT_A8);
  memset(bitmap, 0, stride * box_h);

  char utf8[5];
  const size_t length = encode_utf8_(glyph->code, utf8);
  auto scaled = ::pte_getFont(&adapter->owner->base_font_, adapter->size);

  g_pte_bitmap = bitmap;
  g_pte_bitmap_width = box_w;
  g_pte_bitmap_height = box_h;
  g_pte_bitmap_stride = stride;
  ::pte_drawText(&scaled, -ofs_x, -top_rel_baseline, 0, utf8, length, 0);
  g_pte_bitmap = nullptr;
  g_pte_bitmap_width = 0;
  g_pte_bitmap_height = 0;
  g_pte_bitmap_stride = 0;
}

const void *PTEFont::get_lv_glyph_bitmap_(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *draw_buf) {
  const auto *font = dsc->resolved_font;
  const auto *adapter = static_cast<const LVGLFontAdapter *>(font->dsc);
  const auto *glyph = adapter->owner->get_lv_glyph_data_(adapter, dsc->gid.index);
  if (glyph == nullptr) {
    return nullptr;
  }

  render_lv_glyph_bitmap_(adapter, glyph, draw_buf);
  return draw_buf;
}

bool PTEFont::get_lv_glyph_dsc_cb_(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t unicode_letter,
                                   uint32_t next) {
  const auto *adapter = static_cast<const LVGLFontAdapter *>(font->dsc);
  const auto *glyph = adapter->owner->get_lv_glyph_data_(adapter, unicode_letter);
  if (glyph == nullptr) {
    return false;
  }

  int advance = adapter->owner->scale_(glyph->xadvance, adapter->size);
  if (const auto *kern = adapter->owner->find_kern_(unicode_letter, next)) {
    advance += adapter->owner->scale_(kern->amount, adapter->size);
  }

  int ofs_x;
  int top_rel_baseline;
  int box_w;
  int box_h;
  adapter->owner->measure_lv_glyph_bounds_(adapter, glyph, &ofs_x, &top_rel_baseline, &box_w, &box_h);
  const int lv_ofs_y = -top_rel_baseline - box_h;

  dsc->adv_w = advance;
  dsc->ofs_x = ofs_x;
  dsc->box_w = box_w;
  dsc->box_h = box_h;
  dsc->ofs_y = lv_ofs_y;
  dsc->is_placeholder = 0;
  dsc->format = LV_FONT_GLYPH_FORMAT_A8;
  dsc->gid.index = unicode_letter;
  return true;
}
#endif

#ifdef USE_DISPLAY
void PTEFont::print(int x_start, int y_start, display::Display *display, Color color, const char *text,
                    Color background) {
  this->print(x_start, y_start, display, color, text, background, this->default_render_size_);
}

void PTEFont::print(int x_start, int y_start, display::Display *display, Color color, const char *text,
                    Color background, int size) {
  if (size <= 0) {
    size = this->default_render_size_;
  }

  auto saved_display = g_pte_display;
  auto saved_color = g_pte_color;
  auto saved_background = g_pte_background;

  g_pte_display = display;
  g_pte_color = color;
  g_pte_background = background;

  auto scaled = ::pte_getFont(&this->base_font_, size);
  ::pte_drawText(&scaled, x_start, y_start + scaled.m_baseline, 0, text, static_cast<size_t>(-1), 0);

  g_pte_display = saved_display;
  g_pte_color = saved_color;
  g_pte_background = saved_background;
}

void PTEFont::measure(const char *str, int *width, int *x_offset, int *baseline, int *height) {
  this->measure(str, width, x_offset, baseline, height, this->default_render_size_);
}

void PTEFont::measure(const char *str, int *width, int *x_offset, int *baseline, int *height, int size) {
  if (size <= 0) {
    size = this->default_render_size_;
  }

  const auto scaled = ::pte_getFont(&this->base_font_, size);
  *baseline = scaled.m_baseline;
  *height = scaled.m_line_height;

  int x = 0;
  int min_x = 0;
  int max_x = 0;
  bool has_char = false;
  uint32_t last_char = UINT32_MAX;

  while (true) {
    size_t length;
    uint32_t codepoint = extract_unicode_codepoint(str, &length);
    if (length == 0) {
      break;
    }
    str += length;

    const auto *glyph = this->find_glyph_(codepoint);
    if (glyph == nullptr) {
      continue;
    }

    if (const auto *kern = this->find_kern_(last_char, codepoint)) {
      x += this->scale_(kern->amount, size);
    }

    int glyph_x = x + this->scale_(glyph->xoffset, size);
    int glyph_right = glyph_x + this->scale_(glyph->width, size);
    if (!has_char) {
      min_x = glyph_x;
      max_x = glyph_right;
    } else {
      min_x = std::min(min_x, glyph_x);
      max_x = std::max(max_x, glyph_right);
    }

    x += this->scale_(glyph->xadvance, size);
    last_char = codepoint;
    has_char = true;
  }

  *x_offset = has_char ? min_x : 0;
  *width = has_char ? (max_x - min_x) : 0;
}

display::BaseFont *PTEFont::get_size_font(int size) {
  if (size <= 0 || size == this->default_render_size_) {
    return this;
  }

  for (auto &entry : this->sized_fonts_) {
    if (entry.size == size) {
      return entry.font.get();
    }
  }

  auto sized_font = std::make_unique<PTEFontSizedVariant>(this, size);
  auto *sized_font_ptr = sized_font.get();
  this->sized_fonts_.push_back({size, std::move(sized_font)});
  return sized_font_ptr;
}
#endif
#endif

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
