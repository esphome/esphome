#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "display_color_utils.h"
#include "esphome/core/helpers.h"

#include <cstdarg>

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#ifdef USE_GRAPH
#include "esphome/components/graph/graph.h"
#endif

#ifdef USE_QR_CODE
#include "esphome/components/qr_code/qr_code.h"
#endif

#define POLAR_2PI 6.28318530718F

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

enum ImageType {
  IMAGE_TYPE_BINARY = 0,
  IMAGE_TYPE_GRAYSCALE = 1,
  IMAGE_TYPE_RGB24 = 2,
  IMAGE_TYPE_TRANSPARENT_BINARY = 3,
  IMAGE_TYPE_RGB565 = 4,
};

enum DisplayType {
  DISPLAY_TYPE_BINARY = 1,
  DISPLAY_TYPE_GRAYSCALE = 2,
  DISPLAY_TYPE_COLOR = 3,
};

enum DisplayRotation {
  DISPLAY_ROTATION_0_DEGREES = 0,
  DISPLAY_ROTATION_90_DEGREES = 90,
  DISPLAY_ROTATION_180_DEGREES = 180,
  DISPLAY_ROTATION_270_DEGREES = 270,
};

enum GradientDirection {
  GRADIENT_NONE = 0,
  GRADIENT_HORIZONTAL = 1,
  GRADIENT_VERTICAL = 2,
  GRADIENT_BOTH = 3,
};

struct Rect {
  int16_t x;  ///< X coordinate of corner
  int16_t y;  ///< Y coordinate of corner
  int16_t w;  ///< Width of region
  int16_t h;  ///< Height of region

  inline Rect() ALWAYS_INLINE : x(32766), y(32766), w(32766), h(32766) {}  // NOLINT
  inline Rect(int16_t x, int16_t y, int16_t w, int16_t h) ALWAYS_INLINE : x(x), y(y), w(w), h(h) {}
  inline int16_t x2() { return this->x + this->w;};  ///< X coordinate of corner
  inline int16_t y2() { return this->y + this->h;};;  ///< Y coordinate of corner


  inline bool is_set() ALWAYS_INLINE { return (this->h != 32766) && (this->w != 32766); }

  inline void expand(int16_t width, int16_t height){
    if ((*this).is_set() && ((*this).w >= (-2 * width)) && ( (*this).h >= (-2 * height))) { 
      (*this).x = (*this).x - width;
      (*this).y = (*this).y - height;
      (*this).w = (*this).w + (2 * width);
      (*this).h = (*this).h + (2 * height);
    }
  }

  inline void join(Rect rect) {
    if (!this->is_set()) {
      this->x = rect.x;
      this->y = rect.y;
      this->w = rect.w;
      this->h = rect.h;
    } else {
      if (this->x > rect.x) { this->x = rect.x; }
      if (this->y > rect.y) { this->y = rect.y; }
      if (this->x2() < rect.x2()) { this->w = rect.x2()-this->x; }
      if (this->y2() < rect.y2()) { this->h = rect.y2()-this->y; }
    }
  }
  inline void substract(Rect rect) {
    if (!this->inside(rect)) {
      (*this) = Rect();
    } else {
      if (this->x < rect.x) { this->x = rect.x; }
      if (this->y < rect.y) { this->y = rect.y; }
      if (this->x2() > rect.x2()) { this->w = rect.x2()-this->x; }
      if (this->y2() > rect.y2()) { this->h = rect.y2()-this->y; }
    }
  }

  inline bool inside(int16_t x, int16_t y, bool absolute = false) {
    if (!this->is_set()) return true;
    if (absolute) {
      return ((x >= 0) && (x <= this->w) && (y >= 0) && (y <= this->h));
    } else {
      return ((x >= this->x) && (x <= this->x2()) && (y >= this->y) && (y <= this->y2()));
    }
  }
  inline bool inside(Rect rect, bool absolute = false) {
    if (!this->is_set() || !rect.is_set()) return true;
    if (absolute) {
      return ((rect.x <= this->w) && (rect.w >= 0) && (rect.y <= this->h) && (rect.h >= 0));
    } else {
      ESP_LOGVV("TAG2", "rect inside = %s , %s , %s , %s", YESNO(rect.x <= this->x2()) , YESNO(rect.x2() >= this->x), YESNO(rect.y <= this->y2()) , YESNO(rect.y2() >= this->y));
      return ((rect.x <= this->x2()) && (rect.x2() >= this->x) && (rect.y <= this->y2()) && (rect.y2() >= this->y));
    }
  }

