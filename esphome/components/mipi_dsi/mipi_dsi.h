//
// Created by Clyde Stubbs on 29/10/2023.
//
#pragma once

// only applicable on ESP32-P4
#ifdef USE_ESP32_VARIANT_ESP32P4
#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/gpio.h"

#include "esphome/components/display/display.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"

#include "esp_lcd_mipi_dsi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace esphome::mipi_dsi {

constexpr static const char *const TAG = "display.mipi_dsi";
const uint8_t SW_RESET_CMD = 0x01;
const uint8_t SLEEP_OUT = 0x11;
const uint8_t SDIR_CMD = 0xC7;
const uint8_t MADCTL_CMD = 0x36;
const uint8_t INVERT_OFF = 0x20;
const uint8_t INVERT_ON = 0x21;
const uint8_t DISPLAY_ON = 0x29;
const uint8_t CMD2_BKSEL = 0xFF;
const uint8_t DELAY_FLAG = 0xFF;
const uint8_t MADCTL_BGR = 0x08;
const uint8_t MADCTL_MX = 0x40;
const uint8_t MADCTL_MY = 0x80;
const uint8_t MADCTL_MV = 0x20;     // row/column swap
const uint8_t MADCTL_XFLIP = 0x02;  // Mirror the display horizontally
const uint8_t MADCTL_YFLIP = 0x01;  // Mirror the display vertically

struct MipiDsiCallbackContext {
  SemaphoreHandle_t color_trans_done{};
  SemaphoreHandle_t refresh_done{};
  SemaphoreHandle_t async_flush_done{};
  volatile bool *async_flush_pending{};
};

using AsyncFlushReadyCallback = void (*)(void *);

struct AsyncFlushPerfStats {
  uint32_t flushes{};
  uint32_t zero_copy_flushes{};
  uint32_t staged_flushes{};
  uint32_t done_flushes{};
  uint32_t unsafe_addr_flushes{};
  uint32_t unsafe_row_flushes{};
  uint32_t unsafe_size_flushes{};
  uint64_t staged_bytes{};
  uint64_t sync_us{};
  uint64_t copy_us{};
  uint64_t submit_us{};
  uint64_t done_us{};
  uint32_t sync_max_us{};
  uint32_t copy_max_us{};
  uint32_t submit_max_us{};
  uint32_t done_max_us{};
};

class MipiDsi : public display::Display {
 public:
  MipiDsi(size_t width, size_t height, display::ColorBitness color_depth, uint8_t pixel_mode)
      : width_(width), height_(height), color_depth_(color_depth), pixel_mode_(pixel_mode) {}
  display::ColorOrder get_color_mode() { return this->color_mode_; }
  void set_color_mode(display::ColorOrder color_mode) { this->color_mode_ = color_mode; }
  void set_invert_colors(bool invert_colors) { this->invert_colors_ = invert_colors; }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_enable_pins(std::vector<GPIOPin *> enable_pins) { this->enable_pins_ = std::move(enable_pins); }
  void set_pclk_frequency(float pclk_frequency) { this->pclk_frequency_ = pclk_frequency; }
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  void set_hsync_back_porch(uint16_t hsync_back_porch) { this->hsync_back_porch_ = hsync_back_porch; }
  void set_hsync_front_porch(uint16_t hsync_front_porch) { this->hsync_front_porch_ = hsync_front_porch; }
  void set_hsync_pulse_width(uint16_t hsync_pulse_width) { this->hsync_pulse_width_ = hsync_pulse_width; }
  void set_vsync_pulse_width(uint16_t vsync_pulse_width) { this->vsync_pulse_width_ = vsync_pulse_width; }
  void set_vsync_back_porch(uint16_t vsync_back_porch) { this->vsync_back_porch_ = vsync_back_porch; }
  void set_vsync_front_porch(uint16_t vsync_front_porch) { this->vsync_front_porch_ = vsync_front_porch; }
  void set_init_sequence(const std::vector<uint8_t> &init_sequence) { this->init_sequence_ = init_sequence; }
  void set_model(const char *model) { this->model_ = model; }
  void set_lane_bit_rate(float lane_bit_rate) { this->lane_bit_rate_ = lane_bit_rate; }
  void set_lanes(uint8_t lanes) { this->lanes_ = lanes; }
  void set_use_dma2d(bool use_dma2d) { this->use_dma2d_ = use_dma2d; }
  void set_async_lvgl_flush(bool async_lvgl_flush) { this->async_lvgl_flush_ = async_lvgl_flush; }
  uint8_t *get_frame_buffer() const { return this->frame_buffers_[0]; }
  uint8_t *get_frame_buffer(size_t index) const { return index < 2 ? this->frame_buffers_[index] : nullptr; }
  size_t get_frame_buffer_size() const { return this->width_ * this->height_ * this->get_bytes_per_pixel_(); }
  size_t get_bytes_per_pixel() const { return this->get_bytes_per_pixel_(); }
  bool wait_for_refresh_done(uint32_t timeout_ms = 50);

