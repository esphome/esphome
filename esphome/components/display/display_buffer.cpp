#include "display_buffer.h"

#include <utility>
#include "esphome/core/application.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <cmath>

namespace esphome {
namespace display {

static const char *const TAG = "display";

const Color COLOR_OFF(0, 0, 0, 0);
const Color COLOR_ON(255, 255, 255, 255);

uint8_t DisplayBuffer::init_internal_(uint32_t buffer_length, uint8_t bytes_per_pixel) {
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);

  if (bytes_per_pixel == 0)
    bytes_per_pixel = 1;
  if (bytes_per_pixel > 3)
    bytes_per_pixel = 3;

  while (bytes_per_pixel > 0) {
    this->buffer_ = allocator.allocate(buffer_length * bytes_per_pixel);
    if (this->buffer_ != nullptr) {
      return bytes_per_pixel;
    }
  }
  ESP_LOGE(TAG, "Could not allocate buffer for display!");
  return 0;
}

void DisplayBuffer::fill(Color color) { this->filled_rectangle(0, 0, this->get_width(), this->get_height(), color); }
void DisplayBuffer::clear() { this->fill(COLOR_OFF); }
int DisplayBuffer::get_width() {
  switch (this->rotation_) {
    case DISPLAY_ROTATION_90_DEGREES:
    case DISPLAY_ROTATION_270_DEGREES:
      return this->get_height_internal();
    case DISPLAY_ROTATION_0_DEGREES:
    case DISPLAY_ROTATION_180_DEGREES:
    default:
      return this->get_width_internal();
  }
}
int DisplayBuffer::get_height() {
  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
    case DISPLAY_ROTATION_180_DEGREES:
      return this->get_height_internal();
    case DISPLAY_ROTATION_90_DEGREES:
    case DISPLAY_ROTATION_270_DEGREES:
    default:
      return this->get_width_internal();
  }
}
void DisplayBuffer::set_rotation(DisplayRotation rotation) { this->rotation_ = rotation; }
void HOT DisplayBuffer::draw_pixel_at(int x, int y, Color color) {
  if (!this->get_clipping().inside(x, y))
    return;  // NOLINT

  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
      break;
    case DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = this->get_width_internal() - x - 1;
      break;
    case DISPLAY_ROTATION_180_DEGREES:
      x = this->get_width_internal() - x - 1;
      y = this->get_height_internal() - y - 1;
      break;
    case DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = this->get_height_internal() - y - 1;
      break;
  }
  this->draw_absolute_pixel_internal(x, y, color);
  App.feed_wdt();
}

void DisplayBuffer::line(int x1, int y1, int x2, int y2, Color grandient_from, Color grandient_to,
                         GradientDirection direction) {
  const int32_t delta_x = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
  const int32_t delta_y = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
  float gradient_step = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2)) / 255, gradient_pos = 0;
  int32_t err = delta_x + delta_y;
  while (true) {
    this->draw_pixel_at(x1, y1, grandient_from.gradient(grandient_to, (uint16_t) gradient_pos));
    gradient_pos += gradient_step;
    if (x1 == x2 && y1 == y2)
      break;
    int32_t e2 = 2 * err;
    if (e2 >= delta_y) {
      err += delta_y;
      x1 += sx;
    }
    if (e2 <= delta_x) {
      err += delta_x;
      y1 += sy;
    }
  }
}
/// Draw a horizontal line from the point [x,y] to [x+width,y] with the given color.
void HOT DisplayBuffer::horizontal_line(int x, int y, int width, Color grandient_from, Color grandient_to,
                                        GradientDirection direction) {
  // Future: Could be made more efficient by manipulating buffer directly in certain rotations.
  for (int i = x; i < x + width; i++)
    this->draw_pixel_at(i, y, grandient_from);
}
/// Draw a vertical line from the point [x,y] to [x,y+width] with the given color.
void HOT DisplayBuffer::vertical_line(int x, int y, int height, Color grandient_from, Color grandient_to,
                                      GradientDirection direction) {
  // Future: Could be made more efficient by manipulating buffer directly in certain rotations.
  for (int i = y; i < y + height; i++)
    this->draw_pixel_at(x, i, grandient_from);
}

