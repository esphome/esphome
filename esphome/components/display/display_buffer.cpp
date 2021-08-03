#include "display_buffer.h"

#include "esphome/core/application.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace display {

static const char *const TAG = "display";

const Color COLOR_OFF(0, 0, 0, 0);
const Color COLOR_ON(255, 255, 255, 255);

void DisplayBuffer::init_internal_(uint32_t buffer_length) {
  this->buffer_ = new uint8_t[buffer_length];
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
        for (int glyph_x = 0; glyph_x < glyph_width; glyph_x++)
          for (int glyph_y = 0; glyph_y < height; glyph_y++)
            this->draw_pixel_at(glyph_x + x_at, glyph_y + y_start, color);
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
  }
}

void DisplayBuffer::graph(int x, int y, Graph *graph, Color color_on, Color color_off) {
  for (int img_x = 0; img_x < graph->get_width(); img_x++) {
    for (int img_y = 0; img_y < graph->get_height(); img_y++) {
      this->draw_pixel_at(x + img_x, y + img_y, graph->get_pixel(img_x, img_y) ? color_on : color_off);
    }
  }
}

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
  this->clear();
  if (this->page_ != nullptr) {
    this->page_->get_writer()(*this);
  } else if (this->writer_.has_value()) {
    (*this->writer_)(*this);
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

bool Glyph::get_pixel(int x, int y) const {
  const int x_data = x - this->glyph_data_->offset_x;
  const int y_data = y - this->glyph_data_->offset_y;
  if (x_data < 0 || x_data >= this->glyph_data_->width || y_data < 0 || y_data >= this->glyph_data_->height)
    return false;
  const uint32_t width_8 = ((this->glyph_data_->width + 7u) / 8u) * 8u;
  const uint32_t pos = x_data + y_data * width_8;
  return pgm_read_byte(this->glyph_data_->data + (pos / 8u)) & (0x80 >> (pos % 8u));
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
    if (this->glyphs_[mid].compare_to(str))
      lo = mid;
    else
      hi = mid - 1;
  }
  *match_length = this->glyphs_[lo].match_length(str);
  if (*match_length <= 0)
    return -1;
  return lo;
}
void Font::measure(const char *str, int *width, int *x_offset, int *baseline, int *height) {
  *baseline = this->baseline_;
  *height = this->bottom_;
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
    if (!has_char)
      min_x = glyph.glyph_data_->offset_x;
    else
      min_x = std::min(min_x, x + glyph.glyph_data_->offset_x);
    x += glyph.glyph_data_->width + glyph.glyph_data_->offset_x;

    i += match_length;
    has_char = true;
  }
  *x_offset = min_x;
  *width = x - min_x;
}
const std::vector<Glyph> &Font::get_glyphs() const { return this->glyphs_; }
Font::Font(const GlyphData *data, int data_nr, int baseline, int bottom) : baseline_(baseline), bottom_(bottom) {
  for (int i = 0; i < data_nr; ++i)
    glyphs_.emplace_back(data + i);
}

bool Image::get_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return false;
  const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
  const uint32_t pos = x + y * width_8;
  return pgm_read_byte(this->data_start_ + (pos / 8u)) & (0x80 >> (pos % 8u));
}
Color Image::get_color_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return 0;
  const uint32_t pos = (x + y * this->width_) * 3;
  const uint32_t color32 = (pgm_read_byte(this->data_start_ + pos + 2) << 0) |
                           (pgm_read_byte(this->data_start_ + pos + 1) << 8) |
                           (pgm_read_byte(this->data_start_ + pos + 0) << 16);
  return Color(color32);
}
Color Image::get_grayscale_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return 0;
  const uint32_t pos = (x + y * this->width_);
  const uint8_t gray = pgm_read_byte(this->data_start_ + pos);
  return Color(gray | gray << 8 | gray << 16 | gray << 24);
}
int Image::get_width() const { return this->width_; }
int Image::get_height() const { return this->height_; }
ImageType Image::get_type() const { return this->type_; }
Image::Image(const uint8_t *data_start, int width, int height, ImageType type)
    : width_(width), height_(height), type_(type), data_start_(data_start) {}

