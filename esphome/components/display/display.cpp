#include "display.h"

#include <utility>

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

void Display::print(int x, int y, BaseFont *font, Color color, TextAlign align, const char *text) {
  int x_start, y_start;
  int width, height;
  this->get_text_bounds(x, y, text, font, align, &x_start, &y_start, &width, &height);
  font->print(x_start, y_start, this, color, text);
}
void Display::vprintf_(int x, int y, BaseFont *font, Color color, TextAlign align, const char *format, va_list arg) {
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  if (ret > 0)
    this->print(x, y, font, color, align, buffer);
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
void Display::print(int x, int y, BaseFont *font, Color color, const char *text) {
  this->print(x, y, font, color, TextAlign::TOP_LEFT, text);
}
void Display::print(int x, int y, BaseFont *font, TextAlign align, const char *text) {
  this->print(x, y, font, COLOR_ON, align, text);
}
void Display::print(int x, int y, BaseFont *font, const char *text) {
  this->print(x, y, font, COLOR_ON, TextAlign::TOP_LEFT, text);
}
void Display::printf(int x, int y, BaseFont *font, Color color, TextAlign align, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, color, align, format, arg);
  va_end(arg);
}
void Display::printf(int x, int y, BaseFont *font, Color color, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, color, TextAlign::TOP_LEFT, format, arg);
  va_end(arg);
}
void Display::printf(int x, int y, BaseFont *font, TextAlign align, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, COLOR_ON, align, format, arg);
  va_end(arg);
}
void Display::printf(int x, int y, BaseFont *font, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(x, y, font, COLOR_ON, TextAlign::TOP_LEFT, format, arg);
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
  if (this->page_ != nullptr) {
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

DisplayPage::DisplayPage(display_writer_t writer) : writer_(std::move(writer)) {}
void DisplayPage::show() { this->parent_->show_page(this); }
void DisplayPage::show_next() { this->next_->show(); }
void DisplayPage::show_prev() { this->prev_->show(); }
void DisplayPage::set_parent(Display *parent) { this->parent_ = parent; }
void DisplayPage::set_prev(DisplayPage *prev) { this->prev_ = prev; }
void DisplayPage::set_next(DisplayPage *next) { this->next_ = next; }
const display_writer_t &DisplayPage::get_writer() const { return this->writer_; }

}  // namespace display
}  // namespace esphome