void DisplayBuffer::rectangle(int x, int y, int width, int height, int16_t radius, Color grandient_from,
                              Color grandient_to, GradientDirection direction) {
  int delta_x = -radius;
  int delta_y = 0;
  int err = 2 - 2 * radius;
  int e2;

  x = x + radius;
  y = y + radius;
  height = height - (radius * 2);
  width = width - (radius * 2);
  Color color = grandient_from;
  this->horizontal_line(x, y - radius, width, color);
  this->horizontal_line(x, y + radius + height - 1, width, color);
  this->vertical_line(x - radius, y, height, color);
  this->vertical_line(x + radius + width - 1, y, height, color);
  if (radius > 0) {
    do {
      this->draw_pixel_at(x + delta_x, y - delta_y, color);
      this->draw_pixel_at(x + delta_x, y + height + delta_y - 1, color);
      this->draw_pixel_at(x + width - delta_x - 1, y - delta_y, color);
      this->draw_pixel_at(x + width - delta_x - 1, y + height + delta_y - 1, color);
      e2 = err;
      if (e2 < delta_y) {
        err += ++delta_y * 2 + 1;
        if (-delta_x == delta_y && e2 <= delta_x) {
          e2 = 0;
        }
      }
      if (e2 > delta_x) {
        err += ++delta_x * 2 + 1;
      }
    } while (delta_x <= 0);
  }
}

void DisplayBuffer::filled_rectangle(int x, int y, int width, int height, int16_t radius, Color color,
                                     Color grandient_to, GradientDirection direction) {
  // Future: Use vertical_line and horizontal_line methods depending on rotation to reduce memory accesses.
  int delta_x = -radius;
  int delta_y = 0;
  int err = 2 - 2 * radius;
  int e2;

  x = x + radius;
  y = y + radius;
  height = height - (radius * 2);
  width = width - (radius * 2);
  // this->filled_rectangle(x - radius, y, width + (radius * 2), height, color);

  for (int i = y; i < y + height; i++) {
    this->horizontal_line(x - radius, i, width + (radius * 2), color);
  }
  if (radius > 0) {
    do {
      int hline_width = width + (2 * (-delta_x) + 1) - 1;
      this->horizontal_line(x + delta_x, y + height + delta_y, hline_width, color);
      this->horizontal_line(x + delta_x, y - delta_y, hline_width, color);
      e2 = err;
      if (e2 < delta_y) {
        err += ++delta_y * 2 + 1;
        if (-delta_x == delta_y && e2 <= delta_x) {
          e2 = 0;
        }
      }
      if (e2 > delta_x) {
        err += ++delta_x * 2 + 1;
      }
    } while (delta_x <= 0);
  }
}

void HOT DisplayBuffer::circle(int center_x, int center_xy, int radius, Color color, Color grandient_to,
                               GradientDirection direction) {
  int delta_x = -radius;
  int delta_y = 0;
  int err = 2 - 2 * radius;
  int e2;

  do {
    this->draw_pixel_at(center_x - delta_x, center_xy + delta_y, color);
    this->draw_pixel_at(center_x + delta_x, center_xy + delta_y, color);
    this->draw_pixel_at(center_x + delta_x, center_xy - delta_y, color);
    this->draw_pixel_at(center_x - delta_x, center_xy - delta_y, color);
    e2 = err;
    if (e2 < delta_y) {
      err += ++delta_y * 2 + 1;
      if (-delta_x == delta_y && e2 <= delta_x) {
        e2 = 0;
      }
    }
    if (e2 > delta_x) {
      err += ++delta_x * 2 + 1;
    }
  } while (delta_x <= 0);
}
void DisplayBuffer::filled_circle(int center_x, int center_y, int radius, Color color, Color grandient_to,
                                  GradientDirection direction) {
  int delta_x = -int32_t(radius);
  int delta_y = 0;
  int err = 2 - 2 * radius;
  int e2;

  do {
    int hline_width = 2 * (-delta_x) + 1;
    this->horizontal_line(center_x + delta_x, center_y + delta_y, hline_width, color);
    this->horizontal_line(center_x + delta_x, center_y - delta_y, hline_width, color);
    e2 = err;
    if (e2 < delta_y) {
      err += ++delta_y * 2 + 1;
      if (-delta_x == delta_y && e2 <= delta_x) {
        e2 = 0;
      }
    }
    if (e2 > delta_x) {
      err += ++delta_x * 2 + 1;
    }
  } while (delta_x <= 0);
}

