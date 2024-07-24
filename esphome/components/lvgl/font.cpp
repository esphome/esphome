#include "lvgl_esphome.h"

#ifdef USE_LVGL_FONT
namespace esphome {
namespace lvgl {

static const uint8_t *get_glyph_bitmap(const lv_font_t *font, uint32_t unicode_letter) {
  auto *fe = (FontEngine *) font->dsc;
  const auto *gd = fe->get_glyph_data(unicode_letter);
  if (gd == nullptr)
    return nullptr;
  // esph_log_d(TAG, "Returning bitmap @  %X", (uint32_t)gd->data);

  return gd->data;
}

static bool get_glyph_dsc_cb(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t unicode_letter, uint32_t next) {
  auto *fe = (FontEngine *) font->dsc;
  const auto *gd = fe->get_glyph_data(unicode_letter);
  if (gd == nullptr)
    return false;
  dsc->adv_w = gd->offset_x + gd->width;
  dsc->ofs_x = gd->offset_x;
  dsc->ofs_y = fe->height - gd->height - gd->offset_y - fe->baseline;
  dsc->box_w = gd->width;
  dsc->box_h = gd->height;
  dsc->is_placeholder = 0;
  dsc->bpp = fe->bpp;
  return true;
}

FontEngine::FontEngine(font::Font *esp_font) : font_(esp_font) {
  this->bpp = esp_font->get_bpp();
  this->lv_font_.dsc = this;
  this->lv_font_.line_height = this->height = esp_font->get_height();
  this->lv_font_.base_line = this->baseline = this->lv_font_.line_height - esp_font->get_baseline();
  this->lv_font_.get_glyph_dsc = get_glyph_dsc_cb;
  this->lv_font_.get_glyph_bitmap = get_glyph_bitmap;
  this->lv_font_.subpx = LV_FONT_SUBPX_NONE;
  this->lv_font_.underline_position = -1;
  this->lv_font_.underline_thickness = 1;
}

const lv_font_t *FontEngine::get_lv_font() { return &this->lv_font_; }

const font::GlyphData *FontEngine::get_glyph_data(uint32_t unicode_letter) {
  if (unicode_letter == last_letter_)
    return this->last_data_;
  uint8_t unicode[5];
  memset(unicode, 0, sizeof unicode);
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
  int glyph_n = this->font_->match_next_glyph(unicode, &match_length);
  if (glyph_n < 0)
    return nullptr;
  this->last_data_ = this->font_->get_glyphs()[glyph_n].get_glyph_data();
  this->last_letter_ = unicode_letter;
  return this->last_data_;
}
}  // namespace lvgl
}  // namespace esphome
#endif  // USES_LVGL_FONT
