#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "display_color_utils.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace display {

/** TextAlign is used to tell the display class how to position a piece of text. By default
 * the coordinates you enter for the print*() functions take the upper left corner of the text
 * as the "anchor" point. You can customize this behavior to, for example, make the coordinates
 * refer to the *center* of the text.
 *
 * All text alignments consist of an X and Y-coordinate alignment. For the alignment along the X-axis
 * these options are allowed:
 *
 * - LEFT (x-coordinate of anchor point is on left)
 * - CENTER_HORIZONTAL (x-coordinate of anchor point is in the horizontal center of the text)
 * - RIGHT (x-coordinate of anchor point is on right)
 *
 * For the Y-Axis alignment these options are allowed:
 *
 * - TOP (y-coordinate of anchor is on the top of the text)
 * - CENTER_VERTICAL (y-coordinate of anchor is in the vertical center of the text)
 * - BASELINE (y-coordinate of anchor is on the baseline of the text)
 * - BOTTOM (y-coordinate of anchor is on the bottom of the text)
 *
 * These options are then combined to create combined TextAlignment options like:
 * - TOP_LEFT (default)
 * - CENTER (anchor point is in the middle of the text bounds)
 * - ...
 */
enum class TextAlign {
  TOP = 0x00,
  CENTER_VERTICAL = 0x01,
  BASELINE = 0x02,
  BOTTOM = 0x04,

  LEFT = 0x00,
  CENTER_HORIZONTAL = 0x08,
  RIGHT = 0x10,

  TOP_LEFT = TOP | LEFT,
  TOP_CENTER = TOP | CENTER_HORIZONTAL,
  TOP_RIGHT = TOP | RIGHT,

  CENTER_LEFT = CENTER_VERTICAL | LEFT,
  CENTER = CENTER_VERTICAL | CENTER_HORIZONTAL,
  CENTER_RIGHT = CENTER_VERTICAL | RIGHT,

  BASELINE_LEFT = BASELINE | LEFT,
  BASELINE_CENTER = BASELINE | CENTER_HORIZONTAL,
  BASELINE_RIGHT = BASELINE | RIGHT,

  BOTTOM_LEFT = BOTTOM | LEFT,
  BOTTOM_CENTER = BOTTOM | CENTER_HORIZONTAL,
  BOTTOM_RIGHT = BOTTOM | RIGHT,
};

/// Turn the pixel OFF.
extern const Color COLOR_OFF;
/// Turn the pixel ON.
extern const Color COLOR_ON;

enum ImageType { IMAGE_TYPE_BINARY = 0, IMAGE_TYPE_GRAYSCALE = 1, IMAGE_TYPE_RGB24 = 2 };

enum DisplayRotation {
  DISPLAY_ROTATION_0_DEGREES = 0,
  DISPLAY_ROTATION_90_DEGREES = 90,
  DISPLAY_ROTATION_180_DEGREES = 180,
  DISPLAY_ROTATION_270_DEGREES = 270,
};

class Font;
class Image;
class DisplayBuffer;
class DisplayPage;
class DisplayOnPageChangeTrigger;

using display_writer_t = std::function<void(DisplayBuffer &)>;

#define LOG_DISPLAY(prefix, type, obj) \
  if (obj != nullptr) { \
    ESP_LOGCONFIG(TAG, prefix type); \
    ESP_LOGCONFIG(TAG, "%s  Rotations: %d °", prefix, obj->rotation_); \
    ESP_LOGCONFIG(TAG, "%s  Dimensions: %dpx x %dpx", prefix, obj->get_width(), obj->get_height()); \
  }

class DisplayBuffer {
 public:
  /// Fill the entire screen with the given color.
  virtual void fill(Color color);
  /// Clear the entire screen by filling it with OFF pixels.
  void clear();

  /// Get the width of the image in pixels with rotation applied.
  int get_width();
  /// Get the height of the image in pixels with rotation applied.
  int get_height();
  /// Set a single pixel at the specified coordinates to the given color.
  void draw_pixel_at(int x, int y, Color color = COLOR_ON);

  /// Draw a straight line from the point [x1,y1] to [x2,y2] with the given color.
  void line(int x1, int y1, int x2, int y2, Color color = COLOR_ON);

  /// Draw a horizontal line from the point [x,y] to [x+width,y] with the given color.
  void horizontal_line(int x, int y, int width, Color color = COLOR_ON);

  /// Draw a vertical line from the point [x,y] to [x,y+width] with the given color.
  void vertical_line(int x, int y, int height, Color color = COLOR_ON);

  /// Draw the outline of a rectangle with the top left point at [x1,y1] and the bottom right point at
  /// [x1+width,y1+height].
  void rectangle(int x1, int y1, int width, int height, Color color = COLOR_ON);

  /// Fill a rectangle with the top left point at [x1,y1] and the bottom right point at [x1+width,y1+height].
  void filled_rectangle(int x1, int y1, int width, int height, Color color = COLOR_ON);

