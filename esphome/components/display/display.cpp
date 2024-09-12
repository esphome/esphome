#include "display.h"
#include "display_color_utils.h"
#include <utility>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace display {

static const char *const TAG = "display";

const Color COLOR_OFF(0, 0, 0, 0);
const Color COLOR_ON(255, 255, 255, 255);

void Display::fill(Color color) { this->filled_rectangle(0, 0, this->get_width(), this->get_height(), color); }
void Display::clear() { this->fill(COLOR_OFF); }
void Display::set_rotation(DisplayRotation rotation) { this->rotation_ = rotation; }
void HOT Display::line(int x1, int y1, int x2, int y2, Color color) {
  const int32_t dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
  const int32_t dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
  int32_t err = dx + dy;

  while (true) {
    this->draw_pixel_at(x1, y1, color);
    if (x1 == x2 && y1 == y2)
      break;
    int32_t e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x1 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y1 += sy;
    }
  }
}

void Display::line_at_angle(int x, int y, int angle, int length, Color color) {
  this->line_at_angle(x, y, angle, 0, length, color);
}

void Display::line_at_angle(int x, int y, int angle, int start_radius, int stop_radius, Color color) {
  // Calculate start and end points
  int x1 = (start_radius * cos(angle * M_PI / 180)) + x;
  int y1 = (start_radius * sin(angle * M_PI / 180)) + y;
  int x2 = (stop_radius * cos(angle * M_PI / 180)) + x;
  int y2 = (stop_radius * sin(angle * M_PI / 180)) + y;

  // Draw line
  this->line(x1, y1, x2, y2, color);
}

void Display::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                             ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  size_t line_stride = x_offset + w + x_pad;  // length of each source line in pixels
  uint32_t color_value;
  for (int y = 0; y != h; y++) {
    size_t source_idx = (y_offset + y) * line_stride + x_offset;
    size_t source_idx_mod;
    for (int x = 0; x != w; x++, source_idx++) {
      switch (bitness) {
        default:
          color_value = ptr[source_idx];
          break;
        case COLOR_BITNESS_565:
          source_idx_mod = source_idx * 2;
          if (big_endian) {
            color_value = (ptr[source_idx_mod] << 8) + ptr[source_idx_mod + 1];
          } else {
            color_value = ptr[source_idx_mod] + (ptr[source_idx_mod + 1] << 8);
          }
          break;
        case COLOR_BITNESS_888:
          source_idx_mod = source_idx * 3;
          if (big_endian) {
            color_value = (ptr[source_idx_mod + 0] << 16) + (ptr[source_idx_mod + 1] << 8) + ptr[source_idx_mod + 2];
          } else {
            color_value = ptr[source_idx_mod + 0] + (ptr[source_idx_mod + 1] << 8) + (ptr[source_idx_mod + 2] << 16);
          }
          break;
      }
      this->draw_pixel_at(x + x_start, y + y_start, ColorUtil::to_color(color_value, order, bitness));
    }
  }
}