  void smark_failed(const LogString *message, esp_err_t err);

  void update() override;

  void setup() override;

  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;
  bool draw_pixels_at_async(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                            display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad,
                            AsyncFlushReadyCallback ready_callback, void *ready_arg);
  bool present_frame_buffer(uint8_t *frame_buffer, int y_start, int y_end);
  void consume_async_flush_perf(AsyncFlushPerfStats *stats);

  void draw_pixel_at(int x, int y, Color color) override;
  void fill(Color color) override;
  int get_width() override;
  int get_height() override;

  void dump_config() override;

 protected:
  void write_to_display_(int x_start, int y_start, int w, int h, const uint8_t *ptr, int x_offset, int y_offset,
                         int x_pad);
  void start_async_flush_task_();
  static void async_flush_task_trampoline(void *arg);
  void async_flush_task_();
  bool ensure_async_staging_buffer_(size_t size);
  bool check_buffer_();
  size_t get_bytes_per_pixel_() const { return this->color_depth_ == display::COLOR_BITNESS_888 ? 3 : 2; }
  GPIOPin *reset_pin_{nullptr};
  std::vector<GPIOPin *> enable_pins_{};
  size_t width_{};
  size_t height_{};
  uint16_t hsync_pulse_width_ = 10;
  uint16_t hsync_back_porch_ = 10;
  uint16_t hsync_front_porch_ = 20;
  uint16_t vsync_pulse_width_ = 10;
  uint16_t vsync_back_porch_ = 10;
  uint16_t vsync_front_porch_ = 10;
  const char *model_{"Unknown"};
  std::vector<uint8_t> init_sequence_{};
  float pclk_frequency_ = 16;  // in MHz
  float lane_bit_rate_{1500};  // in Mbps
  uint8_t lanes_{2};           // 1, 2, 3 or 4 lanes
  bool use_dma2d_{true};
  bool async_lvgl_flush_{false};

  bool invert_colors_{};
  display::ColorOrder color_mode_{display::COLOR_ORDER_BGR};
  display::ColorBitness color_depth_;
  uint8_t pixel_mode_{};

  esp_lcd_panel_handle_t handle_{};
  esp_lcd_dsi_bus_handle_t bus_handle_{};
  esp_lcd_panel_io_handle_t io_handle_{};
  SemaphoreHandle_t io_lock_{};
  SemaphoreHandle_t refresh_lock_{};
  SemaphoreHandle_t async_flush_done_{};
  TaskHandle_t async_flush_task_handle_{};
  MipiDsiCallbackContext callback_context_{};
  AsyncFlushReadyCallback async_ready_callback_{};
  void *async_ready_arg_{};
  volatile bool async_flush_pending_{false};
  uint64_t async_transfer_start_us_{0};
  uint8_t *async_staging_buffer_{nullptr};
  size_t async_staging_buffer_size_{0};
  uint32_t async_perf_flushes_{0};
  uint32_t async_perf_zero_copy_flushes_{0};
  uint32_t async_perf_staged_flushes_{0};
  uint32_t async_perf_done_flushes_{0};
  uint32_t async_perf_unsafe_addr_flushes_{0};
  uint32_t async_perf_unsafe_row_flushes_{0};
  uint32_t async_perf_unsafe_size_flushes_{0};
  uint64_t async_perf_staged_bytes_{0};
  uint64_t async_perf_sync_us_{0};
  uint64_t async_perf_copy_us_{0};
  uint64_t async_perf_submit_us_{0};
  uint64_t async_perf_done_us_{0};
  uint32_t async_perf_sync_max_us_{0};
  uint32_t async_perf_copy_max_us_{0};
  uint32_t async_perf_submit_max_us_{0};
  uint32_t async_perf_done_max_us_{0};
  uint8_t *frame_buffers_[2]{nullptr, nullptr};
  uint8_t *buffer_{nullptr};
  uint16_t x_low_{1};
  uint16_t y_low_{1};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
};

}  // namespace esphome::mipi_dsi
#endif