HistoryData::HistoryData(int length)
    : length_(length) {
  this->data_ = new float[length];
  if (this->data_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate HistoryData buffer!");
    return;
  }
  for (int i = 0; i < this->length_; i++)
    this->data_[i] = NAN;
}
HistoryData::~HistoryData() { delete (this->data_); }
void HistoryData::take_sample(float data) {
  this->data_[this->count_] = data;
  if (!isnan(data)) {
    // Track all-time max/min
    if (isnan(this->max_) || (data > this->max_)) {
      this->max_ = data;
    }
    if (isnan(this->min_) || (data < this->min_)) {
      this->min_ = data;
    }
    // Recalc recent max/min
    this->recent_min_ = data;
    this->recent_max_ = data;
    for (int i = 0; i < this->length_; i++) {
      if (!isnan(this->data_[i])) {
        if (this->recent_max_ < this->data_[i])
          this->recent_max_ = this->data_[i];
        if (this->recent_min_ > this->data_[i])
          this->recent_min_ = this->data_[i];
      }
    }
  }
  this->count_ = (this->count_ + 1) % this->length_;
}

void Graph::dump_config() { LOG_UPDATE_INTERVAL(this); }

bool Graph::get_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return false;
  const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
  const uint32_t pos = x + y * width_8;
  return this->pixels_[pos / 8u] & (0x80 >> (pos % 8u));
}
Color Graph::get_grayscale_pixel(int x, int y) const {
  const uint8_t gray = (bool) this->get_pixel(x, y) * 255;
  return Color(gray | gray << 8 | gray << 16 | gray << 24);
}
Color Graph::get_color_pixel(int x, int y) const { return this->get_grayscale_pixel(x, y); }
void Graph::set_pixel_(int x, int y) {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return;
  const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
  const uint32_t pos = x + y * width_8;
  this->pixels_[pos / 8u] |= (0x80 >> (pos % 8u));
}
void Graph::redraw_() {
  /// Clear graph pixel buffer
  uint16_t sz = ((this->width_ + 7u) / 8u) * this->height_;
  for (uint16_t i = 0; i < sz; i++)
    this->pixels_[i] = 0;
  /// Plot border
  if (this->border_) {
    for (int i = 0; i < this->width_; i++) {
      this->set_pixel_(i, 0);
      this->set_pixel_(i, this->height_ - 1);
    }
    for (int i = 0; i < this->height_; i++) {
      this->set_pixel_(0, i);
      this->set_pixel_(this->width_ - 1, i);
    }
  }
  /// Determine best grid scale and range
  /// Get max / min values for history data
  float ymin = this->data_->get_recent_min();
  float ymax = this->data_->get_recent_max();

  // Adjust if manually overridden
  if (!isnan(this->min_value_))
    ymin = this->min_value_;
  if (!isnan(this->max_value_))
    ymax = this->max_value_;

  float yrange = ymax - ymin;
  if (isnan(yrange) || (yrange == 0)) {
    ESP_LOGV(TAG, "Graph, forcing yrange to 1");
    yrange = 1;
  }
  if (yrange < this->min_range_) {
    // Adjust range to keep last value in centre
    float s = this->data_->get_value(0);
    ymin = s - (yrange / 2.0);
    ymax = s + (yrange / 2.0);
    yrange = this->min_range_;
    ESP_LOGV(TAG, "Graphing forcing yrange to min_range");
  }
  if (yrange > this->max_range_) {
    // Look back in data to fit into local range
    float mx = NAN;
    float mn = NAN;
    for (int16_t i = 0; i < this->data_->get_length(); i++) {
      float v = this->data_->get_value(i);
      if (!isnan(v)) {
        if ((v - mn) > this->max_range_)
          break;
        if ((mx - v) > this->max_range_)
          break;
        if (isnan(mx) || (v > mx))
          mx = v;
        if (isnan(mn) || (v < mn))
          mn = v;
      }
    }
    yrange = this->max_range_;
    if (!isnan(mn)) {
      ymin = mn;
      ymax = ymin + this->max_range_;
    }
    ESP_LOGV(TAG, "Graphing at max_range. Using local min %f, max %f", mn, mx);
  }
  /// Draw grid
  if (!isnan(this->gridspacing_y_)) {
    float y_per_div = this->gridspacing_y_;
    int yn = int(ymin / y_per_div);
    int ym = int(ymax / y_per_div) + int(1 * (fmodf(ymax, y_per_div) != 0));
    for (int y = yn; y <= ym; y++) {
      int16_t py = (int16_t) roundf((this->height_ - 1) * (1.0 - (float) (y - yn) / (ym - yn)));
      for (int x = 0; x < this->width_; x += 2) {
        this->set_pixel_(x, py);
      }
    }
    ymin = yn * y_per_div;
    ymax = ym * y_per_div;
    yrange = ymax - ymin;
  }
  if (!isnan(this->gridspacing_x_)) {
    for (int i = 0; i < (this->width_+1) / this->gridspacing_x_; i++) {
      for (int y = 0; y < this->height_; y += 2) {
        this->set_pixel_(this->width_ - 1 - i * this->gridspacing_x_, y);
      }
    }
  }
  ESP_LOGI(TAG, "Updating graph. Last sample %f, ymin %f, ymax %f, yrange %f", this->data_->get_value(0), ymin, ymax, yrange);
  /// Draw data trace
  for (int16_t i = 0; i < this->data_->get_length(); i++) {
    float v = (this->data_->get_value(i) - ymin) / yrange;
    if (!isnan(v) && (this->line_thickness_ > 0)) {
      int16_t x = this->width_ - i;
      uint8_t b = (i % (this->line_thickness_ * LineType::PATTERN_LENGTH)) / this->line_thickness_;
      if ((this->line_type_ & (1 << b)) == (1 << b)) {
        int16_t y = (int16_t) roundf((this->height_ - 1) * (1.0 - v)) - this->line_thickness_ / 2;
        for (int16_t t = 0; t < this->line_thickness_; t++) {
          this->set_pixel_(x, y + t);
        }
      }
    }
  }
}

