#pragma once
#ifdef USE_HOST
// Public ESP-IDF boundary double, with separate DMA memory and CPU read view.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#ifndef MIPI_TEST_IDF_MAJOR
#define MIPI_TEST_IDF_MAJOR 5
#endif
#define ESP_IDF_VERSION_VAL(a, b, c) (((a) << 16) | ((b) << 8) | (c))
#ifndef MIPI_TEST_IDF_PATCH
#define MIPI_TEST_IDF_PATCH (MIPI_TEST_IDF_MAJOR >= 6 ? 3 : 6)
#endif
#define ESP_IDF_VERSION \
  ESP_IDF_VERSION_VAL(MIPI_TEST_IDF_MAJOR, (MIPI_TEST_IDF_MAJOR >= 6 ? 0 : 5), MIPI_TEST_IDF_PATCH)
using esp_err_t = int;
static constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 0x102;
inline const char *esp_err_to_name(esp_err_t) { return "test SDK error"; }
using BaseType_t = int;
using TickType_t = uint32_t;
static constexpr int pdFALSE = 0, pdTRUE = 1;
static constexpr TickType_t portMAX_DELAY = UINT32_MAX;
struct TestSemaphore {
  std::mutex mutex;
  std::condition_variable cv;
  bool available = false;
  bool is_mutex = false;
};
using StaticSemaphore_t = TestSemaphore;
using SemaphoreHandle_t = TestSemaphore *;
struct TestDpiPanel;
using esp_lcd_panel_handle_t = TestDpiPanel *;
using esp_lcd_dsi_bus_handle_t = void *;
using esp_lcd_panel_io_handle_t = void *;
struct esp_lcd_dpi_panel_event_data_t {};
struct esp_lcd_dpi_panel_event_callbacks_t {
  bool (*on_color_trans_done)(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t *, void *){};
};
struct esp_lcd_dsi_bus_config_t {
  unsigned bus_id, num_data_lanes;
  float lane_bit_rate_mbps;
};
struct esp_lcd_dbi_io_config_t {
  unsigned virtual_channel, lcd_cmd_bits, lcd_param_bits;
};
static constexpr unsigned LCD_COLOR_PIXEL_FORMAT_RGB565 = 0, LCD_COLOR_PIXEL_FORMAT_RGB888 = 1,
                          LCD_COLOR_FMT_RGB565 = 0, LCD_COLOR_FMT_RGB888 = 1, MIPI_DSI_DPI_CLK_SRC_DEFAULT = 0;
struct esp_lcd_dpi_panel_config_t {
  unsigned virtual_channel, dpi_clk_src;
  float dpi_clock_freq_mhz;
  unsigned pixel_format = 0, in_color_format = 0, num_fbs;
  struct {
    size_t h_size, v_size;
    unsigned hsync_pulse_width, hsync_back_porch, hsync_front_porch, vsync_pulse_width, vsync_back_porch,
        vsync_front_porch;
  } video_timing;
  struct {
    bool use_dma2d;
  } flags;
};
struct TestDpiPanel {
  size_t width{}, height{}, bytes_per_pixel{}, size{};
  alignas(64) std::array<uint8_t, 512> memory{}, cache{};
  esp_lcd_dpi_panel_event_callbacks_t callbacks{};
  void *user_ctx{};
};
using ppa_srm_color_mode_t = int;
enum {
  PPA_SRM_COLOR_MODE_RGB565 = 16,
  PPA_SRM_COLOR_MODE_RGB888,
  PPA_SRM_COLOR_MODE_ARGB8888,
  PPA_SRM_COLOR_MODE_YUV420,
  PPA_SRM_COLOR_MODE_YUV444,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 6)
  PPA_SRM_COLOR_MODE_YUV422_UYVY,
  PPA_SRM_COLOR_MODE_YUV422_VYUY,
  PPA_SRM_COLOR_MODE_YUV422_YUYV,
  PPA_SRM_COLOR_MODE_YUV422_YVYU,
  PPA_SRM_COLOR_MODE_GRAY8