  /// Draw the outline of a circle centered around [center_x,center_y] with the radius radius with the given color.
  void circle(int center_x, int center_xy, int radius, Color color = COLOR_ON);

  /// Fill a circle centered around [center_x,center_y] with the radius radius with the given color.
  void filled_circle(int center_x, int center_y, int radius, Color color = COLOR_ON);

  /** Print `text` with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param align The alignment of the text.
   * @param text The text to draw.
   */
  void print(int x, int y, Font *font, Color color, TextAlign align, const char *text);

  /** Print `text` with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param text The text to draw.
   */
  void print(int x, int y, Font *font, Color color, const char *text);

  /** Print `text` with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param align The alignment of the text.
   * @param text The text to draw.
   */
  void print(int x, int y, Font *font, TextAlign align, const char *text);

  /** Print `text` with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param text The text to draw.
   */
  void print(int x, int y, Font *font, const char *text);

  /** Evaluate the printf-format `format` and print the result with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param align The alignment of the text.
   * @param format The format to use.
   * @param ... The arguments to use for the text formatting.
   */
  void printf(int x, int y, Font *font, Color color, TextAlign align, const char *format, ...)
      __attribute__((format(printf, 7, 8)));

  /** Evaluate the printf-format `format` and print the result with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param format The format to use.
   * @param ... The arguments to use for the text formatting.
   */
  void printf(int x, int y, Font *font, Color color, const char *format, ...) __attribute__((format(printf, 6, 7)));

  /** Evaluate the printf-format `format` and print the result with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param align The alignment of the text.
   * @param format The format to use.
   * @param ... The arguments to use for the text formatting.
   */
  void printf(int x, int y, Font *font, TextAlign align, const char *format, ...) __attribute__((format(printf, 6, 7)));

  /** Evaluate the printf-format `format` and print the result with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param format The format to use.
   * @param ... The arguments to use for the text formatting.
   */
  void printf(int x, int y, Font *font, const char *format, ...) __attribute__((format(printf, 5, 6)));

#ifdef USE_TIME
  /** Evaluate the strftime-format `format` and print the result with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param align The alignment of the text.
   * @param format The strftime format to use.
   * @param time The time to format.
   */
  void strftime(int x, int y, Font *font, Color color, TextAlign align, const char *format, time::ESPTime time)
      __attribute__((format(strftime, 7, 0)));

  /** Evaluate the strftime-format `format` and print the result with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param format The strftime format to use.
   * @param time The time to format.
   */
  void strftime(int x, int y, Font *font, Color color, const char *format, time::ESPTime time)
      __attribute__((format(strftime, 6, 0)));

  /** Evaluate the strftime-format `format` and print the result with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param align The alignment of the text.
   * @param format The strftime format to use.
   * @param time The time to format.
   */
  void strftime(int x, int y, Font *font, TextAlign align, const char *format, time::ESPTime time)
      __attribute__((format(strftime, 6, 0)));

  /** Evaluate the strftime-format `format` and print the result with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param format The strftime format to use.
   * @param time The time to format.
   */
  void strftime(int x, int y, Font *font, const char *format, time::ESPTime time)
      __attribute__((format(strftime, 5, 0)));
#endif

  /** Draw the `image` with the top-left corner at [x,y] to the screen.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param image The image to draw
   * @param color_on The color to replace in binary images for the on bits.
   * @param color_off The color to replace in binary images for the off bits.
   */
  void image(int x, int y, Image *image, Color color_on = COLOR_ON, Color color_off = COLOR_OFF);

  /** Get the text bounds of the given string.
   *
   * @param x The x coordinate to place the string at, can be 0 if only interested in dimensions.
   * @param y The y coordinate to place the string at, can be 0 if only interested in dimensions.
   * @param text The text to measure.
   * @param font The font to measure the text bounds with.
   * @param align The alignment of the text. Set to TextAlign::TOP_LEFT if only interested in dimensions.
   * @param x1 A pointer to store the returned x coordinate of the upper left corner in.
   * @param y1 A pointer to store the returned y coordinate of the upper left corner in.
   * @param width A pointer to store the returned text width in.
   * @param height A pointer to store the returned text height in.
   */
  void get_text_bounds(int x, int y, const char *text, Font *font, TextAlign align, int *x1, int *y1, int *width,
                       int *height);

  /// Internal method to set the display writer lambda.
  void set_writer(display_writer_t &&writer);

  void show_page(DisplayPage *page);
  void show_next_page();
  void show_prev_page();

  void set_pages(std::vector<DisplayPage *> pages);

  const DisplayPage *get_active_page() const { return this->page_; }

  void add_on_page_change_trigger(DisplayOnPageChangeTrigger *t) { on_page_change_triggers_.push_back(t); }

  /// Internal method to set the display rotation with.
  void set_rotation(DisplayRotation rotation);

 protected:
  void vprintf_(int x, int y, Font *font, Color color, TextAlign align, const char *format, va_list arg);

  virtual void draw_absolute_pixel_internal(int x, int y, Color color) = 0;

  virtual int get_height_internal() = 0;