void HOT Display::horizontal_line(int x, int y, int width, Color color) {
  // Future: Could be made more efficient by manipulating buffer directly in certain rotations.
  for (int i = x; i < x + width; i++)
    this->draw_pixel_at(i, y, color);
}
void HOT Display::vertical_line(int x, int y, int height, Color color) {
  // Future: Could be made more efficient by manipulating buffer directly in certain rotations.
  for (int i = y; i < y + height; i++)
    this->draw_pixel_at(x, i, color);
}
void Display::rectangle(int x1, int y1, int width, int height, Color color) {
  this->horizontal_line(x1, y1, width, color);
  this->horizontal_line(x1, y1 + height - 1, width, color);
  this->vertical_line(x1, y1, height, color);
  this->vertical_line(x1 + width - 1, y1, height, color);
}
void Display::filled_rectangle(int x1, int y1, int width, int height, Color color) {
  // Future: Use vertical_line and horizontal_line methods depending on rotation to reduce memory accesses.
  for (int i = y1; i < y1 + height; i++) {
    this->horizontal_line(x1, i, width, color);
  }
}
void HOT Display::circle(int center_x, int center_xy, int radius, Color color) {
  int dx = -radius;
  int dy = 0;
  int err = 2 - 2 * radius;
  int e2;

  do {
    this->draw_pixel_at(center_x - dx, center_xy + dy, color);
    this->draw_pixel_at(center_x + dx, center_xy + dy, color);
    this->draw_pixel_at(center_x + dx, center_xy - dy, color);
    this->draw_pixel_at(center_x - dx, center_xy - dy, color);
    e2 = err;
    if (e2 < dy) {
      err += ++dy * 2 + 1;
      if (-dx == dy && e2 <= dx) {
        e2 = 0;
      }
    }
    if (e2 > dx) {
      err += ++dx * 2 + 1;
    }
  } while (dx <= 0);
}
void Display::filled_circle(int center_x, int center_y, int radius, Color color) {
  int dx = -int32_t(radius);
  int dy = 0;
  int err = 2 - 2 * radius;
  int e2;

  do {
    this->draw_pixel_at(center_x - dx, center_y + dy, color);
    this->draw_pixel_at(center_x + dx, center_y + dy, color);
    this->draw_pixel_at(center_x + dx, center_y - dy, color);
    this->draw_pixel_at(center_x - dx, center_y - dy, color);
    int hline_width = 2 * (-dx) + 1;
    this->horizontal_line(center_x + dx, center_y + dy, hline_width, color);
    this->horizontal_line(center_x + dx, center_y - dy, hline_width, color);
    e2 = err;
    if (e2 < dy) {
      err += ++dy * 2 + 1;
      if (-dx == dy && e2 <= dx) {
        e2 = 0;
      }
    }
    if (e2 > dx) {
      err += ++dx * 2 + 1;
    }
  } while (dx <= 0);
}
void HOT Display::triangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color) {
  this->line(x1, y1, x2, y2, color);
  this->line(x1, y1, x3, y3, color);
  this->line(x2, y2, x3, y3, color);
}
void Display::sort_triangle_points_by_y_(int *x1, int *y1, int *x2, int *y2, int *x3, int *y3) {
  if (*y1 > *y2) {
    int x_temp = *x1, y_temp = *y1;
    *x1 = *x2, *y1 = *y2;
    *x2 = x_temp, *y2 = y_temp;
  }
  if (*y1 > *y3) {
    int x_temp = *x1, y_temp = *y1;
    *x1 = *x3, *y1 = *y3;
    *x3 = x_temp, *y3 = y_temp;
  }
  if (*y2 > *y3) {
    int x_temp = *x2, y_temp = *y2;
    *x2 = *x3, *y2 = *y3;
    *x3 = x_temp, *y3 = y_temp;
  }
}
void Display::filled_flat_side_triangle_(int x1, int y1, int x2, int y2, int x3, int y3, Color color) {
  // y2 must be equal to y3 (same horizontal line)

  // Initialize Bresenham's algorithm for side 1
  int s1_current_x = x1;
  int s1_current_y = y1;
  bool s1_axis_swap = false;
  int s1_dx = abs(x2 - x1);
  int s1_dy = abs(y2 - y1);
  int s1_sign_x = ((x2 - x1) >= 0) ? 1 : -1;
  int s1_sign_y = ((y2 - y1) >= 0) ? 1 : -1;
  if (s1_dy > s1_dx) {  // swap values
    int tmp = s1_dx;
    s1_dx = s1_dy;
    s1_dy = tmp;
    s1_axis_swap = true;
  }
  int s1_error = 2 * s1_dy - s1_dx;

  // Initialize Bresenham's algorithm for side 2
  int s2_current_x = x1;
  int s2_current_y = y1;
  bool s2_axis_swap = false;
  int s2_dx = abs(x3 - x1);
  int s2_dy = abs(y3 - y1);
  int s2_sign_x = ((x3 - x1) >= 0) ? 1 : -1;
  int s2_sign_y = ((y3 - y1) >= 0) ? 1 : -1;
  if (s2_dy > s2_dx) {  // swap values
    int tmp = s2_dx;
    s2_dx = s2_dy;
    s2_dy = tmp;
    s2_axis_swap = true;
  }
  int s2_error = 2 * s2_dy - s2_dx;

  // Iterate on side 1 and allow side 2 to be processed to match the advance of the y-axis.
  for (int i = 0; i <= s1_dx; i++) {
    if (s1_current_x <= s2_current_x) {
      this->horizontal_line(s1_current_x, s1_current_y, s2_current_x - s1_current_x + 1, color);
    } else {
      this->horizontal_line(s2_current_x, s2_current_y, s1_current_x - s2_current_x + 1, color);
    }

    // Bresenham's #1
    // Side 1 s1_current_x and s1_current_y calculation
    while (s1_error >= 0) {
      if (s1_axis_swap) {
        s1_current_x += s1_sign_x;
      } else {
        s1_current_y += s1_sign_y;
      }
      s1_error = s1_error - 2 * s1_dx;
    }
    if (s1_axis_swap) {
      s1_current_y += s1_sign_y;
    } else {
      s1_current_x += s1_sign_x;
    }
    s1_error = s1_error + 2 * s1_dy;

    // Bresenham's #2
    // Side 2 s2_current_x and s2_current_y calculation
    while (s2_current_y != s1_current_y) {
      while (s2_error >= 0) {
        if (s2_axis_swap) {
          s2_current_x += s2_sign_x;
        } else {
          s2_current_y += s2_sign_y;
        }
        s2_error = s2_error - 2 * s2_dx;
      }
      if (s2_axis_swap) {
        s2_current_y += s2_sign_y;
      } else {
        s2_current_x += s2_sign_x;
      }
      s2_error = s2_error + 2 * s2_dy;
    }
  }
}
void Display::filled_triangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color) {
  // Sort the three points by y-coordinate ascending, so [x1,y1] is the topmost point
  this->sort_triangle_points_by_y_(&x1, &y1, &x2, &y2, &x3, &y3);

  if (y2 == y3) {  // Check for special case of a bottom-flat triangle
    this->filled_flat_side_triangle_(x1, y1, x2, y2, x3, y3, color);
  } else if (y1 == y2) {  // Check for special case of a top-flat triangle
    this->filled_flat_side_triangle_(x3, y3, x1, y1, x2, y2, color);
  } else {  // General case: split the no-flat-side triangle in a top-flat triangle and bottom-flat triangle
    int x_temp = (int) (x1 + ((float) (y2 - y1) / (float) (y3 - y1)) * (x3 - x1)), y_temp = y2;
    this->filled_flat_side_triangle_(x1, y1, x2, y2, x_temp, y_temp, color);
    this->filled_flat_side_triangle_(x3, y3, x2, y2, x_temp, y_temp, color);
  }
}
void HOT Display::get_regular_polygon_vertex(int vertex_id, int *vertex_x, int *vertex_y, int center_x, int center_y,
                                             int radius, int edges, RegularPolygonVariation variation,
                                             float rotation_degrees) {
  if (edges >= 2) {
    // Given the orientation of the display component, an angle is measured clockwise from the x axis.
    // For a regular polygon, the human reference would be the top of the polygon,
    // hence we rotate the shape by 270° to orient the polygon up.
    rotation_degrees += ROTATION_270_DEGREES;
    // Convert the rotation to radians, easier to use in trigonometrical calculations
    float rotation_radians = rotation_degrees * PI / 180;
    // A pointy top variation means the first vertex of the polygon is at the top center of the shape, this requires no
    // additional rotation of the shape.
    // A flat top variation means the first point of the polygon has to be rotated so that the first edge is horizontal,
    // this requires to rotate the shape by π/edges radians counter-clockwise so that the first point is located on the
    // left side of the first horizontal edge.
    rotation_radians -= (variation == VARIATION_FLAT_TOP) ? PI / edges : 0.0;

    float vertex_angle = ((float) vertex_id) / edges * 2 * PI + rotation_radians;
    *vertex_x = (int) round(cos(vertex_angle) * radius) + center_x;
    *vertex_y = (int) round(sin(vertex_angle) * radius) + center_y;
  }
}

