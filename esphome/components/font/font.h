#pragma once

#include "esphome/core/color.h"
#include "esphome/core/datatypes.h"
#include "esphome/core/defines.h"
#include <memory>
#include <vector>
#ifdef USE_DISPLAY
#include "esphome/components/display/display.h"
#endif
#ifdef USE_FONT_PTE
#include "pte.h"
#endif
#ifdef USE_LVGL_FONT
#include <lvgl.h>
#endif

namespace esphome {
namespace font {

class Font;

class Glyph {
 public:
  constexpr Glyph(uint32_t code_point, const uint8_t *data, int advance, int offset_x, int offset_y, int width,
                  int height)
      : code_point(code_point),
        data(data),
        advance(advance),
        offset_x(offset_x),
        offset_y(offset_y),
        width(width),
        height(height) {}

  bool is_less_or_equal(uint32_t other) const { return this->code_point <= other; }

  const uint32_t code_point;
  const uint8_t *data;
  int advance;
  int offset_x;
  int offset_y;
  int width;
  int height;
};

class BaseFont
#ifdef USE_DISPLAY
    : public display::BaseFont
#endif
{
 public:
#ifdef USE_LVGL_FONT
  const lv_font_t *get_lv_font() const { return this->get_lv_font(0); }
  virtual const lv_font_t *get_lv_font(int size) const = 0;
#endif
};

class Font : public BaseFont {
 public:
  /** Construct the font with the given glyphs.
   *
   * @param data A list of glyphs, must be sorted lexicographically.
   * @param data_nr The number of glyphs
   * @param baseline The y-offset from the top of the text to the baseline.
   * @param height The y-offset from the top of the text to the bottom.
   * @param descender The y-offset from the baseline to the lowest stroke in the font (e.g. from letters like g or p).
   * @param xheight The height of lowercase letters, usually measured at the "x" glyph.
   * @param capheight The height of capital letters, usually measured at the "X" glyph.
   * @param bpp The bits per pixel used for this font. Used to read data out of the glyph bitmaps.
   */
  Font(const Glyph *data, int data_nr, int baseline, int height, int descender, int xheight, int capheight,
       uint8_t bpp = 1);

  const Glyph *find_glyph(uint32_t codepoint) const;

#ifdef USE_DISPLAY
  void print(int x_start, int y_start, display::Display *display, Color color, const char *text,
             Color background) override;
  void measure(const char *str, int *width, int *x_offset, int *baseline, int *height) override;
#endif
  inline int get_baseline() { return this->baseline_; }
  inline int get_height() { return this->height_; }
  inline int get_ascender() { return this->baseline_; }
  inline int get_descender() { return this->descender_; }
  inline int get_linegap() { return this->linegap_; }
  inline int get_xheight() { return this->xheight_; }
  inline int get_capheight() { return this->capheight_; }
  inline int get_bpp() { return this->bpp_; }
#ifdef USE_LVGL_FONT
  const lv_font_t *get_lv_font(int size) const override {
    (void) size;
    return &this->lv_font_;
  }
#endif

  const ConstVector<Glyph> &get_glyphs() const { return glyphs_; }

 protected:
  ConstVector<Glyph> glyphs_;
  int baseline_;
  int height_;
  int descender_;
  int linegap_;
  int xheight_;
  int capheight_;
  uint8_t bpp_;  // bits per pixel
#ifdef USE_LVGL_FONT
  lv_font_t lv_font_{};
  static const void *get_glyph_bitmap(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *draw_buf);
  static bool get_glyph_dsc_cb(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t unicode_letter, uint32_t next);
  const Glyph *get_glyph_data_(uint32_t unicode_letter);
  uint32_t last_letter_{};
  const Glyph *last_data_{};
#endif
};

#ifdef USE_FONT_PTE
class PTEFont : public BaseFont {
 public:
  PTEFont(int sample_size, const uint8_t *data, const PteGlyph *glyphs, int glyph_count, const PteKern *kerns,
          int kern_count, int line_height, int baseline, int default_render_size = 20);

#ifdef USE_DISPLAY
  void print(int x_start, int y_start, display::Display *display, Color color, const char *text,
             Color background) override;
  void print(int x_start, int y_start, display::Display *display, Color color, const char *text, Color background,
             int size) override;
  void measure(const char *str, int *width, int *x_offset, int *baseline, int *height) override;
  void measure(const char *str, int *width, int *x_offset, int *baseline, int *height, int size) override;
  display::BaseFont *get_size_font(int size) override;
#endif

#ifdef USE_LVGL_FONT
  const lv_font_t *get_lv_font(int size) const override;
#endif

 protected:
  int scale_(int value, int size) const;
  int scale_box_(int value, int size) const;
  const PteGlyph *find_glyph_(uint32_t codepoint) const;
  const PteKern *find_kern_(uint32_t first, uint32_t second) const;

#ifdef USE_LVGL_FONT
  struct LVGLFontAdapter {
    lv_font_t lv_font{};
    const PTEFont *owner{nullptr};
    int size{0};
    mutable uint32_t last_letter{0};
    mutable const PteGlyph *last_data{nullptr};
    mutable uint32_t last_metrics_letter{0};
    mutable int last_metrics_ofs_x{0};
    mutable int last_metrics_ofs_y{0};
    mutable int last_metrics_box_w{0};
    mutable int last_metrics_box_h{0};
  };

  static const void *get_lv_glyph_bitmap_(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *draw_buf);
  static bool get_lv_glyph_dsc_cb_(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t unicode_letter,
                                   uint32_t next);
  const PteGlyph *get_lv_glyph_data_(const LVGLFontAdapter *adapter, uint32_t unicode_letter) const;
  bool measure_lv_glyph_bounds_(const LVGLFontAdapter *adapter, const PteGlyph *glyph, int *ofs_x, int *ofs_y,
                                int *box_w, int *box_h) const;
  static size_t encode_utf8_(uint32_t codepoint, char *buffer);
  static void render_lv_glyph_bitmap_(const LVGLFontAdapter *adapter, const PteGlyph *glyph, lv_draw_buf_t *draw_buf);

  mutable std::vector<std::unique_ptr<LVGLFontAdapter>> lv_fonts_{};
#endif

  PteBaseFont base_font_{};
  int default_render_size_{20};
#ifdef USE_DISPLAY
  struct SizedFontCacheEntry {
    int size;
    std::unique_ptr<BaseFont> font;
  };
  mutable std::vector<SizedFontCacheEntry> sized_fonts_{};
#endif
};
#endif

}  // namespace font
}  // namespace esphome