void Graph::update() {
  float sensor_value = this->sensor_->get_state();
  this->data_->take_sample(sensor_value);
  ESP_LOGV(TAG, "Updating graph with value: %f", sensor_value);
  this->redraw_();  // TODO: move to only when updating display
}
void Graph::set_sensor(sensor::Sensor *sensor) {this->sensor_ = sensor; }
int Graph::get_width() const { return this->width_; }
int Graph::get_height() const { return this->height_; }

Graph::Graph(int width, int height) : width_(width), height_(height) {
  uint16_t sz = ((this->width_ + 7u) / 8u) * this->height_;
  this->pixels_ = new uint8_t[sz];
  if (this->pixels_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate graph pixel buffer!");
    return;
  }
  for (uint16_t i = 0; i < sz; i++) {
    this->pixels_[i] = 0xF0;
  }
  this->data_ = new HistoryData(width);
}
Graph::~Graph() { delete (this->pixels_); }

bool Animation::get_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return false;
  const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
  const uint32_t frame_index = this->height_ * width_8 * this->current_frame_;
  if (frame_index >= this->width_ * this->height_ * this->animation_frame_count_)
    return false;
  const uint32_t pos = x + y * width_8 + frame_index;
  return pgm_read_byte(this->data_start_ + (pos / 8u)) & (0x80 >> (pos % 8u));
}
Color Animation::get_color_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return 0;
  const uint32_t frame_index = this->width_ * this->height_ * this->current_frame_;
  if (frame_index >= this->width_ * this->height_ * this->animation_frame_count_)
    return 0;
  const uint32_t pos = (x + y * this->width_ + frame_index) * 3;
  const uint32_t color32 = (pgm_read_byte(this->data_start_ + pos + 2) << 0) |
                           (pgm_read_byte(this->data_start_ + pos + 1) << 8) |
                           (pgm_read_byte(this->data_start_ + pos + 0) << 16);
  return Color(color32);
}
Color Animation::get_grayscale_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return 0;
  const uint32_t frame_index = this->width_ * this->height_ * this->current_frame_;
  if (frame_index >= this->width_ * this->height_ * this->animation_frame_count_)
    return 0;
  const uint32_t pos = (x + y * this->width_ + frame_index);
  const uint8_t gray = pgm_read_byte(this->data_start_ + pos);
  return Color(gray | gray << 8 | gray << 16 | gray << 24);
}
Animation::Animation(const uint8_t *data_start, int width, int height, uint32_t animation_frame_count, ImageType type)
    : Image(data_start, width, height, type), animation_frame_count_(animation_frame_count) {
  current_frame_ = 0;
}
int Animation::get_animation_frame_count() const { return this->animation_frame_count_; }
int Animation::get_current_frame() const { return this->current_frame_; }
void Animation::next_frame() {
  this->current_frame_++;
  if (this->current_frame_ >= animation_frame_count_) {
    this->current_frame_ = 0;
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