void HOT Display::regular_polygon(int x, int y, int radius, int edges, RegularPolygonVariation variation,
                                  float rotation_degrees, Color color, RegularPolygonDrawing drawing) {
  if (edges >= 2) {
    int previous_vertex_x, previous_vertex_y;
    for (int current_vertex_id = 0; current_vertex_id <= edges; current_vertex_id++) {
      int current_vertex_x, current_vertex_y;
      get_regular_polygon_vertex(current_vertex_id, &current_vertex_x, &current_vertex_y, x, y, radius, edges,
                                 variation, rotation_degrees);
      if (current_vertex_id > 0) {  // Start drawing after the 2nd vertex coordinates has been calculated
        if (drawing == DRAWING_FILLED) {
          this->filled_triangle(x, y, previous_vertex_x, previous_vertex_y, current_vertex_x, current_vertex_y, color);
        } else if (drawing == DRAWING_OUTLINE) {
          this->line(previous_vertex_x, previous_vertex_y, current_vertex_x, current_vertex_y, color);
        }
      }
      previous_vertex_x = current_vertex_x;
      previous_vertex_y = current_vertex_y;
    }
  }
}
void HOT Display::regular_polygon(int x, int y, int radius, int edges, RegularPolygonVariation variation, Color color,
                                  RegularPolygonDrawing drawing) {
  regular_polygon(x, y, radius, edges, variation, ROTATION_0_DEGREES, color, drawing);
}
void HOT Display::regular_polygon(int x, int y, int radius, int edges, Color color, RegularPolygonDrawing drawing) {
  regular_polygon(x, y, radius, edges, VARIATION_POINTY_TOP, ROTATION_0_DEGREES, color, drawing);
}
void Display::filled_regular_polygon(int x, int y, int radius, int edges, RegularPolygonVariation variation,
                                     float rotation_degrees, Color color) {
  regular_polygon(x, y, radius, edges, variation, rotation_degrees, color, DRAWING_FILLED);
}
void Display::filled_regular_polygon(int x, int y, int radius, int edges, RegularPolygonVariation variation,
                                     Color color) {
  regular_polygon(x, y, radius, edges, variation, ROTATION_0_DEGREES, color, DRAWING_FILLED);
}
void Display::filled_regular_polygon(int x, int y, int radius, int edges, Color color) {
  regular_polygon(x, y, radius, edges, VARIATION_POINTY_TOP, ROTATION_0_DEGREES, color, DRAWING_FILLED);
}

