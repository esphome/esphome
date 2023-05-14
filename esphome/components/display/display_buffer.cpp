#include "display_buffer.h"

#include <utility>
#include "esphome/core/application.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace display {

static const char *const TAG = "display";

const Color COLOR_OFF(0, 0, 0, 0);
const Color COLOR_ON(255, 255, 255, 255);

void Rect::expand(int16_t horizontal, int16_t vertical) {
  if (this->is_set() && (this->w >= (-2 * horizontal)) && (this->h >= (-2 * vertical))) {
    this->x = this->x - horizontal;
    this->y = this->y - vertical;
    this->w = this->w + (2 * horizontal);
    this->h = this->h + (2 * vertical);
  }
}

void Rect::extend(Rect rect) {
  if (!this->is_set()) {
    this->x = rect.x;
    this->y = rect.y;
    this->w = rect.w;
    this->h = rect.h;
  } else {
    if (this->x > rect.x) {
      this->w = this->w + (this->x - rect.x);
      this->x = rect.x;
    }
    if (this->y > rect.y) {
      this->h = this->h + (this->y - rect.y);
      this->y = rect.y;
    }
    if (this->x2() < rect.x2()) {
      this->w = rect.x2() - this->x;
    }
    if (this->y2() < rect.y2()) {
      this->h = rect.y2() - this->y;
    }
  }
}
void Rect::shrink(Rect rect) {
  if (!this->inside(rect)) {
    (*this) = Rect();
  } else {
    if (this->x2() > rect.x2()) {
      this->w = rect.x2() - this->x;
    }
    if (this->x < rect.x) {
      this->w = this->w + (this->x - rect.x);
      this->x = rect.x;
    }
    if (this->y2() > rect.y2()) {
      this->h = rect.y2() - this->y;
    }
    if (this->y < rect.y) {
      this->h = this->h + (this->y - rect.y);
      this->y = rect.y;
    }
  }
}

bool Rect::equal(Rect rect) {
  return (rect.x == this->x) && (rect.w == this->w) && (rect.y == this->y) && (rect.h == this->h);
}

bool Rect::inside(int16_t test_x, int16_t test_y, bool absolute) {  // NOLINT
  if (!this->is_set()) {
    return true;
  }
  if (absolute) {
    return ((test_x >= this->x) && (test_x <= this->x2()) && (test_y >= this->y) && (test_y <= this->y2()));
  } else {
    return ((test_x >= 0) && (test_x <= this->w) && (test_y >= 0) && (test_y <= this->h));
  }
}

bool Rect::inside(Rect rect, bool absolute) {
  if (!this->is_set() || !rect.is_set()) {
    return true;
  }
  if (absolute) {
    return ((rect.x <= this->x2()) && (rect.x2() >= this->x) && (rect.y <= this->y2()) && (rect.y2() >= this->y));
  } else {
    return ((rect.x <= this->w) && (rect.w >= 0) && (rect.y <= this->h) && (rect.h >= 0));
  }
}

void Rect::info(const std::string &prefix) {
  if (this->is_set()) {
    ESP_LOGI(TAG, "%s [%3d,%3d,%3d,%3d] (%3d,%3d)", prefix.c_str(), this->x, this->y, this->w, this->h, this->x2(),
             this->y2());
  } else
    ESP_LOGI(TAG, "%s ** IS NOT SET **", prefix.c_str());
}

void DisplayBuffer::init_internal_(uint32_t buffer_length) {
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buffer_ = allocator.allocate(buffer_length);
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
    return;
  }
  this->clear();
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
void HOT DisplayBuffer::line(int x1, int y1, int x2, int y2, Color color) {
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
void HOT DisplayBuffer::horizontal_line(int x, int y, int width, Color color) {
  // Future: Could be made more efficient by manipulating buffer directly in certain rotations.
  for (int i = x; i < x + width; i++)
    this->draw_pixel_at(i, y, color);
}
void HOT DisplayBuffer::vertical_line(int x, int y, int height, Color color) {
  // Future: Could be made more efficient by manipulating buffer directly in certain rotations.
  for (int i = y; i < y + height; i++)
    this->draw_pixel_at(x, i, color);
}
void DisplayBuffer::rectangle(int x1, int y1, int width, int height, Color color) {
  this->horizontal_line(x1, y1, width, color);
  this->horizontal_line(x1, y1 + height - 1, width, color);
  this->vertical_line(x1, y1, height, color);
  this->vertical_line(x1 + width - 1, y1, height, color);
}
void DisplayBuffer::filled_rectangle(int x1, int y1, int width, int height, Color color) {
  // Future: Use vertical_line and horizontal_line methods depending on rotation to reduce memory accesses.
  for (int i = y1; i < y1 + height; i++) {
    this->horizontal_line(x1, i, width, color);
  }
}
void HOT DisplayBuffer::circle(int center_x, int center_xy, int radius, Color color) {
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
void DisplayBuffer::filled_circle(int center_x, int center_y, int radius, Color color) {
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

    {
      const int glyph_x_max = scan_x1 + scan_width;
      const int glyph_y_max = scan_y1 + scan_height;
      for (int glyph_x = scan_x1; glyph_x < glyph_x_max; glyph_x++) {
        for (int glyph_y = scan_y1; glyph_y < glyph_y_max; glyph_y++) {
          if (glyph.get_pixel(glyph_x, glyph_y)) {
            this->draw_pixel_at(glyph_x + x_at, glyph_y + y_start, color);
          }
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
void DisplayBuffer::do_update_() {
  if (this->auto_clear_enabled_) {
    this->clear();
  }
  if (this->page_ != nullptr) {
    this->page_->get_writer()(*this);
  } else if (this->writer_.has_value()) {
    (*this->writer_)(*this);
  }
  // remove all not ended clipping regions
  while (is_clipping()) {
    end_clipping();
  }
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

void DisplayBuffer::start_clipping(Rect rect) {
  if (!this->clipping_rectangle_.empty()) {
    Rect r = this->clipping_rectangle_.back();
    rect.shrink(r);
  }
  this->clipping_rectangle_.push_back(rect);
}
void DisplayBuffer::end_clipping() {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "clear: Clipping is not set.");
  } else {
    this->clipping_rectangle_.pop_back();
  }
}
void DisplayBuffer::extend_clipping(Rect add_rect) {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "add: Clipping is not set.");
  } else {
    this->clipping_rectangle_.back().extend(add_rect);
  }
}
void DisplayBuffer::shrink_clipping(Rect add_rect) {
  if (this->clipping_rectangle_.empty()) {
    ESP_LOGE(TAG, "add: Clipping is not set.");
  } else {
    this->clipping_rectangle_.back().shrink(add_rect);
  }
}
Rect DisplayBuffer::get_clipping() {
  if (this->clipping_rectangle_.empty()) {
    return Rect();
  } else {
    return this->clipping_rectangle_.back();
  }
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
Font::Font(const GlyphData *data, int data_nr, int baseline, int height) : baseline_(baseline), height_(height) {
  glyphs_.reserve(data_nr);
  for (int i = 0; i < data_nr; ++i)
    glyphs_.emplace_back(&data[i]);
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
int Image::get_current_frame() const { return 0; }

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

void Animation::set_frame(int frame) {
  unsigned abs_frame = abs(frame);

  if (abs_frame < this->animation_frame_count_) {
    if (frame >= 0) {
      this->current_frame_ = frame;
    } else {
      this->current_frame_ = this->animation_frame_count_ - abs_frame;
    }
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