// Draw a triangle
void DisplayBuffer::triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color color,
                             Color grandient_to, GradientDirection direction) {
  // Draw triangle with three lines
  this->line(x0, y0, x1, y1, color);
  this->line(x1, y1, x2, y2, color);
  this->line(x2, y2, x0, y0, color);
}

// Draw a filled triangle
void DisplayBuffer::filled_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, Color color,
                                    Color grandient_to, GradientDirection direction) {
  // Emulate triangle fill

  // Algorithm:
  // - An arbitrary triangle is cut into two portions:
  //   1) a flat bottom triangle
  //   2) a flat top triangle
  // - Sort the vertices in descending vertical position.
  //   This serves two purposes:
  //   1) ensures that the division between the flat bottom
  //      and flat top triangles occurs at Y=Y1
  //   2) ensure that we avoid division-by-zero in the for loops
  // - Walk each scan line and determine the substraction
  //   between triangle side A & B (flat bottom triangle)
  //   and then C and B (flat top triangle) using line slopes.

  // Sort vertices
  // - Want y0 >= y1 >= y2
  if (y2 > y1) {
    this->swap_coords_(&x2, &y2, &x1, &y1);
  }
  if (y1 > y0) {
    this->swap_coords_(&x0, &y0, &x1, &y1);
  }
  if (y2 > y1) {
    this->swap_coords_(&x2, &y2, &x1, &y1);
  }

  // TODO: It is more efficient to calculate row endpoints
  // using incremental additions instead of multiplies/divides

  int16_t xa, xb, xc, yos;
  int16_t x01 = x0 - x1, y01 = y0 - y1, x20 = x2 - x0, y20 = y2 - y0, x21 = x2 - x1, y21 = y2 - y1;

  // Flat bottom scenario
  // NOTE: Due to vertex sorting and loop range, it shouldn't
  // be possible to enter loop when y0 == y1 or y2
  for (yos = 0; yos < y01; yos++) {
    // Determine row endpoints (no rounding)
    // xa = (yos          )*(x0-x1)/(y0-y1);
    // xb = (yos-(y0-y1))*(x2-x0)/(y2-y0);

    // Determine row endpoints (using rounding)
    xa = 2 * yos * x01;
    xa += (xa >= 0) ? abs(y01) : -abs(y01);
    xa /= 2 * y01;

    xb = 2 * (yos - y01) * x20;
    xb += (xb >= 0) ? abs(y20) : -abs(y20);
    xb /= 2 * y20;

    // Draw horizontal line between endpoints
    this->line(x1 + xa, y1 + yos, x0 + xb, y1 + yos, color);
  }

  // Flat top scenario
  // NOTE: Due to vertex sorting and loop range, it shouldn't
  // be possible to enter loop when y2 == y0 or y1
  for (yos = y21; yos < 0; yos++) {
    // Determine row endpoints (no rounding)
    // xc = (yos          )*(x2-x1)/(y2-y1);
    // xb = (yos-(y0-y1))*(x2-x0)/(y2-y0);

    // Determine row endpoints (using rounding)
    xc = 2 * yos * x21;
    xc += (xc >= 0) ? abs(y21) : -abs(y21);
    xc /= 2 * y21;

    xb = 2 * (yos - y01) * x20;
    xb += (xb >= 0) ? abs(y20) : -abs(y20);
    xb /= 2 * y20;

    // Draw horizontal line between endpoints
    this->line(x1 + xc, y1 + yos, x0 + xb, y1 + yos, color);
  }
}

void DisplayBuffer::quad(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3,
                         Color color, Color grandient_to, GradientDirection direction) {
  this->line(x0, y0, x1, y1, color);
  this->line(x1, y1, x2, y2, color);
  this->line(x2, y2, x3, y3, color);
  this->line(x3, y3, x0, y0, color);
}