  //  rect           x--------------w
  //  this        x--------------w
  inline void info(std::string prefix = "rect info:") {
    if (this->is_set()) {
      ESP_LOGI("Rect", "%s [%3d,%3d,%3d,%3d]",prefix.c_str(), this->x, this->y, this->w, this->h);
    } else
      ESP_LOGI("Rect", "%s ** NOT SET **",prefix.c_str());
  }

};

class Font;
class Image;
class DisplayBuffer;
class DisplayPage;
class DisplayOnPageChangeTrigger;

using display_writer_t = std::function<void(DisplayBuffer &)>;

#define LOG_DISPLAY(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, prefix type); \
    ESP_LOGCONFIG(TAG, "%s  Rotations: %d °", prefix, (obj)->rotation_); \
    ESP_LOGCONFIG(TAG, "%s  Dimensions: %dpx x %dpx", prefix, (obj)->get_width(), (obj)->get_height()); \
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
  void line(int x1, int y1, int x2, int y2, Color color = COLOR_ON){
    line(x1, y1, x2, y2, color, color, GRADIENT_NONE);
  }
  void line(int x1, int y1, int x2, int y2, Color grandient_from, Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);

  /// Draw a horizontal line from the point [x,y] to [x+width,y] with the given color.
  void horizontal_line(int x, int y, int width, Color color = COLOR_ON) {
    horizontal_line(x, y, width, color, color, GRADIENT_NONE);
  }
  void horizontal_line(int x, int y, int width, Color grandient_from , Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);

  /// Draw a vertical line from the point [x,y] to [x,y+width] with the given color.
  void vertical_line(int x, int y, int height, Color color = COLOR_ON){
    vertical_line(x, y, height, color, color, GRADIENT_NONE);
  }
  /// Draw a vertical line from the point [x,y] to [x,y+width] with the given color.
  void vertical_line(int x, int y, int height, Color grandient_from, Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);

  /// Draw the outline of a rectangle with the top left point at [x1,y1] and the bottom right point at
  /// [x1+width,y1+height].
  void rectangle(int x1, int y1, int width, int height, Color color = COLOR_ON) {
    rectangle(x1, y1, width, height, 0, color, color, GRADIENT_NONE);
  }
  void rectangle(int x, int y, int width, int height, int16_t radius, Color color = COLOR_ON) {
    rectangle(x, y, width, height, radius, color, color, GRADIENT_NONE);
  };
  void rectangle(int x1, int y1, int width, int height, Color grandient_from, Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL){
    rectangle(x1, y1, width, height, 0, grandient_from, grandient_to, direction);
  }
  void rectangle(int x, int y, int width, int height, int16_t radius, Color grandient_from , Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);


  void filled_rectangle(int x1, int y1, int width, int height, Color color = COLOR_ON) {
    filled_rectangle(x1, y1, width, height, 0, color, color, GRADIENT_NONE);
  }
  void filled_rectangle(int x, int y, int width, int height, int16_t radius, Color color = COLOR_ON) {
    filled_rectangle(x, y, width, height, radius, color, color, GRADIENT_NONE);
  };
  void filled_rectangle(int x1, int y1, int width, int height, Color grandient_from, Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL){
    filled_rectangle(x1, y1, width, height, 0, grandient_from, grandient_to, direction);
  }
  void filled_rectangle(int x, int y, int width, int height, int16_t radius, Color grandient_from , Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);


  /// Draw the outline of a circle centered around [center_x,center_y] with the radius radius with the given color.
  void circle(int center_x, int center_xy, int radius, Color color = COLOR_ON){
    circle(center_x, center_xy, radius, color, color, GRADIENT_NONE);
 }
  void circle(int center_x, int center_xy, int radius, Color color, Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);

  /// Fill a circle centered around [center_x,center_y] with the radius radius with the given color.
  void filled_circle(int center_x, int center_y, int radius, Color color = COLOR_ON){
    filled_circle(center_x, center_y, radius, color, color, GRADIENT_NONE);
  }
  void filled_circle(int center_x, int center_y, int radius, Color color, Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);

  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color color = COLOR_ON)
  {
    triangle(x0, y0, x1, y1, x2, y2, color, color, GRADIENT_NONE);
  }
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color color,
                Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);

  // Draw a filled triangle
  void filled_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color color = COLOR_ON){
    filled_triangle(x0, y0, x1, y1, x2, y2, color, color, GRADIENT_NONE);
  }
  void filled_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                      Color color, Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);

 ///
  /// Draw a framed quadrilateral
  ///
  /// \param[in]  points:        Pointer to array of 4 points
  /// \param[in]  color:        Color RGB value for the frame
  ///
  /// \return true if success, false if error
  ///
  void quad(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3,
            Color color = COLOR_ON) {
    quad(x0, y0, x1, y1, x2, y2, x3, y3, color, color, GRADIENT_NONE);
  }

  void quad(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3,
            Color color, Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);


  ///
  /// Draw a filled quadrilateral
  ///
  /// \param[in]  points:        Pointer to array of 4 points
  /// \param[in]  color:        Color RGB value for the frame
  ///
  /// \return true if success, false if error
  ///
  void filled_quad(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3,
                   Color color = COLOR_ON) {
    filled_quad(x0, y0, x1, y1, x2, y2, x3, y3, color, color, GRADIENT_NONE);
  }

  void filled_quad(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3,
                   Color color, Color grandient_to, GradientDirection direction = GRADIENT_HORIZONTAL);

  void filled_arc(int16_t x, int16_t y, int16_t radius1, int16_t radius2,
                  int16_t angle_start, int16_t angle_end, Color color = COLOR_ON, int16_t quality = 255) {
    filled_arc(x, y, radius1, radius2,  angle_start, angle_end, color, color, 0, 0, quality);
  }
  void filled_arc(int16_t x, int16_t y, int16_t radius1, int16_t radius2, int16_t angle_start, int16_t angle_end,
                  Color color, Color grandient_to, int16_t gradient_angle_start, int16_t gradient_angle_range, int16_t quality = 255);

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