  virtual int get_width_internal() = 0;

  void init_internal_(uint32_t buffer_length);

  void do_update_();

  uint8_t *buffer_{nullptr};
  DisplayRotation rotation_{DISPLAY_ROTATION_0_DEGREES};
  optional<display_writer_t> writer_{};
  DisplayPage *page_{nullptr};
  DisplayPage *previous_page_{nullptr};
  std::vector<DisplayOnPageChangeTrigger *> on_page_change_triggers_;
};

class DisplayPage {
 public:
  DisplayPage(const display_writer_t &writer);
  void show();
  void show_next();
  void show_prev();
  void set_parent(DisplayBuffer *parent);
  void set_prev(DisplayPage *prev);
  void set_next(DisplayPage *next);
  const display_writer_t &get_writer() const;

 protected:
  DisplayBuffer *parent_;
  display_writer_t writer_;
  DisplayPage *prev_{nullptr};
  DisplayPage *next_{nullptr};
};

class Glyph {
 public:
  Glyph(const char *a_char, const uint8_t *data_start, uint32_t offset, int offset_x, int offset_y, int width,
        int height);

  bool get_pixel(int x, int y) const;

  const char *get_char() const;

  bool compare_to(const char *str) const;

  int match_length(const char *str) const;

  void scan_area(int *x1, int *y1, int *width, int *height) const;

 protected:
  friend Font;
  friend DisplayBuffer;

  const char *char_;
  const uint8_t *data_;
  int offset_x_;
  int offset_y_;
  int width_;
  int height_;
};

class Font {
 public:
  /** Construct the font with the given glyphs.
   *
   * @param glyphs A vector of glyphs, must be sorted lexicographically.
   * @param baseline The y-offset from the top of the text to the baseline.
   * @param bottom The y-offset from the top of the text to the bottom (i.e. height).
   */
  Font(std::vector<Glyph> &&glyphs, int baseline, int bottom);

  int match_next_glyph(const char *str, int *match_length);

  void measure(const char *str, int *width, int *x_offset, int *baseline, int *height);

  const std::vector<Glyph> &get_glyphs() const;

 protected:
  std::vector<Glyph> glyphs_;
  int baseline_;
  int bottom_;
};

class Image {
 public:
  Image(const uint8_t *data_start, int width, int height, ImageType type);
  virtual bool get_pixel(int x, int y) const;
  virtual Color get_color_pixel(int x, int y) const;
  virtual Color get_grayscale_pixel(int x, int y) const;
  int get_width() const;
  int get_height() const;
  ImageType get_type() const;

 protected:
  int width_;
  int height_;
  ImageType type_;
  const uint8_t *data_start_;
};

class Animation : public Image {
 public:
  Animation(const uint8_t *data_start, int width, int height, uint32_t animation_frame_count, ImageType type);
  bool get_pixel(int x, int y) const override;
  Color get_color_pixel(int x, int y) const override;
  Color get_grayscale_pixel(int x, int y) const override;

  int get_animation_frame_count() const;
  int get_current_frame() const;
  void next_frame();

 protected:
  int current_frame_;
  int animation_frame_count_;
};

template<typename... Ts> class DisplayPageShowAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(DisplayPage *, page)

  void play(Ts... x) override {
    auto *page = this->page_.value(x...);
    if (page != nullptr) {
      page->show();
    }
  }
};

template<typename... Ts> class DisplayPageShowNextAction : public Action<Ts...> {
 public:
  DisplayPageShowNextAction(DisplayBuffer *buffer) : buffer_(buffer) {}

  void play(Ts... x) override { this->buffer_->show_next_page(); }

  DisplayBuffer *buffer_;
};

template<typename... Ts> class DisplayPageShowPrevAction : public Action<Ts...> {
 public:
  DisplayPageShowPrevAction(DisplayBuffer *buffer) : buffer_(buffer) {}

  void play(Ts... x) override { this->buffer_->show_prev_page(); }

  DisplayBuffer *buffer_;
};

template<typename... Ts> class DisplayIsDisplayingPageCondition : public Condition<Ts...> {
 public:
  DisplayIsDisplayingPageCondition(DisplayBuffer *parent) : parent_(parent) {}

  void set_page(DisplayPage *page) { this->page_ = page; }
  bool check(Ts... x) override { return this->parent_->get_active_page() == this->page_; }

 protected:
  DisplayBuffer *parent_;
  DisplayPage *page_;
};

class DisplayOnPageChangeTrigger : public Trigger<DisplayPage *, DisplayPage *> {
 public:
  explicit DisplayOnPageChangeTrigger(DisplayBuffer *parent) { parent->add_on_page_change_trigger(this); }
  void process(DisplayPage *from, DisplayPage *to);
  void set_from(DisplayPage *p) { this->from_ = p; }
  void set_to(DisplayPage *p) { this->to_ = p; }

 protected:
  DisplayPage *from_{nullptr};
  DisplayPage *to_{nullptr};
};

}  // namespace display
}  // namespace esphome