// Filling a quadrilateral is done by breaking it down into
// two filled triangles sharing one side. We have to be careful
// about the triangle fill routine (ie. using rounding) so that
// we can avoid leaving a thin seam between the two triangles.
void DisplayBuffer::filled_quad(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3,
                                int16_t y3, Color color, Color grandient_to, GradientDirection direction) {
  this->filled_triangle(x0, y0, x1, y1, x2, y2, color, grandient_to, direction);
  this->filled_triangle(x2, y2, x0, y0, x3, y3, color, grandient_to, direction);
}

void DisplayBuffer::filled_arc(int16_t x, int16_t y, int16_t radius1, int16_t radius2, int16_t angle_start,
                               int16_t angle_end, Color color, Color grandient_to, int16_t gradient_angle_start,
                               int16_t gradient_angle_range, int16_t quality) {
  int16_t x0, y0, x1, y1, x2, y2, x3, y3;
  int16_t gradient_pos = 0;

  // Calculate degrees per step (based on quality setting)
  int16_t step_angle = 360 / quality;
  int16_t step = 64 * step_angle;

  int16_t angle;
  int16_t calc_x, calc_y;
  int16_t segment_start, segment_end;

  segment_start = angle_start * (int32_t) quality / 360;
  segment_end = angle_end * (int32_t) quality / 360;

  int16_t segment_gradient_start, segment_gradient_range;
  segment_gradient_start = gradient_angle_start * (int32_t) quality / 360;
  segment_gradient_range = gradient_angle_range * (int32_t) quality / 360;
  segment_gradient_range = (segment_gradient_range == 0) ? 1 : segment_gradient_range;  // Guard against div/0

  bool clockwise;
  int16_t segment_index;
  int16_t step_count;
  if (segment_end >= segment_start) {
    step_count = segment_end - segment_start;
    clockwise = true;
  } else {
    step_count = segment_start - segment_end;
    clockwise = false;
  }

  for (int16_t step_index = 0; step_index < step_count; step_index++) {
    // Remap from the step to the segment index, depending on direction
    segment_index = (clockwise) ? (segment_start + step_index) : (segment_start - step_index - 1);

    angle = (int32_t) (segment_index * step) % (int32_t) (23040);

    this->calc_polar(radius1, angle, &calc_x, &calc_y);
    x0 = x + calc_x;
    y0 = y + calc_y;

    this->calc_polar(radius2, angle, &calc_x, &calc_y);
    x1 = x + calc_x;
    y1 = y + calc_y;

    this->calc_polar(radius2, angle + step, &calc_x, &calc_y);
    x2 = x + calc_x;
    y2 = y + calc_y;

    this->calc_polar(radius1, angle + step, &calc_x, &calc_y);
    x3 = x + calc_x;
    y3 = y + calc_y;

    if (grandient_to == color) {
      // Gradient coloring
      gradient_pos = 255 * (int32_t) (segment_index - segment_gradient_start) / segment_gradient_range;
    }
    this->filled_quad(x0, y0, x1, y1, x2, y2, x3, y3, color.gradient(grandient_to, gradient_pos));
  }
}

void DisplayBuffer::print(int x, int y, Font *font, Color color, TextAlign align, const char *text) {
  int x_start, y_start;
  int width, height;
  this->get_text_bounds(x, y, text, font, align, &x_start, &y_start, &width, &height);

  int i = 0;
  int x_at = x_start;
  while (text[i] != '\0') {
    int match_length;
    int glyph_n = font->match_next_glyph(text + i, &match_length);
    if (glyph_n < 0) {
      // Unknown char, skip
      ESP_LOGW(TAG, "Encountered character without representation in font: '%c'", text[i]);
      if (!font->get_glyphs().empty()) {
        uint8_t glyph_width = font->get_glyphs()[0].glyph_data_->width;
        for (int glyph_x = 0; glyph_x < glyph_width; glyph_x++) {
          for (int glyph_y = 0; glyph_y < height; glyph_y++)
            this->draw_pixel_at(glyph_x + x_at, glyph_y + y_start, color);
        }
        x_at += glyph_width;
      }

      i++;
      continue;
    }

    const Glyph &glyph = font->get_glyphs()[glyph_n];
    int scan_x1, scan_y1, scan_width, scan_height;
    glyph.scan_area(&scan_x1, &scan_y1, &scan_width, &scan_height);

    for (int glyph_x = scan_x1; glyph_x < scan_x1 + scan_width; glyph_x++) {
      for (int glyph_y = scan_y1; glyph_y < scan_y1 + scan_height; glyph_y++) {
        if (glyph.get_pixel(glyph_x, glyph_y)) {
          this->draw_pixel_at(glyph_x + x_at, glyph_y + y_start, color);
        }
      }
    }

    x_at += glyph.glyph_data_->width + glyph.glyph_data_->offset_x;

    i += match_length;
  }
}
void DisplayBuffer::vprintf_(int x, int y, Font *font, Color color, TextAlign align, const char *format, va_list arg) {
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  if (ret > 0)
    this->print(x, y, font, color, align, buffer);
}

