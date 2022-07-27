#include "display_buffer.h"

#include <utility>
#include "esphome/core/application.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <math.h>

namespace esphome {
namespace display {
// Forward declaration for trigonometric lookup table
#if defined(LUT_SIN_64)
extern const uint16_t m_nLUTSinF0X16[65];
#else
extern const uint16_t m_nLUTSinF0X16[257];
#endif

static const char *const TAG = "display";

const Color COLOR_OFF(0, 0, 0, 0);
const Color COLOR_ON(255, 255, 255, 255);

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
void DisplayBuffer::set_rotation(DisplayRotation rotation) {
  this->rotation_ = rotation;
#ifdef USE_EXTENDEDDRAW
    this->clear_clipping();
#endif
}
void HOT DisplayBuffer::draw_pixel_at(int x, int y, Color color) {
#ifdef USE_EXTENDEDDRAW
  if (this->is_clipped(x,y)) return;
#endif

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
void DisplayBuffer::do_update_() {
  if (this->auto_clear_enabled_) {
    this->clear();
  }
  if (this->page_ != nullptr) {
    this->page_->get_writer()(*this);
  } else if (this->writer_.has_value()) {
    (*this->writer_)(*this);
  }
#ifdef USE_EXTENDEDDRAW
  this->clear_clipping();
#endif
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

#ifdef USE_EXTENDEDDRAW


void DisplayBuffer::set_transparent_color(Color color)
{
  this->transparant_color_ = color;
}


// Call with nMidAmt=500 to create simple linear blend between two colors
Color DisplayBuffer::blend_color(Color color_start, Color color_end, uint16_t mid_amt, uint16_t blend_amt)
{
  Color color_mid;
  color_mid.r = (color_end.r + color_start.r) / 2;
  color_mid.g = (color_end.g + color_start.g) / 2;
  color_mid.b = (color_end.b + color_start.b) / 2;
  return this->blend_color(color_start, color_mid, color_end, mid_amt, blend_amt);
}

Color DisplayBuffer::blend_color(Color color_start, Color color_mid, Color color_end, uint16_t mid_amt, uint16_t blend_amt)
{
  Color colNew;
  mid_amt = (mid_amt > 1000) ? 1000 : mid_amt;
  blend_amt = (blend_amt > 1000) ? 1000 : blend_amt;

  uint16_t nRngLow = mid_amt;
  uint16_t nRngHigh = 1000 - mid_amt;
  int32_t nSubBlendAmt;
  if (blend_amt >= mid_amt)
  {
    nSubBlendAmt = (int32_t)(blend_amt - mid_amt) * 1000 / nRngHigh;
    colNew.r = nSubBlendAmt * (color_end.r - color_mid.r) / 1000 + color_mid.r;
    colNew.g = nSubBlendAmt * (color_end.g - color_mid.g) / 1000 + color_mid.g;
    colNew.b = nSubBlendAmt * (color_end.b - color_mid.b) / 1000 + color_mid.b;
  }
  else
  {
    nSubBlendAmt = (int32_t)(blend_amt - 0) * 1000 / nRngLow;
    colNew.r = nSubBlendAmt * (color_mid.r - color_start.r) / 1000 + color_start.r;
    colNew.g = nSubBlendAmt * (color_mid.g - color_start.g) / 1000 + color_start.g;
    colNew.b = nSubBlendAmt * (color_mid.b - color_start.b) / 1000 + color_start.b;
  }
  return colNew;
}

bool DisplayBuffer::is_color_equal(Color a, Color b)
{
  return a.r == b.r && a.g == b.g && a.b == b.b;
}

// -----


// Expand or contract a rectangle in width and/or height (equal
// amounts on both side), based on the centerpoint of the rectangle.
Rect DisplayBuffer::expand_rect(Rect rect, uint16_t width, uint16_t height)
{
  Rect rNew = {1,1,0,0};

  // Detect error case of contracting region too far
  if ( ((int16_t)rect.w < (-2*width)) || ((int16_t)rect.h < (-2*height)) ) {
    // Return an empty coordinate box (which won't be drawn)
    return rNew;
  }

  // Adjust the new width/height
  // Note that the overall width/height changes by a factor of
  // two since we are applying the adjustment on both sides (ie.
  // top/bottom or left/right) equally.
  rNew.w = rect.w + (2*width);
  rNew.h = rect.h + (2*height);

  // Adjust the rectangle coordinate to allow for new dimensions
  // Note that this moves the coordinate in the opposite
  // direction of the expansion/contraction.
  rNew.x = rect.x - width;
  rNew.y = rect.y - height;

  return rNew;
}

// Expand the current rect (pRect) to enclose the additional rect region (rAddRect)
Rect DisplayBuffer::union_rect(Rect rect, Rect addRect)
{
  int16_t nSrcX0, nSrcY0, nSrcX1, nSrcY1;
  int16_t nAddX0, nAddY0, nAddX1, nAddY1;

  // If the source rect has zero dimensions, then treat as empty
  if ((rect.w == 0) || (rect.h == 0)) {
    // No source region defined, simply copy add region
    return addRect;
  }

  // Source region valid, so increase dimensions

  // Calculate the rect boundary coordinates
  nSrcX0 = rect.x;
  nSrcY0 = rect.y;
  nSrcX1 = rect.x + rect.w - 1;
  nSrcY1 = rect.y + rect.h - 1;
  nAddX0 = addRect.x;
  nAddY0 = addRect.y;
  nAddX1 = addRect.x + addRect.w - 1;
  nAddY1 = addRect.y + addRect.h - 1;

  // Find the new maximal dimensions
  nSrcX0 = (nAddX0 < nSrcX0) ? nAddX0 : nSrcX0;
  nSrcY0 = (nAddY0 < nSrcY0) ? nAddY0 : nSrcY0;
  nSrcX1 = (nAddX1 > nSrcX1) ? nAddX1 : nSrcX1;
  nSrcY1 = (nAddY1 > nSrcY1) ? nAddY1 : nSrcY1;

  // Update the original rect region
  return Rect(nSrcX0, nSrcY0, (uint16_t) (nSrcX1 - nSrcX0 + 1), (uint16_t) (nSrcY1 - nSrcY0 + 1));
}

bool DisplayBuffer::in_rect(int16_t nSelX, int16_t nSelY, Rect rRect) {
  return ((nSelX >= rRect.x) && (nSelX <= rRect.x + (int16_t)rRect.w) &&
          (nSelY >= rRect.y) && (nSelY <= rRect.y + (int16_t)rRect.h));
}

bool DisplayBuffer::is_inside(int16_t x, int16_t y, uint16_t width, uint16_t height)
{
  return ((x >= 0) && (x <= (int16_t)(width)-1) &&
          (y >= 0) && (y <= (int16_t)(height)-1));
}


void DisplayBuffer::clear_clipping()
{
  this->clipping_rectangle_ = (Rect) { 0, 0, 0, 0};
}

void DisplayBuffer::add_clipping(Rect addRect)
{
  this->clipping_rectangle_ = this->union_rect(this->clipping_rectangle_, addRect);
}

void DisplayBuffer::set_clipping(Rect rect)
{
  this->clipping_rectangle_ = rect;
}

Rect DisplayBuffer::get_clipping()
{
  return this->clipping_rectangle_;
}

bool DisplayBuffer::is_clipped(int16_t x, int16_t y)
{
  if ((this->clipping_rectangle_.w == 0) || (this->clipping_rectangle_.h == 0))  return false;

  int16_t nCX0 = this->clipping_rectangle_.x;
  int16_t nCY0 = this->clipping_rectangle_.y;
  int16_t nCX1 = this->clipping_rectangle_.x + this->clipping_rectangle_.w - 1;
  int16_t nCY1 = this->clipping_rectangle_.y + this->clipping_rectangle_.h - 1;
/*
  switch (this->rotation_)
  {
  case DISPLAY_ROTATION_0_DEGREES:
    break;
  case DISPLAY_ROTATION_90_DEGREES:
    std::swap(nCX0, nCY0);
    nCX0 = this->get_width_internal() - nCX0 - 1;
    std::swap(nCX1, nCY1);
    nCX1 = this->get_width_internal() - nCX1 - 1;
    break;
  case DISPLAY_ROTATION_180_DEGREES:
    nCX0 = this->get_width_internal() - nCX0 - 1;
    nCY0 = this->get_height_internal() - nCY0 - 1;
    nCX1 = this->get_width_internal() - nCX1 - 1;
    nCY1 = this->get_height_internal() - nCY1 - 1;
    break;
  case DISPLAY_ROTATION_270_DEGREES:
    std::swap(nCX0, nCY0);
    nCY0 = this->get_height_internal() - nCY0 - 1;
    std::swap(nCX1, nCY1);
    nCY1 = this->get_height_internal() - nCY1 - 1;
    break;
  }
*/
  if ((x < nCX0) || (x > nCX1)) return true;
  if ((y < nCY0) || (y > nCY1)) return true;
  return false;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// Graphics General Functions
// ------------------------------------------------------------------------

// Sine function with optional lookup table
// - Note that the n64Ang range is limited by 16-bit integers
//   to an effective degree range of -511 to +511 degrees,
//   defined by the max integer range: 32767/64.
int16_t DisplayBuffer::get_sin(int16_t n64Ang)
{
  // Use floating-point math library function
  // Calculate angle in radians
  float fAngRad = n64Ang * POLAR_2PI / (23040.0); // = 360.0 x 64.0
  // Perform floating point calc
  float fSin = sin(fAngRad);
  // Return as fixed point result
  return fSin * 32767.0;
}

// Cosine function with optional lookup table
// - Note that the n64Ang range is limited by 16-bit integers
//   to an effective degree range of -511 to +511 degrees,
//   defined by the max integer range: 32767/64.
int16_t DisplayBuffer::get_cos(int16_t n64Ang)
{
  // Use floating-point math library function
  // Calculate angle in radians
  float fAngRad = n64Ang * POLAR_2PI / (23040.0); // = 360.0 x 64.0
  // Perform floating point calc
  float fCos = cos(fAngRad);
  // Return as fixed point result

  return fCos * 32767.0;
}

// Convert from polar to cartesian
void DisplayBuffer::polar_to_point(uint16_t nRad, int16_t n64Ang, int16_t *nDX, int16_t *nDY)
{
  int32_t nTmp;
  // TODO: Clean up excess integer typecasting
  nTmp = (int32_t)nRad * this->get_sin(n64Ang);
  *nDX = nTmp / 32767;
  nTmp = (int32_t)nRad * -this->get_cos(n64Ang);
  *nDY = nTmp / 32767;
}


// Note that angle is in degrees * 64
void DisplayBuffer::polar_line(int16_t nX, int16_t nY, uint16_t nRadStart, uint16_t nRadEnd, int16_t n64Ang, Color nCol)
{
  // Draw the ray representing the current value
  int16_t nDxS = (int32_t)nRadStart * get_sin(n64Ang) / 32768;
  int16_t nDyS = (int32_t)nRadStart * get_cos(n64Ang) / 32768;
  int16_t nDxE = (int32_t)nRadEnd * get_sin(n64Ang) / 32768;
  int16_t nDyE = (int32_t)nRadEnd * get_cos(n64Ang) / 32768;
  this->line(nX + nDxS, nY - nDyS, nX + nDxE, nY - nDyE, nCol);
}


void HOT DisplayBuffer::rectangle(int x, int y, int width, int height, int16_t radius, Color color )
{
  int dx = -radius;
  int dy = 0;
  int err = 2 - 2 * radius;
  int e2;

  x = x + radius; y = y + radius; height = height- (radius*2); width = width- (radius*2);

//  ESP_LOGW(TAG, "Rounded rect: : '%d, %d, %d, %d'", x,y,width,height);

  //this->rectangle(x, y, width, height, COLOR_ON);


  this->horizontal_line(x, y - (radius), width, color);
  this->horizontal_line(x, y + (radius)  + height - 1, width, color);
  this->vertical_line(x-radius, y , height, color);
  this->vertical_line(x +(radius) + width - 1, y, height, color);

  do
  {
    this->draw_pixel_at(x + dx, y - dy, color);
    this->draw_pixel_at(x + dx, y + height + dy-1, color);
    this->draw_pixel_at(x + width - dx-1, y - dy, color);
    this->draw_pixel_at(x + width - dx-1, y + height + dy-1, color);
    e2 = err;
    if (e2 < dy)
    {
      err += ++dy * 2 + 1;
      if (-dx == dy && e2 <= dx)
      {
        e2 = 0;
      }
    }
    if (e2 > dx)
    {
      err += ++dx * 2 + 1;
    }
  } while (dx <= 0);
}

void DisplayBuffer::filled_rectangle(int x, int y, int width, int height, int16_t radius, Color color)
{
  int dx = -radius;
  int dy = 0;
  int err = 2 - 2 * radius;
  int e2;

  x = x + radius; y = y + radius; height = height- (radius*2); width = width- (radius*2);

  this->filled_rectangle(x-radius, y, width+(radius*2), height, color);
  do
  {
    int hline_width = width + ( 2 * (-dx) + 1)-1;
    this->horizontal_line(x + dx, y +height + dy, hline_width, color);
    this->horizontal_line(x + dx, y - dy, hline_width, color);
    e2 = err;
    if (e2 < dy)
    {
      err += ++dy * 2 + 1;
      if (-dx == dy && e2 <= dx)
      {
        e2 = 0;
      }
    }
    if (e2 > dx)
    {
      err += ++dx * 2 + 1;
    }
  } while (dx <= 0);
}

// Draw a triangle
void DisplayBuffer::triangle(int16_t nX0,int16_t nY0, int16_t nX1,int16_t nY1,int16_t nX2,int16_t nY2, Color nCol)
{
  // Draw triangle with three lines
  this->line(nX0,nY0,nX1,nY1,nCol);
  this->line(nX1,nY1,nX2,nY2,nCol);
  this->line(nX2,nY2,nX0,nY0,nCol);
}

void DisplayBuffer::swap_coords_(int16_t* pnXa,int16_t* pnYa,int16_t* pnXb,int16_t* pnYb)
{
  int16_t nSwapX,nSwapY;
  nSwapX = *pnXa;
  nSwapY = *pnYa;
  *pnXa = *pnXb;
  *pnYa = *pnYb;
  *pnXb = nSwapX;
  *pnYb = nSwapY;
}


// Draw a filled triangle
void DisplayBuffer::filled_triangle(int16_t nX0,int16_t nY0, int16_t nX1,int16_t nY1,int16_t nX2,int16_t nY2, Color nCol)
{
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
  // - Walk each scan line and determine the intersection
  //   between triangle side A & B (flat bottom triangle)
  //   and then C and B (flat top triangle) using line slopes.

  // Sort vertices
  // - Want nY0 >= nY1 >= nY2
  if (nY2>nY1) { this->swap_coords_(&nX2,&nY2,&nX1,&nY1); }
  if (nY1>nY0) { this->swap_coords_(&nX0,&nY0,&nX1,&nY1); }
  if (nY2>nY1) { this->swap_coords_(&nX2,&nY2,&nX1,&nY1); }

  // TODO: It is more efficient to calculate row endpoints
  // using incremental additions instead of multiplies/divides

  int16_t nXa,nXb,nXc,nYos;
  int16_t nX01,nX20,nY01,nY20,nX21,nY21;
  nX01 = nX0-nX1; nY01 = nY0-nY1;
  nX20 = nX2-nX0; nY20 = nY2-nY0;
  nX21 = nX2-nX1; nY21 = nY2-nY1;

  // Flat bottom scenario
  // NOTE: Due to vertex sorting and loop range, it shouldn't
  // be possible to enter loop when nY0 == nY1 or nY2
  for (nYos=0;nYos<nY01;nYos++) {
    // Determine row endpoints (no rounding)
    //nXa = (nYos          )*(nX0-nX1)/(nY0-nY1);
    //nXb = (nYos-(nY0-nY1))*(nX2-nX0)/(nY2-nY0);

    // Determine row endpoints (using rounding)
    nXa  = 2*(nYos)*nX01;
    nXa += (nXa>=0)?abs(nY01):-abs(nY01);
    nXa /= 2*nY01;

    nXb = 2*(nYos-nY01)*nX20;
    nXb += (nXb>=0)?abs(nY20):-abs(nY20);
    nXb /= 2*nY20;

    // Draw horizontal line between endpoints
    this->line(nX1+nXa,nY1+nYos,nX0+nXb,nY1+nYos,nCol);
  }

  // Flat top scenario
  // NOTE: Due to vertex sorting and loop range, it shouldn't
  // be possible to enter loop when nY2 == nY0 or nY1
  for (nYos=nY21;nYos<0;nYos++) {

    // Determine row endpoints (no rounding)
    //nXc = (nYos          )*(nX2-nX1)/(nY2-nY1);
    //nXb = (nYos-(nY0-nY1))*(nX2-nX0)/(nY2-nY0);

    // Determine row endpoints (using rounding)
    nXc  = 2*(nYos)*nX21;
    nXc += (nXc>=0)?abs(nY21):-abs(nY21);
    nXc /= 2*nY21;

    nXb = 2*(nYos-nY01)*nX20;
    nXb += (nXb>=0)?abs(nY20):-abs(nY20);
    nXb /= 2*nY20;

    // Draw horizontal line between endpoints
    this->line(nX1+nXc,nY1+nYos,nX0+nXb,nY1+nYos,nCol);
  }
}

void DisplayBuffer::quad(Point * psPt, Color nCol) {
  int16_t nX0,nY0,nX1,nY1;

  nX0 = psPt[0].x; nY0 = psPt[0].y; nX1 = psPt[1].x; nY1 = psPt[1].y;
  this->line(nX0,nY0,nX1,nY1,nCol);

  nX0 = psPt[1].x; nY0 = psPt[1].y; nX1 = psPt[2].x; nY1 = psPt[2].y;
  this->line(nX0,nY0,nX1,nY1,nCol);

  nX0 = psPt[2].x; nY0 = psPt[2].y; nX1 = psPt[3].x; nY1 = psPt[3].y;
  this->line(nX0,nY0,nX1,nY1,nCol);

  nX0 = psPt[3].x; nY0 = psPt[3].y; nX1 = psPt[0].x; nY1 = psPt[0].y;
  this->line(nX0,nY0,nX1,nY1,nCol);

}

// Filling a quadrilateral is done by breaking it down into
// two filled triangles sharing one side. We have to be careful
// about the triangle fill routine (ie. using rounding) so that
// we can avoid leaving a thin seam between the two triangles.
void DisplayBuffer::filled_quad(Point * psPt, Color nCol) {
  int16_t nX0,nY0,nX1,nY1,nX2,nY2;

  // Break down quadrilateral into two triangles
  nX0 = psPt[0].x; nY0 = psPt[0].y;
  nX1 = psPt[1].x; nY1 = psPt[1].y;
  nX2 = psPt[2].x; nY2 = psPt[2].y;
  this->filled_triangle(nX0,nY0,nX1,nY1,nX2,nY2,nCol);

  nX0 = psPt[2].x; nY0 = psPt[2].y;
  nX1 = psPt[0].x; nY1 = psPt[0].y;
  nX2 = psPt[3].x; nY2 = psPt[3].y;
  this->filled_triangle(nX0,nY0,nX1,nY1,nX2,nY2,nCol);
}

void DisplayBuffer::filled_Sector_(int16_t nQuality, int16_t nMidX, int16_t nMidY, int16_t nRad1, int16_t nRad2,
                                   Color cArcStart, Color cArcEnd, int16_t nAngSecStart, int16_t nAngSecEnd,
                                   bool gradient, int16_t nAngGradStart, int16_t nAngGradRange) {
  Point anPts[4];

  // Calculate degrees per step (based on quality setting)
  int16_t nStepAng = 360 / nQuality;
  int16_t nStep64 = 64 * nStepAng;

  int16_t nAng64;
  int16_t nX, nY;
  int16_t nSegStart, nSegEnd;
  Color colSeg;

  nSegStart = nAngSecStart * (int32_t)nQuality / 360;
  nSegEnd = nAngSecEnd * (int32_t)nQuality / 360;

  int16_t nSegGradStart, nSegGradRange;
  nSegGradStart = nAngGradStart * (int32_t)nQuality / 360;
  nSegGradRange = nAngGradRange * (int32_t)nQuality / 360;
  nSegGradRange = (nSegGradRange == 0) ? 1 : nSegGradRange; // Guard against div/0

  bool bClockwise;
  int16_t nSegInd;
  int16_t nStepCnt;
  if (nSegEnd >= nSegStart) {
    nStepCnt = nSegEnd - nSegStart;
    bClockwise = true;
  } else {
    nStepCnt = nSegStart - nSegEnd;
    bClockwise = false;
  }

  for (int16_t nStepInd = 0; nStepInd < nStepCnt; nStepInd++) {
    // Remap from the step to the segment index, depending on direction
    nSegInd = (bClockwise) ? (nSegStart + nStepInd) : (nSegStart - nStepInd - 1);

    nAng64 = (int32_t)(nSegInd * nStep64) % (int32_t)(360 * 64);

    this->polar_to_point(nRad1, nAng64, &nX, &nY);
    anPts[0] = Point(nMidX + nX, nMidY + nY);
    this->polar_to_point(nRad2, nAng64, &nX, &nY);
    anPts[1] = Point(nMidX + nX, nMidY + nY);
    this->polar_to_point(nRad2, nAng64 + nStep64, &nX, &nY);
    anPts[2] = Point(nMidX + nX, nMidY + nY);
    this->polar_to_point(nRad1, nAng64 + nStep64, &nX, &nY);
    anPts[3] = Point(nMidX + nX, nMidY + nY);

    if (gradient) {
      // Gradient coloring
      int16_t nGradPos = 1000 * (int32_t)(nSegInd - nSegGradStart) / nSegGradRange;
      colSeg = this->blend_color(cArcStart, cArcEnd, 500, nGradPos);
    } else {
      // Flat coloring
      colSeg = cArcStart;
    }
    this->filled_quad( anPts, colSeg);
  }
}

void DisplayBuffer::gradient_sector(int16_t nQuality, int16_t nMidX, int16_t nMidY, int16_t nRad1, int16_t nRad2,
  Color cArcStart, Color cArcEnd, int16_t nAngSecStart, int16_t nAngSecEnd, int16_t nAngGradStart, int16_t nAngGradRange) {
  this->filled_Sector_(nQuality, nMidX, nMidY, nRad1, nRad2, cArcStart, cArcEnd, nAngSecStart, nAngSecEnd,
                       true, nAngGradStart, nAngGradRange);
}

void DisplayBuffer::filled_Sector( int16_t nQuality, int16_t nMidX, int16_t nMidY, int16_t nRad1, int16_t nRad2,
  Color cArc, int16_t nAngSecStart, int16_t nAngSecEnd) {
  this->filled_Sector_(nQuality, nMidX, nMidY, nRad1, nRad2, cArc, cArc, nAngSecStart, nAngSecEnd);
}


#endif


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
Font::Font(const GlyphData *data, int data_nr, int baseline, int bottom) : baseline_(baseline), bottom_(bottom) {
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
  if (frame_index >= (uint32_t)(this->width_ * this->height_ * this->animation_frame_count_))
    return false;
  const uint32_t pos = x + y * width_8 + frame_index;
  return progmem_read_byte(this->data_start_ + (pos / 8u)) & (0x80 >> (pos % 8u));
}
Color Animation::get_color_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return Color::BLACK;
  const uint32_t frame_index = this->width_ * this->height_ * this->current_frame_;
  if (frame_index >= (uint32_t)(this->width_ * this->height_ * this->animation_frame_count_))
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
  if (frame_index >= (uint32_t)(this->width_ * this->height_ * this->animation_frame_count_))
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
  if (frame_index >= (uint32_t)(this->width_ * this->height_ * this->animation_frame_count_))
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