void Display::print(int x, int y, BaseFont *font, Color color, TextAlign align, const char *text, Color background) {
  int x_start, y_start;
  int width, height;
  this->get_text_bounds(x, y, text, font, align, &x_start, &y_start, &width, &height);
  font->print(x_start, y_start, this, color, text, background);
}

void Display::vprintf_(int x, int y, BaseFont *font, Color color, Color background, TextAlign align, const char *format,
                       va_list arg) {
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  if (ret > 0)
    this->print(x, y, font, color, align, buffer, background);
}

void Display::image(int x, int y, BaseImage *image, Color color_on, Color color_off) {
  this->image(x, y, image, ImageAlign::TOP_LEFT, color_on, color_off);
}

void Display::image(int x, int y, BaseImage *image, ImageAlign align, Color color_on, Color color_off) {
  auto x_align = ImageAlign(int(align) & (int(ImageAlign::HORIZONTAL_ALIGNMENT)));
  auto y_align = ImageAlign(int(align) & (int(ImageAlign::VERTICAL_ALIGNMENT)));

  switch (x_align) {
    case ImageAlign::RIGHT:
      x -= image->get_width();
      break;
    case ImageAlign::CENTER_HORIZONTAL:
      x -= image->get_width() / 2;
      break;
    case ImageAlign::LEFT:
    default:
      break;
  }

  switch (y_align) {
    case ImageAlign::BOTTOM:
      y -= image->get_height();
      break;
    case ImageAlign::CENTER_VERTICAL:
      y -= image->get_height() / 2;
      break;
    case ImageAlign::TOP:
    default:
      break;
  }

  image->draw(x, y, this, color_on, color_off);
}