void DisplayBuffer::image(int x, int y, Image *image, Color color_on, Color color_off) {
  switch (image->get_type()) {
    case IMAGE_TYPE_BINARY:
      for (int img_x = 0; img_x < image->get_width(); img_x++) {
        for (int img_y = 0; img_y < image->get_height(); img_y++) {
          this->draw_pixel_at(x + img_x, y + img_y, image->get_pixel(img_x, img_y) ? color_on : color_off);
        }
      }
      break;
    case IMAGE_TYPE_GRAYSCALE:
      for (int img_x = 0; img_x < image->get_width(); img_x++) {
        for (int img_y = 0; img_y < image->get_height(); img_y++) {
          this->draw_pixel_at(x + img_x, y + img_y, image->get_grayscale_pixel(img_x, img_y));
        }
      }
      break;
    case IMAGE_TYPE_RGB24:
      for (int img_x = 0; img_x < image->get_width(); img_x++) {
        for (int img_y = 0; img_y < image->get_height(); img_y++) {
          this->draw_pixel_at(x + img_x, y + img_y, image->get_color_pixel(img_x, img_y));
        }
      }
      break;
    case IMAGE_TYPE_TRANSPARENT_BINARY:
      for (int img_x = 0; img_x < image->get_width(); img_x++) {
        for (int img_y = 0; img_y < image->get_height(); img_y++) {
          if (image->get_pixel(img_x, img_y))
            this->draw_pixel_at(x + img_x, y + img_y, color_on);
        }
      }
      break;
    case IMAGE_TYPE_RGB565:
      for (int img_x = 0; img_x < image->get_width(); img_x++) {
        for (int img_y = 0; img_y < image->get_height(); img_y++) {
          this->draw_pixel_at(x + img_x, y + img_y, image->get_rgb565_pixel(img_x, img_y));
        }
      }
      break;
  }
}

#ifdef USE_GRAPH
void DisplayBuffer::graph(int x, int y, graph::Graph *graph, Color color_on) { graph->draw(this, x, y, color_on); }
void DisplayBuffer::legend(int x, int y, graph::Graph *graph, Color color_on) {
  graph->draw_legend(this, x, y, color_on);
}
#endif  // USE_GRAPH

#ifdef USE_QR_CODE
void DisplayBuffer::qr_code(int x, int y, qr_code::QrCode *qr_code, Color color_on, int scale) {
  qr_code->draw(this, x, y, color_on, scale);
}
#endif  // USE_QR_CODE

