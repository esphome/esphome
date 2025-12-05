#include "hub75_component.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32

namespace esphome::hub75 {

static const char *const TAG = "hub75";

// ========================================
// Constructor
// ========================================

HUB75Display::HUB75Display(const Hub75Config &config) : config_(config) {
  // Initialize runtime state from config
  this->brightness_ = config.brightness;
  this->enabled_ = (config.brightness > 0);
}

// ========================================
// Core Component methods
// ========================================

void HUB75Display::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HUB75Display...");

  // Create driver with pre-configured config
  driver_ = new Hub75Driver(config_);
  if (!driver_->begin()) {
    ESP_LOGE(TAG, "Failed to initialize HUB75 driver!");
    return;
  }

  this->enabled_ = true;
}

void HUB75Display::dump_config() {
  LOG_DISPLAY("", "HUB75", this);

  ESP_LOGCONFIG(TAG,
                "  Panel: %dx%d pixels\n"
                "  Layout: %dx%d panels\n"
                "  Virtual Display: %dx%d pixels",
                config_.panel_width, config_.panel_height, config_.layout_cols, config_.layout_rows,
                config_.panel_width * config_.layout_cols, config_.panel_height * config_.layout_rows);

  ESP_LOGCONFIG(TAG,
                "  Scan Wiring: %d\n"
                "  Shift Driver: %d",
                static_cast<int>(config_.scan_wiring), static_cast<int>(config_.shift_driver));

  ESP_LOGCONFIG(TAG,
                "  Pins: R1:%i, G1:%i, B1:%i, R2:%i, G2:%i, B2:%i\n"
                "  Pins: A:%i, B:%i, C:%i, D:%i, E:%i\n"
                "  Pins: LAT:%i, OE:%i, CLK:%i",
                config_.pins.r1, config_.pins.g1, config_.pins.b1, config_.pins.r2, config_.pins.g2, config_.pins.b2,
                config_.pins.a, config_.pins.b, config_.pins.c, config_.pins.d, config_.pins.e, config_.pins.lat,
                config_.pins.oe, config_.pins.clk);

  ESP_LOGCONFIG(TAG,
                "  Clock Speed: %u MHz\n"
                "  Latch Blanking: %i\n"
                "  Clock Phase: %s\n"
                "  Min Refresh Rate: %i Hz\n"
                "  Bit Depth: %i\n"
                "  Double Buffer: %s",
                static_cast<uint32_t>(config_.output_clock_speed) / 1000000, config_.latch_blanking,
                TRUEFALSE(config_.clk_phase_inverted), config_.min_refresh_rate, HUB75_BIT_DEPTH,
                YESNO(config_.double_buffer));
}

// ========================================
// Display/PollingComponent methods
// ========================================

void HUB75Display::update() {
  if (!driver_) [[unlikely]]
    return;
  if (!this->enabled_) [[unlikely]]
    return;

  this->do_update_();

  if (config_.double_buffer) {
    driver_->flip_buffer();
  }
}

void HUB75Display::fill(Color color) {
  if (!driver_) [[unlikely]]
    return;
  if (!this->enabled_) [[unlikely]]
    return;

  // Special case: black (off) - use fast hardware clear
  if (!color.is_on()) {
    driver_->clear();
    return;
  }

  // For non-black colors, fall back to base class (pixel-by-pixel)
  Display::fill(color);
}

void HOT HUB75Display::draw_pixel_at(int x, int y, Color color) {
  if (!driver_) [[unlikely]]
    return;
  if (!this->enabled_) [[unlikely]]
    return;

  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) [[unlikely]]
    return;

  driver_->set_pixel(x, y, color.r, color.g, color.b);
  App.feed_wdt();
}

void HOT HUB75Display::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                                      ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (!driver_) [[unlikely]]
    return;
  if (!this->enabled_) [[unlikely]]
    return;

  // Map ESPHome enums to hub75 enums
  Hub75PixelFormat format;
  Hub75ColorOrder color_order = Hub75ColorOrder::RGB;
  int bytes_per_pixel;

  // Determine format based on bitness
  if (bitness == ColorBitness::COLOR_BITNESS_565) {
    format = Hub75PixelFormat::RGB565;
    bytes_per_pixel = 2;
  } else if (bitness == ColorBitness::COLOR_BITNESS_888) {
#ifdef USE_LVGL
#if LV_COLOR_DEPTH == 32
    // 32-bit: 4 bytes per pixel with padding byte (LVGL mode)
    format = Hub75PixelFormat::RGB888_32;
    bytes_per_pixel = 4;

    // Map ESPHome ColorOrder to Hub75ColorOrder
    // ESPHome ColorOrder is typically BGR for little-endian 32-bit
    color_order = (order == ColorOrder::COLOR_ORDER_RGB) ? Hub75ColorOrder::RGB : Hub75ColorOrder::BGR;
#elif LV_COLOR_DEPTH == 24
    // 24-bit: 3 bytes per pixel, tightly packed
    format = Hub75PixelFormat::RGB888;
    bytes_per_pixel = 3;
    // Note: 24-bit is always RGB order in LVGL
#else
    ESP_LOGE(TAG, "Unsupported LV_COLOR_DEPTH: %d", LV_COLOR_DEPTH);
    return;
#endif
#else
    // Non-LVGL mode: standard 24-bit RGB888
    format = Hub75PixelFormat::RGB888;
    bytes_per_pixel = 3;
    color_order = (order == ColorOrder::COLOR_ORDER_RGB) ? Hub75ColorOrder::RGB : Hub75ColorOrder::BGR;
#endif
  } else {
    ESP_LOGE(TAG, "Unsupported bitness: %d", static_cast<int>(bitness));
    return;
  }

  // Check if buffer is tightly packed (no stride)
  const int stride_px = x_offset + w + x_pad;
  const bool is_packed = (x_offset == 0 && x_pad == 0 && y_offset == 0);

  if (is_packed) {
    // Tightly packed buffer - single bulk call for best performance
    driver_->draw_pixels(x_start, y_start, w, h, ptr, format, color_order, big_endian);
  } else {
    // Buffer has stride (padding between rows) - draw row by row
    for (int yy = 0; yy < h; ++yy) {
      const size_t row_offset = ((y_offset + yy) * stride_px + x_offset) * bytes_per_pixel;
      const uint8_t *row_ptr = ptr + row_offset;

      driver_->draw_pixels(x_start, y_start + yy, w, 1, row_ptr, format, color_order, big_endian);
    }
  }
}

void HUB75Display::set_brightness(int brightness) {
  this->brightness_ = brightness;
  this->enabled_ = (brightness > 0);
  if (this->driver_ != nullptr) {
    this->driver_->set_brightness(brightness);
  }
}

}  // namespace esphome::hub75

#endif