#ifdef USE_GRAPH
  /** Draw the `graph` with the top-left corner at [x,y] to the screen.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param graph The graph id to draw
   * @param color_on The color to replace in binary images for the on bits.
   */
  void graph(int x, int y, graph::Graph *graph, Color color_on = COLOR_ON);

  /** Draw the `legend` for graph with the top-left corner at [x,y] to the screen.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param graph The graph id for which the legend applies to
   * @param graph The graph id for which the legend applies to
   * @param graph The graph id for which the legend applies to
   * @param name_font The font used for the trace name
   * @param value_font The font used for the trace value and units
   * @param color_on The color of the border
   */
  void legend(int x, int y, graph::Graph *graph, Color color_on = COLOR_ON);
#endif  // USE_GRAPH

#ifdef USE_QR_CODE
  /** Draw the `qr_code` with the top-left corner at [x,y] to the screen.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param qr_code The qr_code to draw
   * @param color_on The color to replace in binary images for the on bits.
   */
  void qr_code(int x, int y, qr_code::QrCode *qr_code, Color color_on = COLOR_ON, int scale = 1);
#endif

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

  void add_on_page_change_trigger(DisplayOnPageChangeTrigger *t) { this->on_page_change_triggers_.push_back(t); }
 
  // Internal method to set display auto clearing.
  void set_auto_clear(bool auto_clear_enabled) { this->auto_clear_enabled_ = auto_clear_enabled; }

  virtual int get_height_internal() = 0;
  virtual int get_width_internal() = 0;

  /// Internal method to set the display rotation with.
  virtual void set_rotation(DisplayRotation rotation);
  DisplayRotation get_rotation() const { return this->rotation_; }

  /** Get the type of display that the buffer corresponds to. In case of dynamically configurable displays,
   * returns the type the display is currently configured to.
   */
  virtual DisplayType get_display_type() = 0;

  ///
  /// Expand or contract a rectangle in width and/or height (equal
  /// amounts on both side), based on the centerpoint of the rectangle.
  ///
  /// \param[in]  rect:       Rectangular region before resizing
  /// \param[in]  width:    Number of pixels to expand the width (if positive)
  ///                          of contract the width (if negative)
  /// \param[in]  height:    Number of pixels to expand the height (if positive)
  ///                          of contract the height (if negative)
  ///
  /// \return new rect with resized dimensions
  ///

  ///
  /// Set the clipping rectangle for further drawing
  ///
  /// \param[in]  rect:       Pointer to Rect for clipping (or NULL for entire screen)
  ///
  /// \return true if success, false if error
  ///
  void set_clipping(Rect rect);
  void set_clipping(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    set_clipping(Rect(left, top, right, bottom));
  };

  ///
  /// Add a rectangular region to the invalidation region
  /// - This is usually called when an element has been modified
  ///
  /// \param[in]  rect: Rectangle to add to the invalidation region
  ///
  /// \return none
  ///
  void add_clipping(Rect rect);
  void add_clipping(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    this->add_clipping(Rect(left, top, right, bottom));
  };

  ///
  /// substract a rectangular region to the invalidation region
  /// - This is usually called when an element has been modified
  ///
  /// \param[in]  rect: Rectangle to add to the invalidation region
  ///
  /// \return none
  ///
  void sub_clipping(Rect rect);
  void sub_clipping(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom) {
    this->sub_clipping(Rect(left, top, right, bottom));
  };

  ///
  /// Reset the invalidation region
  ///
  /// \return none
  ///
  void clear_clipping();

  ///
  /// Get the current the clipping rectangle
  ///
  ///
  /// \return rect for active clipping region
  ///
  Rect get_clipping();

  ///
  /// Perform basic clipping of a single point to a clipping region
  ///
  /// \param[in]  X:          X coordinate of point
  /// \param[in]  Y:          Y coordinate of point
  ///
  /// \return true if point is visible, false if it should be discarded
  ///
  bool is_clipped(int16_t x, int16_t y);
  bool is_clipped(Rect rect);

  ///
  /// Convert polar coordinate to cartesian
  ///
  /// \param[in]   rad         Radius of ray
  /// \param[in]   angle       Angle of ray (in units of 1/64 degrees, 0 is up)
  /// \param[out]  nDX          X offset for ray end
  /// \param[out]  nDY          Y offset for ray end
  ///
  /// \return none
  ///
  void calc_polar(uint16_t rad, int16_t angle, int16_t *x, int16_t *y);

  ///
  /// Calculate fixed-point sine function from fractional degrees
  /// - Depending on configuration, the result is derived from either
  ///   floating point math library or fixed point lookup table.
  /// - get_sin(nAngDeg*64)/32768.0 = sin(nAngDeg*2pi/360)
  ///
  /// \param[in]   angle       Angle (in units of 1/64 degrees)
  ///
  /// \return Fixed-point sine result. Signed 16-bit; divide by 32768
  ///         to get the actual value.
  ///
  int16_t get_sin(int16_t angle);

  ///
  /// Calculate fixed-point cosine function from fractional degrees
  /// - Depending on configuration, the result is derived from either
  ///   floating point math library or fixed point lookup table.
  /// - get_cos(nAngDeg*64)/32768.0 = cos(nAngDeg*2pi/360)
  ///
  /// \param[in]   angle       Angle (in units of 1/64 degrees)
  ///
  /// \return Fixed-point cosine result. Signed 16-bit; divide by 32768
  ///         to get the actual value.
  ///
  int16_t get_cos(int16_t angle);

  ///
  /// Draw a polar ray segment
  ///
  /// \param[in]  pGui:        Pointer to GUI
  /// \param[in]  x:          X coordinate of line startpoint
  /// \param[in]  y:          Y coordinate of line startpoint
  /// \param[in]  nRadStart:   Starting radius of line
  /// \param[in]  nRadEnd:     Ending radius of line
  /// \param[in]  n64Ang:      Angle of ray (degrees * 64). 0 is up, +90*64 is to right
  ///                          From -180*64 to +180*64
  /// \param[in]  color:        Color RGB value for the line
  ///
  /// \return none
  ///
  void polar_line(int16_t x, int16_t y, uint16_t radius_start, uint16_t radius_end, int16_t angle,
                  Color color = COLOR_ON);

 protected:
  void vprintf_(int x, int y, Font *font, Color color, TextAlign align, const char *format, va_list arg);

  virtual void draw_absolute_pixel_internal(int x, int y, Color color) = 0;

  uint8_t init_internal_(uint32_t buffer_length, uint8_t bytes_per_pixel = 1);
 
  void do_update_();

  void swap_coords_(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1);
  std::vector<Rect> clipping_rectangle_;

  uint8_t *buffer_{nullptr};
  DisplayRotation rotation_{DISPLAY_ROTATION_0_DEGREES};
  optional<display_writer_t> writer_{};
  DisplayPage *page_{nullptr};
  DisplayPage *previous_page_{nullptr};
  std::vector<DisplayOnPageChangeTrigger *> on_page_change_triggers_;
  bool auto_clear_enabled_{true};
};

class DisplayPage {
 public:
  DisplayPage(display_writer_t writer);
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

  bool get_pixel(int x, int y) const;

  const char *get_char() const;

  bool compare_to(const char *str) const;

  int match_length(const char *str) const;

  void scan_area(int *x1, int *y1, int *width, int *height) const;

 protected:
  friend Font;
  friend DisplayBuffer;

  const GlyphData *glyph_data_;
};

class Font {
 public:
  /** Construct the font with the given glyphs.
   *
   * @param glyphs A vector of glyphs, must be sorted lexicographically.
   * @param baseline The y-offset from the top of the text to the baseline.
   * @param bottom The y-offset from the top of the text to the bottom (i.e. height).
   */
  Font(const GlyphData *data, int data_nr, int baseline, int bottom);

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
  virtual Color get_rgb565_pixel(int x, int y) const;
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
  Color get_rgb565_pixel(int x, int y) const override;
  Color get_grayscale_pixel(int x, int y) const override;

  int get_animation_frame_count() const;
  int get_current_frame() const;
  void next_frame();
  void prev_frame();

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