#ifdef USE_GRAPH
void Display::graph(int x, int y, graph::Graph *graph, Color color_on) { graph->draw(this, x, y, color_on); }
void Display::legend(int x, int y, graph::Graph *graph, Color color_on) { graph->draw_legend(this, x, y, color_on); }
#endif  // USE_GRAPH

#ifdef USE_QR_CODE
void Display::qr_code(int x, int y, qr_code::QrCode *qr_code, Color color_on, int scale) {
  qr_code->draw(this, x, y, color_on, scale);
}
#endif  // USE_QR_CODE

#ifdef USE_GRAPHICAL_DISPLAY_MENU
void Display::menu(int x, int y, graphical_display_menu::GraphicalDisplayMenu *menu, int width, int height) {
  Rect rect(x, y, width, height);
  menu->draw(this, &rect);
}
#endif  // USE_GRAPHICAL_DISPLAY_MENU

void Display::get_text_bounds(int x, int y, const char *text, BaseFont *font, TextAlign align, int *x1, int *y1,
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
void Display::print(int x, int y, BaseFont *font, Color color, const char *text, Color background) {
  this->print(x, y, font, color, TextAlign::TOP_LEFT, text, background);
}
void Display::print(int x, int y, BaseFont *font, TextAlign align, const char *text) {
  this->print(x, y, font, COLOR_ON, align, text);
}
void Display::print(int x, int y, BaseFont *font, const char *text) {
  this->print(x, y, font, COLOR_ON, TextAlign::TOP_LEFT, text);
}
void Display::printf(int x, int y, BaseFont *font, Color color, Color background, TextAlign align, const char *format,
                     ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, color, background, align, format, arg);
  va_end(arg);
}
void Display::printf(int x, int y, BaseFont *font, Color color, TextAlign align, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, color, COLOR_OFF, align, format, arg);
  va_end(arg);
}
void Display::printf(int x, int y, BaseFont *font, Color color, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, color, COLOR_OFF, TextAlign::TOP_LEFT, format, arg);
  va_end(arg);
}
void Display::printf(int x, int y, BaseFont *font, TextAlign align, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, COLOR_ON, COLOR_OFF, align, format, arg);
  va_end(arg);
}
void Display::printf(int x, int y, BaseFont *font, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, COLOR_ON, COLOR_OFF, TextAlign::TOP_LEFT, format, arg);
  va_end(arg);
}
void Display::set_writer(display_writer_t &&writer) { this->writer_ = writer; }
void Display::set_pages(std::vector<DisplayPage *> pages) {
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
void Display::show_page(DisplayPage *page) {
  this->previous_page_ = this->page_;
  this->page_ = page;
  if (this->previous_page_ != this->page_) {
    for (auto *t : on_page_change_triggers_)
      t->process(this->previous_page_, this->page_);
  }
}
void Display::show_next_page() { this->page_->show_next(); }
void Display::show_prev_page() { this->page_->show_prev(); }
void Display::do_update_() {
  if (this->auto_clear_enabled_) {
    this->clear();
  }
  if (this->show_test_card_) {
    this->test_card();
  } else if (this->page_ != nullptr) {
    this->page_->get_writer()(*this);
  } else if (this->writer_.has_value()) {
    (*this->writer_)(*this);
  }
  this->clear_clipping_();
}
void DisplayOnPageChangeTrigger::process(DisplayPage *from, DisplayPage *to) {
  if ((this->from_ == nullptr || this->from_ == from) && (this->to_ == nullptr || this->to_ == to))
    this->trigger(from, to);
}
void Display::strftime(int x, int y, BaseFont *font, Color color, TextAlign align, const char *format, ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    this->print(x, y, font, color, align, buffer);
}
void Display::strftime(int x, int y, BaseFont *font, Color color, const char *format, ESPTime time) {
  this->strftime(x, y, font, color, TextAlign::TOP_LEFT, format, time);
}
void Display::strftime(int x, int y, BaseFont *font, TextAlign align, const char *format, ESPTime time) {
  this->strftime(x, y, font, COLOR_ON, align, format, time);
}
void Display::strftime(int x, int y, BaseFont *font, const char *format, ESPTime time) {
  this->strftime(x, y, font, COLOR_ON, TextAlign::TOP_LEFT, format, time);
}

void Display::start_clipping(Rect rect) {
  if (!this->clipping_rectangle_.empty()) {
    Rect r = this->clipping_rectangle_.back();
    rect.shrink(r);
  }
  this->clipping_rectangle_.push_back(rect);
}
void Display::end_clipping() {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "clear: Clipping is not set.");
  } else {
    this->clipping_rectangle_.pop_back();
  }
}
void Display::extend_clipping(Rect add_rect) {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "add: Clipping is not set.");
  } else {
    this->clipping_rectangle_.back().extend(add_rect);
  }
}
void Display::shrink_clipping(Rect add_rect) {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "add: Clipping is not set.");
  } else {
    this->clipping_rectangle_.back().shrink(add_rect);
  }
}
Rect Display::get_clipping() const {
  if (this->clipping_rectangle_.empty()) {
    return Rect();
  } else {
    return this->clipping_rectangle_.back();
  }
}
void Display::clear_clipping_() { this->clipping_rectangle_.clear(); }
bool Display::clip(int x, int y) {
  if (x < 0 || x >= this->get_width() || y < 0 || y >= this->get_height())
    return false;
  if (!this->get_clipping().inside(x, y))
    return false;
  return true;
}
bool Display::clamp_x_(int x, int w, int &min_x, int &max_x) {
  min_x = std::max(x, 0);
  max_x = std::min(x + w, this->get_width());

  if (!this->clipping_rectangle_.empty()) {
    const auto &rect = this->clipping_rectangle_.back();
    if (!rect.is_set())
      return false;

    min_x = std::max(min_x, (int) rect.x);
    max_x = std::min(max_x, (int) rect.x2());
  }

  return min_x < max_x;
}
bool Display::clamp_y_(int y, int h, int &min_y, int &max_y) {
  min_y = std::max(y, 0);
  max_y = std::min(y + h, this->get_height());

  if (!this->clipping_rectangle_.empty()) {
    const auto &rect = this->clipping_rectangle_.back();
    if (!rect.is_set())
      return false;

    min_y = std::max(min_y, (int) rect.y);
    max_y = std::min(max_y, (int) rect.y2());
  }

  return min_y < max_y;
}