#endif
};
enum ppa_srm_rotation_angle_t {
  PPA_SRM_ROTATION_ANGLE_0,
  PPA_SRM_ROTATION_ANGLE_90,
  PPA_SRM_ROTATION_ANGLE_180,
  PPA_SRM_ROTATION_ANGLE_270
};
static constexpr int PPA_TRANS_MODE_BLOCKING = 0, PPA_TRANS_MODE_NON_BLOCKING = 1, PPA_OPERATION_SRM = 0;
struct ppa_client_config_t {
  int oper_type{};
  unsigned max_pending_trans_num{}, data_burst_length{};
};
struct TestPpaClient {
  TestDpiPanel *panel;
};
using ppa_client_handle_t = TestPpaClient *;
struct PpaIn {
  const void *buffer{};
  uint32_t pic_w{}, pic_h{}, block_w{}, block_h{}, block_offset_x{}, block_offset_y{};
  int srm_cm{}, yuv_range{}, yuv_std{};
};
struct PpaOut {
  void *buffer{};
  size_t buffer_size{};
  uint32_t pic_w{}, pic_h{}, block_offset_x{}, block_offset_y{};
  int srm_cm{}, yuv_range{}, yuv_std{};
};
struct ppa_srm_oper_config_t {
  PpaIn in;
  PpaOut out;
  ppa_srm_rotation_angle_t rotation_angle{};
  float scale_x{}, scale_y{};
  bool mirror_x{}, mirror_y{}, rgb_swap{}, byte_swap{};
  int alpha_update_mode{};
  uint32_t alpha_fix_val{};
  int mode{};
  void *user_data{};
};
namespace mipi_test_sdk {
inline TestDpiPanel *last_panel{};
inline std::vector<TestDpiPanel *> panels;
inline unsigned get_calls{}, cache_calls{}, draw_calls{}, wait_calls{}, blocking_takes{}, try_takes{}, io_waits{},
    ppa_calls{}, no_copy_calls{}, from_isr_gives{}, task_gives{};
inline esp_err_t get_error{}, cache_error{}, draw_error{}, ppa_error{};
inline bool null_frame{}, callback_isr{};
inline thread_local bool inside_isr{};
inline std::function<void()> on_submit, on_ppa;
struct Observed {
  uint32_t in_w{}, in_h{}, block_w{}, block_h{}, out_w{}, out_h{}, x{}, y{};
  int input_cm{}, output_cm{}, rotation{}, mode{};
  float sx{}, sy{};
  bool owner_output{};
};
inline Observed observed;
struct Transfer {
  TestDpiPanel *panel{};
  const uint8_t *source{};
  int x0{}, y0{}, x1{}, y1{};
  bool active{};
};
inline Transfer pending;
inline void callback(TestDpiPanel *p) {
  bool old = inside_isr;
  inside_isr = callback_isr;
  if (p->callbacks.on_color_trans_done)
    p->callbacks.on_color_trans_done(p, nullptr, p->user_ctx);
  inside_isr = old;
}
inline void complete_transfer() {
  assert(pending.active);
  auto t = pending;
  pending.active = false;
  size_t row = (t.x1 - t.x0) * t.panel->bytes_per_pixel;
  for (int y = t.y0; y < t.y1; y++)
    std::memcpy(t.panel->memory.data() + (y * t.panel->width + t.x0) * t.panel->bytes_per_pixel,
                t.source + (y - t.y0) * row, row);
  callback(t.panel);
}
inline void reset_calls() {
  assert(!pending.active);
  get_calls = cache_calls = draw_calls = wait_calls = blocking_takes = try_takes = io_waits = ppa_calls =
      no_copy_calls = from_isr_gives = task_gives = 0;
  get_error = cache_error = draw_error = ppa_error = ESP_OK;
  null_frame = callback_isr = inside_isr = false;
  on_submit = {};
  on_ppa = {};
  observed = {};
}
}  // namespace mipi_test_sdk
inline bool xPortInIsrContext() { return mipi_test_sdk::inside_isr; }
inline SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *s) {
  s->available = false;
  s->is_mutex = false;
  return s;
}
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *s) {
  s->available = true;
  s->is_mutex = true;
  return s;
}
inline SemaphoreHandle_t xSemaphoreCreateBinary() { return xSemaphoreCreateBinaryStatic(new TestSemaphore); }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait) {
  ++mipi_test_sdk::wait_calls;
  if (wait == 0)
    ++mipi_test_sdk::try_takes;
  else {
    ++mipi_test_sdk::blocking_takes;
    if (!s->is_mutex)
      ++mipi_test_sdk::io_waits;
  }
  std::unique_lock lock(s->mutex);
  if (s->available) {
    s->available = false;
    return pdTRUE;
  }
  if (wait == 0)
    return pdFALSE;
  if (!s->is_mutex) {
    lock.unlock();
    assert(mipi_test_sdk::pending.active);
    mipi_test_sdk::complete_transfer();
    lock.lock();
  }
  s->cv.wait(lock, [&] { return s->available; });
  s->available = false;
  return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t s) {
  assert(!xPortInIsrContext());
  {
    std::lock_guard lock(s->mutex);
    s->available = true;
  }
  s->cv.notify_one();
  ++mipi_test_sdk::task_gives;
}
inline void xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t *y) {
  assert(xPortInIsrContext());
  {
    std::lock_guard lock(s->mutex);
    s->available = true;
  }
  s->cv.notify_one();
  *y = pdFALSE;
  ++mipi_test_sdk::from_isr_gives;
}
inline esp_err_t esp_lcd_new_dsi_bus(const esp_lcd_dsi_bus_config_t *, esp_lcd_dsi_bus_handle_t *h) {
  *h = reinterpret_cast<void *>(1);
  return ESP_OK;
}
inline esp_err_t esp_lcd_new_panel_io_dbi(esp_lcd_dsi_bus_handle_t, const esp_lcd_dbi_io_config_t *,
                                          esp_lcd_panel_io_handle_t *h) {
  *h = reinterpret_cast<void *>(2);
  return ESP_OK;
}
inline esp_err_t esp_lcd_new_panel_dpi(esp_lcd_dsi_bus_handle_t, const esp_lcd_dpi_panel_config_t *c,
                                       esp_lcd_panel_handle_t *h) {
  assert(c->num_fbs == 1);
  auto *p = new TestDpiPanel;
  p->width = c->video_timing.h_size;
  p->height = c->video_timing.v_size;
  const auto format = MIPI_TEST_IDF_MAJOR >= 6 ? c->in_color_format : c->pixel_format;
  p->bytes_per_pixel = format == LCD_COLOR_PIXEL_FORMAT_RGB565 ? 2 : 3;
  p->size = p->width * p->height * p->bytes_per_pixel;
  if (p->size > p->memory.size()) {
    delete p;
    return ESP_ERR_INVALID_ARG;
  }
  mipi_test_sdk::last_panel = p;
  mipi_test_sdk::panels.push_back(p);
  *h = p;
  return ESP_OK;
}
inline esp_err_t esp_lcd_dpi_panel_enable_dma2d(esp_lcd_panel_handle_t) { return ESP_OK; }
inline esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t) { return ESP_OK; }
inline esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t, int, const void *, size_t) { return ESP_OK; }
inline esp_err_t esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t p,
                                                            const esp_lcd_dpi_panel_event_callbacks_t *c, void *ctx) {
  p->callbacks = *c;
  p->user_ctx = ctx;
  return ESP_OK;
}
inline esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t p, int x0, int y0, int x1, int y1,
                                           const void *source) {
  assert(!mipi_test_sdk::pending.active);
  assert(x0 >= 0 && y0 >= 0 && x1 <= int(p->width) && y1 <= int(p->height));
  ++mipi_test_sdk::draw_calls;
  if (mipi_test_sdk::draw_error != ESP_OK)
    return mipi_test_sdk::draw_error;
  if (mipi_test_sdk::on_submit)
    mipi_test_sdk::on_submit();
  auto a = reinterpret_cast<uintptr_t>(source), b = reinterpret_cast<uintptr_t>(p->cache.data());
  if (a >= b && a < b + p->size) {
    ++mipi_test_sdk::no_copy_calls;
    mipi_test_sdk::callback(p);
  } else
    mipi_test_sdk::pending = {p, static_cast<const uint8_t *>(source), x0, y0, x1, y1, true};
  return ESP_OK;
}
inline esp_err_t esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t p, uint32_t n, void **out, ...) {
  assert(n == 1);
  ++mipi_test_sdk::get_calls;
  if (mipi_test_sdk::get_error != ESP_OK)
    return mipi_test_sdk::get_error;
  *out = mipi_test_sdk::null_frame ? nullptr : p->cache.data();
  return ESP_OK;
}
static constexpr int ESP_CACHE_MSYNC_FLAG_DIR_M2C = 8, ESP_CACHE_MSYNC_FLAG_UNALIGNED = 2;
inline esp_err_t esp_cache_msync(void *address, size_t size, int flags) {
  ++mipi_test_sdk::cache_calls;
  assert(!mipi_test_sdk::pending.active);
  if (flags != (ESP_CACHE_MSYNC_FLAG_DIR_M2C))
    return ESP_ERR_INVALID_ARG;
  if (mipi_test_sdk::cache_error != ESP_OK)
    return mipi_test_sdk::cache_error;
  for (auto *p : mipi_test_sdk::panels)
    if (address == p->cache.data() && size == p->size) {
      std::memcpy(p->cache.data(), p->memory.data(), size);
      return ESP_OK;
    }
  return ESP_ERR_INVALID_ARG;
}
inline esp_err_t ppa_register_client(const ppa_client_config_t *c, ppa_client_handle_t *h) {
  assert(c->oper_type == PPA_OPERATION_SRM);
  *h = new TestPpaClient{mipi_test_sdk::last_panel};
  return ESP_OK;
}
inline esp_err_t ppa_do_scale_rotate_mirror(ppa_client_handle_t client, const ppa_srm_oper_config_t *c) {
  ++mipi_test_sdk::ppa_calls;
  auto *p = client->panel;
  mipi_test_sdk::observed = {c->in.pic_w,
                             c->in.pic_h,
                             c->in.block_w,
                             c->in.block_h,
                             c->out.pic_w,
                             c->out.pic_h,
                             c->out.block_offset_x,
                             c->out.block_offset_y,
                             c->in.srm_cm,
                             c->out.srm_cm,
                             c->rotation_angle,
                             c->mode,
                             c->scale_x,
                             c->scale_y,
                             c->out.buffer == p->cache.data()};
  if (mipi_test_sdk::on_ppa)
    mipi_test_sdk::on_ppa();
  if (mipi_test_sdk::ppa_error != ESP_OK)
    return mipi_test_sdk::ppa_error;
  assert(c->mode == PPA_TRANS_MODE_BLOCKING && c->out.buffer == p->cache.data() &&
         c->out.srm_cm == PPA_SRM_COLOR_MODE_RGB565);
  unsigned w = c->in.block_w * c->scale_x, h = c->in.block_h * c->scale_y;
  for (unsigned y = 0; y < h; y++)
    for (unsigned x = 0; x < w; x++) {
      unsigned ox = x, oy = y;
      switch (c->rotation_angle) {
        case PPA_SRM_ROTATION_ANGLE_90:
          ox = y;
          oy = w - 1 - x;
          break;
        case PPA_SRM_ROTATION_ANGLE_180:
          ox = w - 1 - x;
          oy = h - 1 - y;
          break;
        case PPA_SRM_ROTATION_ANGLE_270:
          ox = h - 1 - y;
          oy = x;
          break;
        default:
          break;
      }
      ox += c->out.block_offset_x;
      oy += c->out.block_offset_y;
      assert(ox < p->width && oy < p->height);
      uint16_t value = 0x1234;
      if (c->in.srm_cm == PPA_SRM_COLOR_MODE_RGB565) {
        unsigned sx = c->in.block_offset_x + unsigned(x / c->scale_x),
                 sy = c->in.block_offset_y + unsigned(y / c->scale_y);
        std::memcpy(&value, static_cast<const uint8_t *>(c->in.buffer) + (sy * c->in.pic_w + sx) * 2, 2);
      }
      std::memcpy(p->memory.data() + (oy * p->width + ox) * 2, &value, 2);
    }
  std::memcpy(p->cache.data(), p->memory.data(), p->size);
  return ESP_OK;
}
inline void axi_icm_ll_set_dw_gdma_qos_arbiter_prio(unsigned, unsigned, unsigned) {}

#endif  // USE_HOST