void DisplayBuffer::get_text_bounds(int x, int y, const char *text, Font *font, TextAlign align, int *x1, int *y1,
                                    int *width, int *height) {
  int x_offset, baseline;
  font->measure(text, width, &x_offset, &baseline, height);

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
void DisplayBuffer::print(int x, int y, Font *font, Color color, const char *text) {
  this->print(x, y, font, color, TextAlign::TOP_LEFT, text);
}
void DisplayBuffer::print(int x, int y, Font *font, TextAlign align, const char *text) {
  this->print(x, y, font, COLOR_ON, align, text);
}
void DisplayBuffer::print(int x, int y, Font *font, const char *text) {
  this->print(x, y, font, COLOR_ON, TextAlign::TOP_LEFT, text);
}
void DisplayBuffer::printf(int x, int y, Font *font, Color color, TextAlign align, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, color, align, format, arg);
  va_end(arg);
}
void DisplayBuffer::printf(int x, int y, Font *font, Color color, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, color, TextAlign::TOP_LEFT, format, arg);
  va_end(arg);
}
void DisplayBuffer::printf(int x, int y, Font *font, TextAlign align, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, COLOR_ON, align, format, arg);
  va_end(arg);
}
void DisplayBuffer::printf(int x, int y, Font *font, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, COLOR_ON, TextAlign::TOP_LEFT, format, arg);
  va_end(arg);
}
void DisplayBuffer::set_writer(display_writer_t &&writer) { this->writer_ = writer; }
void DisplayBuffer::set_pages(std::vector<DisplayPage *> pages) {
  for (auto *page : pages)
    page->set_parent(this);

  for (uint32_t i = 0; i < pages.size() - 1; i++) {
    pages[i]->set_next(pages[i + 1]);
    pages[i + 1]->set_prev(pages[i]);
  }
  pages[0]->set_prev(pages[pages.size() - 1]);
  pages[pages.size() - 1]->set_next(pages[0]);
  this->show_page(pages[0]);
}
void DisplayBuffer::show_page(DisplayPage *page) {
  this->previous_page_ = this->page_;
  this->page_ = page;
  if (this->previous_page_ != this->page_) {
    for (auto *t : on_page_change_triggers_)
      t->process(this->previous_page_, this->page_);
  }
}
void DisplayBuffer::show_next_page() { this->page_->show_next(); }
void DisplayBuffer::show_prev_page() { this->page_->show_prev(); }

void DisplayBuffer::call_update() { this->do_update_(); }

void DisplayBuffer::do_update_() {
  static bool prossing_update = false, need_update = false;
  if (prossing_update) {
    need_update = true;
    return;
  }
  do {
    prossing_update = true;
    need_update = false;
    if (this->auto_clear_enabled_) {
      this->clear();
    }
    if (this->page_ != nullptr) {
      this->page_->get_writer()(*this);
    } else if (this->writer_.has_value()) {
      (*this->writer_)(*this);
    }
    if (!need_update)
      this->display_();
  } while (need_update);
  prossing_update = false;
}

void DisplayOnPageChangeTrigger::process(DisplayPage *from, DisplayPage *to) {
  if ((this->from_ == nullptr || this->from_ == from) && (this->to_ == nullptr || this->to_ == to))
    this->trigger(from, to);
}
#ifdef USE_TIME
void DisplayBuffer::strftime(int x, int y, Font *font, Color color, TextAlign align, const char *format,
                             time::ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    this->print(x, y, font, color, align, buffer);
}
void DisplayBuffer::strftime(int x, int y, Font *font, Color color, const char *format, time::ESPTime time) {
  this->strftime(x, y, font, color, TextAlign::TOP_LEFT, format, time);
}
void DisplayBuffer::strftime(int x, int y, Font *font, TextAlign align, const char *format, time::ESPTime time) {
  this->strftime(x, y, font, COLOR_ON, align, format, time);
}
void DisplayBuffer::strftime(int x, int y, Font *font, const char *format, time::ESPTime time) {
  this->strftime(x, y, font, COLOR_ON, TextAlign::TOP_LEFT, format, time);
}
#endif