const uint8_t TESTCARD_FONT[3][8] PROGMEM = {{0x41, 0x7F, 0x7F, 0x09, 0x19, 0x7F, 0x66, 0x00},   // 'R'
                                             {0x1C, 0x3E, 0x63, 0x41, 0x51, 0x73, 0x72, 0x00},   // 'G'
                                             {0x41, 0x7F, 0x7F, 0x49, 0x49, 0x7F, 0x36, 0x00}};  // 'B'

void Display::test_card() {
  int w = get_width(), h = get_height(), image_w, image_h;
  this->clear();
  this->show_test_card_ = false;
  if (this->get_display_type() == DISPLAY_TYPE_COLOR) {
    Color r(255, 0, 0), g(0, 255, 0), b(0, 0, 255);
    image_w = std::min(w - 20, 310);
    image_h = std::min(h - 20, 255);

    int shift_x = (w - image_w) / 2;
    int shift_y = (h - image_h) / 2;
    int line_w = (image_w - 6) / 6;
    int image_c = image_w / 2;
    for (auto i = 0; i <= image_h; i++) {
      int c = esp_scale(i, image_h);
      this->horizontal_line(shift_x + 0, shift_y + i, line_w, r.fade_to_white(c));
      this->horizontal_line(shift_x + line_w, shift_y + i, line_w, r.fade_to_black(c));  //

      this->horizontal_line(shift_x + image_c - line_w, shift_y + i, line_w, g.fade_to_white(c));
      this->horizontal_line(shift_x + image_c, shift_y + i, line_w, g.fade_to_black(c));

      this->horizontal_line(shift_x + image_w - (line_w * 2), shift_y + i, line_w, b.fade_to_white(c));
      this->horizontal_line(shift_x + image_w - line_w, shift_y + i, line_w, b.fade_to_black(c));
    }
    this->rectangle(shift_x, shift_y, image_w, image_h, Color(127, 127, 0));

    uint16_t shift_r = shift_x + line_w - (8 * 3);
    uint16_t shift_g = shift_x + image_c - (8 * 3);
    uint16_t shift_b = shift_x + image_w - line_w - (8 * 3);
    shift_y = h / 2 - (8 * 3);
    for (auto i = 0; i < 8; i++) {
      uint8_t ftr = progmem_read_byte(&TESTCARD_FONT[0][i]);
      uint8_t ftg = progmem_read_byte(&TESTCARD_FONT[1][i]);
      uint8_t ftb = progmem_read_byte(&TESTCARD_FONT[2][i]);
      for (auto k = 0; k < 8; k++) {
        if ((ftr & (1 << k)) != 0) {
          this->filled_rectangle(shift_r + (i * 6), shift_y + (k * 6), 6, 6, COLOR_OFF);
        }
        if ((ftg & (1 << k)) != 0) {
          this->filled_rectangle(shift_g + (i * 6), shift_y + (k * 6), 6, 6, COLOR_OFF);
        }
        if ((ftb & (1 << k)) != 0) {
          this->filled_rectangle(shift_b + (i * 6), shift_y + (k * 6), 6, 6, COLOR_OFF);
        }
      }
    }
  }
  this->rectangle(0, 0, w, h, Color(127, 0, 127));
  this->filled_rectangle(0, 0, 10, 10, Color(255, 0, 255));
  this->stop_poller();
}

