#pragma once
#include "esphome/components/display/display.h"
#include "esphome/components/display/display_color_utils.h"
#include "esphome/components/image/image.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome::recdisp {

static constexpr int W = 64, H = 64;
static constexpr uint32_t UNTOUCHED = 0xA5A55A5Au;

// Test only: records every draw_pixel_at() color; no bus. report() logs a summary (drawn pixels, pixels with a
// nonzero white channel, SSD1306 "on" pixels, gray4 histogram) and RRGGBBWW rows; ana.py checks them.
class RecDisplay : public display::Display {
 public:
  void update() override {}
  display::DisplayType get_display_type() override { return display::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return W; }
  int get_height_internal() override { return H; }
  void draw_pixel_at(int x, int y, Color c) override {
    if (x < 0 || y < 0 || x >= W || y >= H)
      return;
    this->buf_[y * W + x] = c.raw_32;
  }
  void reset() { std::fill(this->buf_, this->buf_ + W * H, UNTOUCHED); }
  static void hex(char *p, Color c) { snprintf(p, 9, "%02x%02x%02x%02x", c.r, c.g, c.b, c.w); }
  // Summary over the whole buffer, then the region rows.
  void report(const char *name, int x0, int y0, int w, int h) {
    uint32_t drawn = 0, wnz = 0, on = 0, hist[16] = {0};
    for (int i = 0; i < W * H; i++) {
      uint32_t v = this->buf_[i];
      if (v == UNTOUCHED)
        continue;
      Color c;
      c.raw_32 = v;
      drawn++;
      if (c.w)
        wnz++;
      if (c.is_on())  // SSD1306 rule
        on++;
      hist[display::ColorUtil::color_to_grayscale4(c) & 15]++;
    }
    ESP_LOGI("rec", "S %s drawn=%u w=%u on=%u", name, (unsigned) drawn, (unsigned) wnz, (unsigned) on);
    char line[16 * 4 + 8];
    int n = 0;
    for (int k = 0; k < 16; k++)
      n += snprintf(line + n, sizeof(line) - n, "%u,", (unsigned) hist[k]);
    ESP_LOGI("rec", "Hg %s %s", name, line);
    char row[64 * 9 + 4];
    for (int y = y0; y < y0 + h; y++) {
      for (int x = x0; x < x0 + w; x++) {
        uint32_t v = this->buf_[y * W + x];
        if (v == UNTOUCHED) {
          memcpy(row + (x - x0) * 8, "--------", 9);
        } else {
          Color c;
          c.raw_32 = v;
          hex(row + (x - x0) * 8, c);
        }
      }
      ESP_LOGI("rec", "R %s %d %s", name, y - y0, row);
      App.feed_wdt();
    }
  }
  // get_pixel() of an image, and the gray4 level of sample colors
  void dump_get_pixel(const char *name, image::Image *img) {
    char row[64 * 9 + 4];
    for (int y = 0; y < img->get_height(); y++) {
      for (int x = 0; x < img->get_width(); x++)
        hex(row + x * 8, img->get_pixel(x, y, display::COLOR_ON, display::COLOR_OFF));
      ESP_LOGI("rec", "G %s %d %s", name, y, row);
      App.feed_wdt();
    }
  }
  void level(const char *name, Color c) {
    ESP_LOGI("rec", "L %s %02x%02x%02x%02x gray4=%u on=%d", name, c.r, c.g, c.b, c.w,
             (unsigned) display::ColorUtil::color_to_grayscale4(c), c.is_on());
  }
  template<typename F> void time(const char *name, F &&f) {
    uint32_t t[21];
    for (int i = 0; i < 21; i++) {
      uint32_t s = micros();
      f();
      t[i] = micros() - s;
      App.feed_wdt();
    }
    std::sort(t, t + 21);
    ESP_LOGI("rec", "T %s min=%u med=%u max=%u", name, (unsigned) t[0], (unsigned) t[10], (unsigned) t[20]);
  }

 protected:
  uint32_t buf_[W * H];
};

}  // namespace esphome::recdisp