void DisplayBuffer::push_clipping(Rect rect) {
  // ESP_LOGW(TAG, "set: Push new clipping");
  if (!this->clipping_rectangle_.empty()) {
    Rect r = this->clipping_rectangle_.back();
    rect.substract(r);
  }
  this->clipping_rectangle_.push_back(rect);
}
void DisplayBuffer::pop_clipping() {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "clear: Clipping is not set.");
  } else {
    this->clipping_rectangle_.pop_back();
  }
}
void DisplayBuffer::add_clipping(Rect add_rect) {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "add: Clipping is not set.");
  } else {
    this->clipping_rectangle_.back().join(add_rect);
  }
}
void DisplayBuffer::substract_clipping(Rect add_rect) {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "add: Clipping is not set.");
  } else {
    // ESP_LOGW(TAG, "add: join new clipping");
    this->clipping_rectangle_.back().substract(add_rect);
  }
}
Rect DisplayBuffer::get_clipping() {
  if (this->clipping_rectangle_.empty()) {
    return Rect();
  } else {
    return this->clipping_rectangle_.back();
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// Graphics General Functions
// ------------------------------------------------------------------------

// Sine function with optional lookup table
// - Note that the angle range is limited by 16-bit integers
//   to an effective degree range of -511 to +511 degrees,
//   defined by the max integer range: 32767/64.
int16_t DisplayBuffer::get_sin(int16_t angle) {
  // Use floating-point math library function
  // Calculate angle in radians
  float angle_rad = angle * POLAR_2PI / (23040.0);  // = 360.0 x 64.0
  // Perform floating point calc
  float sin = std::sin(angle_rad);
  // Return as fixed point result
  return sin * 32767.0;
}

// Cosine function with optional lookup table
// - Note that the angle range is limited by 16-bit integers
//   to an effective degree range of -511 to +511 degrees,
//   defined by the max integer range: 32767/64.
int16_t DisplayBuffer::get_cos(int16_t angle) {
  // Use floating-point math library function
  // Calculate angle in radians
  float angle_rad = angle * POLAR_2PI / (23040.0);  // = 360.0 x 64.0
  // Return as fixed point result
  float cos = std::cos(angle_rad);

  return cos * 32767.0;
}

// Convert from polar to cartesian
void DisplayBuffer::calc_polar(uint16_t radius, int16_t angle, int16_t *x, int16_t *y) {
  int32_t temp;
  // TODO: Clean up excess integer typecasting
  temp = (int32_t) radius * this->get_sin(angle);
  *x = temp / 32767;
  temp = (int32_t) radius * -this->get_cos(angle);
  *y = temp / 32767;
}

// Note that angle is in degrees * 64
void DisplayBuffer::polar_line(int16_t x, int16_t y, uint16_t radius_start, uint16_t radius_end, int16_t angle,
                               Color color) {
  // Draw the ray representing the current value
  int16_t delta_x_start = (int32_t) radius_start * get_sin(angle) / 32768;
  int16_t delta_y_start = (int32_t) radius_start * get_cos(angle) / 32768;
  int16_t delta_x_end = (int32_t) radius_end * get_sin(angle) / 32768;
  int16_t delta_y_end = (int32_t) radius_end * get_cos(angle) / 32768;
  this->line(x + delta_x_start, y - delta_y_start, x + delta_x_end, y - delta_y_end, color);
}

void DisplayBuffer::swap_coords_(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1) {
  int16_t swap_x, swap_y;
  swap_x = *x0;
  swap_y = *y0;
  *x0 = *x1;
  *y0 = *y1;
  *x1 = swap_x;
  *y1 = swap_y;
}

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
const std::vector<Glyph> &Font::get_glyphs() const { return this->glyphs_; }
Font::Font(const GlyphData *data, int data_nr, int baseline, int height) : baseline_(baseline), height_(height) {
  for (int i = 0; i < data_nr; ++i)
    glyphs_.emplace_back(data + i);
}

bool Image::get_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return false;
  const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
  const uint32_t pos = x + y * width_8;
  return progmem_read_byte(this->data_start_ + (pos / 8u)) & (0x80 >> (pos % 8u));
}
Color Image::get_color_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return Color::BLACK;
  const uint32_t pos = (x + y * this->width_) * 3;
  const uint32_t color32 = (progmem_read_byte(this->data_start_ + pos + 2) << 0) |
                           (progmem_read_byte(this->data_start_ + pos + 1) << 8) |
                           (progmem_read_byte(this->data_start_ + pos + 0) << 16);
  return Color(color32);
}
Color Image::get_rgb565_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return Color::BLACK;
  const uint32_t pos = (x + y * this->width_) * 2;
  uint16_t rgb565 =
      progmem_read_byte(this->data_start_ + pos + 0) << 8 | progmem_read_byte(this->data_start_ + pos + 1);
  auto r = (rgb565 & 0xF800) >> 11;
  auto g = (rgb565 & 0x07E0) >> 5;
  auto b = rgb565 & 0x001F;
  return Color((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
}
Color Image::get_grayscale_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return Color::BLACK;
  const uint32_t pos = (x + y * this->width_);
  const uint8_t gray = progmem_read_byte(this->data_start_ + pos);
  return Color(gray | gray << 8 | gray << 16 | gray << 24);
}
int Image::get_width() const { return this->width_; }
int Image::get_height() const { return this->height_; }
ImageType Image::get_type() const { return this->type_; }
Image::Image(const uint8_t *data_start, int width, int height, ImageType type)
    : width_(width), height_(height), type_(type), data_start_(data_start) {}