DisplayPage::DisplayPage(display_writer_t writer) : writer_(std::move(writer)) {}
void DisplayPage::show() { this->parent_->show_page(this); }
void DisplayPage::show_next() { this->next_->show(); }
void DisplayPage::show_prev() { this->prev_->show(); }
void DisplayPage::set_parent(Display *parent) { this->parent_ = parent; }
void DisplayPage::set_prev(DisplayPage *prev) { this->prev_ = prev; }
void DisplayPage::set_next(DisplayPage *next) { this->next_ = next; }
const display_writer_t &DisplayPage::get_writer() const { return this->writer_; }

const LogString *text_align_to_string(TextAlign textalign) {
  switch (textalign) {
    case TextAlign::TOP_LEFT:
      return LOG_STR("TOP_LEFT");
    case TextAlign::TOP_CENTER:
      return LOG_STR("TOP_CENTER");
    case TextAlign::TOP_RIGHT:
      return LOG_STR("TOP_RIGHT");
    case TextAlign::CENTER_LEFT:
      return LOG_STR("CENTER_LEFT");
    case TextAlign::CENTER:
      return LOG_STR("CENTER");
    case TextAlign::CENTER_RIGHT:
      return LOG_STR("CENTER_RIGHT");
    case TextAlign::BASELINE_LEFT:
      return LOG_STR("BASELINE_LEFT");
    case TextAlign::BASELINE_CENTER:
      return LOG_STR("BASELINE_CENTER");
    case TextAlign::BASELINE_RIGHT:
      return LOG_STR("BASELINE_RIGHT");
    case TextAlign::BOTTOM_LEFT:
      return LOG_STR("BOTTOM_LEFT");
    case TextAlign::BOTTOM_CENTER:
      return LOG_STR("BOTTOM_CENTER");
    case TextAlign::BOTTOM_RIGHT:
      return LOG_STR("BOTTOM_RIGHT");
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace display
}  // namespace esphome