bool Animation::get_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return false;
  const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
  const uint32_t frame_index = this->height_ * width_8 * this->current_frame_;
  if (frame_index >= (uint32_t) (this->width_ * this->height_ * this->animation_frame_count_))
    return false;
  const uint32_t pos = x + y * width_8 + frame_index;
  return progmem_read_byte(this->data_start_ + (pos / 8u)) & (0x80 >> (pos % 8u));
}
Color Animation::get_color_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return Color::BLACK;
  const uint32_t frame_index = this->width_ * this->height_ * this->current_frame_;
  if (frame_index >= (uint32_t) (this->width_ * this->height_ * this->animation_frame_count_))
    return Color::BLACK;
  const uint32_t pos = (x + y * this->width_ + frame_index) * 3;
  const uint32_t color32 = (progmem_read_byte(this->data_start_ + pos + 2) << 0) |
                           (progmem_read_byte(this->data_start_ + pos + 1) << 8) |
                           (progmem_read_byte(this->data_start_ + pos + 0) << 16);
  return Color(color32);
}
Color Animation::get_rgb565_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return Color::BLACK;
  const uint32_t frame_index = this->width_ * this->height_ * this->current_frame_;
  if (frame_index >= (uint32_t) (this->width_ * this->height_ * this->animation_frame_count_))
    return Color::BLACK;
  const uint32_t pos = (x + y * this->width_ + frame_index) * 2;
  uint16_t rgb565 =
      progmem_read_byte(this->data_start_ + pos + 0) << 8 | progmem_read_byte(this->data_start_ + pos + 1);
  auto r = (rgb565 & 0xF800) >> 11;
  auto g = (rgb565 & 0x07E0) >> 5;
  auto b = rgb565 & 0x001F;
  return Color((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
}
Color Animation::get_grayscale_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return Color::BLACK;
  const uint32_t frame_index = this->width_ * this->height_ * this->current_frame_;
  if (frame_index >= (uint32_t) (this->width_ * this->height_ * this->animation_frame_count_))
    return Color::BLACK;
  const uint32_t pos = (x + y * this->width_ + frame_index);
  const uint8_t gray = progmem_read_byte(this->data_start_ + pos);
  return Color(gray | gray << 8 | gray << 16 | gray << 24);
}
Animation::Animation(const uint8_t *data_start, int width, int height, uint32_t animation_frame_count, ImageType type)
    : Image(data_start, width, height, type), current_frame_(0), animation_frame_count_(animation_frame_count) {}
int Animation::get_animation_frame_count() const { return this->animation_frame_count_; }
int Animation::get_current_frame() const { return this->current_frame_; }
void Animation::next_frame() {
  this->current_frame_++;
  if (this->current_frame_ >= animation_frame_count_) {
    this->current_frame_ = 0;
  }
}
void Animation::prev_frame() {
  this->current_frame_--;
  if (this->current_frame_ < 0) {
    this->current_frame_ = this->animation_frame_count_ - 1;
  }
}

DisplayPage::DisplayPage(display_writer_t writer) : writer_(std::move(writer)) {}
void DisplayPage::show() { this->parent_->show_page(this); }
void DisplayPage::show_next() { this->next_->show(); }
void DisplayPage::show_prev() { this->prev_->show(); }
void DisplayPage::set_parent(DisplayBuffer *parent) { this->parent_ = parent; }
void DisplayPage::set_prev(DisplayPage *prev) { this->prev_ = prev; }
void DisplayPage::set_next(DisplayPage *next) { this->next_ = next; }
const display_writer_t &DisplayPage::get_writer() const { return this->writer_; }

}  // namespace display
}  // namespace esphome
