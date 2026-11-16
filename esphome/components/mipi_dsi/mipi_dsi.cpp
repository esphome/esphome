#ifdef USE_ESP32_VARIANT_ESP32P4
#include "mipi_dsi.h"
#include "esphome/core/helpers.h"
#include <utility>

#include <hal/axi_icm_ll.h>

#include "esp_cache.h"
#include "esp_idf_version.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace esphome::mipi_dsi {

// Maximum bytes to log for init commands (truncated if larger)
static constexpr size_t MIPI_DSI_MAX_CMD_LOG_BYTES = 64;
// Highest AXI QoS priority (4 bits); all masters start at 0
static constexpr uint32_t DSI_DMA_READ_PRIORITY = 15;

// With CONFIG_LCD_DSI_ISR_CACHE_SAFE the DPI driver only accepts callbacks in
// IRAM
#if CONFIG_LCD_DSI_ISR_CACHE_SAFE
#define MIPI_DSI_ISR_ATTR IRAM_ATTR
#else
#define MIPI_DSI_ISR_ATTR
#endif

static bool MIPI_DSI_ISR_ATTR notify_refresh_ready(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata,
                                                   void *user_ctx) {
  SemaphoreHandle_t sem = static_cast<SemaphoreHandle_t>(user_ctx);
  if (xPortInIsrContext()) {
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(sem, &need_yield);
    return need_yield == pdTRUE;
  }
  xSemaphoreGive(sem);
  return false;
}

void MipiDsi::smark_failed(const LogString *message, esp_err_t err) {
  ESP_LOGE(TAG, "%s: %s", LOG_STR_ARG(message), esp_err_to_name(err));
  this->mark_failed(message);
}

void MipiDsi::setup() {
  ESP_LOGCONFIG(TAG, "Running Setup");

  if (!this->enable_pins_.empty()) {
    for (auto *pin : this->enable_pins_) {
      pin->setup();
      pin->digital_write(true);
    }
    delay(10);
  }

  esp_lcd_dsi_bus_config_t bus_config = {
      .bus_id = 0,                     // index from 0, specify the DSI host to use
      .num_data_lanes = this->lanes_,  // Number of data lanes to use, can't set a value that
                                       // exceeds the chip's capability
      // phy_clk_src left at 0 to enable runtime auto-select.
      .lane_bit_rate_mbps = this->lane_bit_rate_,  // Bit rate of the data lanes, in Mbps
  };
  auto err = esp_lcd_new_dsi_bus(&bus_config, &this->bus_handle_);
  if (err != ESP_OK) {
    this->smark_failed(LOG_STR("lcd_new_dsi_bus failed"), err);
    return;
  }
  esp_lcd_dbi_io_config_t dbi_config = {
      .virtual_channel = 0,
      .lcd_cmd_bits = 8,    // according to the LCD spec
      .lcd_param_bits = 8,  // according to the LCD spec
  };
  err = esp_lcd_new_panel_io_dbi(this->bus_handle_, &dbi_config, &this->io_handle_);
  if (err != ESP_OK) {
    this->smark_failed(LOG_STR("new_panel_io_dbi failed"), err);
    return;
  }
  // clang-format off
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  auto color_format = LCD_COLOR_FMT_RGB565;
  if (this->color_depth_ == display::COLOR_BITNESS_888) {
    color_format = LCD_COLOR_FMT_RGB888;
  }
  esp_lcd_dpi_panel_config_t dpi_config = {.virtual_channel = 0,
                                           .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
                                           .dpi_clock_freq_mhz = this->pclk_frequency_,
                                           .in_color_format = color_format,
#else
  auto pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
  if (this->color_depth_ == display::COLOR_BITNESS_888) {
    pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB888;
  }
  esp_lcd_dpi_panel_config_t dpi_config = {.virtual_channel = 0,
                                           .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
                                           .dpi_clock_freq_mhz = this->pclk_frequency_,
                                           .pixel_format = pixel_format,
#endif
                                           .num_fbs = 1,  // number of frame buffers to allocate
                                           .video_timing =
                                               {
                                                   .h_size = this->width_,
                                                   .v_size = this->height_,
                                                   .hsync_pulse_width = this->hsync_pulse_width_,
                                                   .hsync_back_porch = this->hsync_back_porch_,
                                                   .hsync_front_porch = this->hsync_front_porch_,
                                                   .vsync_pulse_width = this->vsync_pulse_width_,
                                                   .vsync_back_porch = this->vsync_back_porch_,
                                                   .vsync_front_porch = this->vsync_front_porch_,
                                               },
                                           .flags = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
                                               .use_dma2d = true,
#endif
                                           }};
  // clang-format on
  err = esp_lcd_new_panel_dpi(this->bus_handle_, &dpi_config, &this->handle_);
  if (err != ESP_OK) {
    this->smark_failed(LOG_STR("esp_lcd_new_panel_dpi failed"), err);
    return;
  }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  err = esp_lcd_dpi_panel_enable_dma2d(this->handle_);
  if (err != ESP_OK) {
    this->smark_failed(LOG_STR("esp_lcd_dpi_panel_enable_dma2d failed"), err);
    return;
  }
#endif
  if (this->reset_pin_ != nullptr) {
    pulse_reset(this->reset_pin_, {.high_before_ms = 5, .low_ms = 5});
  } else {
    esp_lcd_panel_io_tx_param(this->io_handle_, SW_RESET_CMD, nullptr, 0);
  }
  // need to know when the display is ready for SLPOUT command - will be 120ms
  // after reset
  auto when = millis() + 120;
  err = esp_lcd_panel_init(this->handle_);
  if (err != ESP_OK) {
    this->smark_failed(LOG_STR("esp_lcd_init failed"), err);
    return;
  }
  size_t index = 0;
  const uint8_t *seq = this->init_sequence_;
  const size_t len = this->init_sequence_len_;
  while (index != len) {
    if (len - index < 2) {
      this->mark_failed(LOG_STR("Malformed init sequence"));
      return;
    }
    uint8_t cmd = seq[index++];
    uint8_t x = seq[index++];
    if (x == DELAY_FLAG) {
      ESP_LOGD(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      uint8_t num_args = x & 0x7F;
      if (len - index < num_args) {
        this->mark_failed(LOG_STR("Malformed init sequence"));
        return;
      }
      if (cmd == SLEEP_OUT) {
        // are we ready, boots?
        int duration = when - millis();
        if (duration > 0) {
          delay(duration);
        }
      }
      const auto *ptr = seq + index;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
      char hex_buf[format_hex_pretty_size(MIPI_DSI_MAX_CMD_LOG_BYTES)];
#endif
      ESP_LOGVV(TAG, "Command %02X, length %d, byte(s) %s", cmd, num_args,
                format_hex_pretty_to(hex_buf, ptr, num_args, '.'));
      err = esp_lcd_panel_io_tx_param(this->io_handle_, cmd, ptr, num_args);
      if (err != ESP_OK) {
        this->smark_failed(LOG_STR("lcd_panel_io_tx_param failed"), err);
        return;
      }
      index += num_args;
      if (cmd == SLEEP_OUT)
        delay(10);
    }
  }
  void *frame_buffer = nullptr;
  err = esp_lcd_dpi_panel_get_frame_buffer(this->handle_, 1, &frame_buffer);
  if (err != ESP_OK || frame_buffer == nullptr) {
    this->smark_failed(LOG_STR("Failed to obtain DPI framebuffer"), err);
    return;
  }
  this->frame_buffer_ = static_cast<uint8_t *>(frame_buffer);
  this->frame_buffer_size_ = this->width_ * this->height_ * (3 - this->color_depth_);
  this->io_lock_ = xSemaphoreCreateBinaryStatic(&this->io_lock_storage_);
  this->submit_mutex_ = xSemaphoreCreateMutexStatic(&this->submit_mutex_storage_);
  if (this->io_lock_ == nullptr || this->submit_mutex_ == nullptr) {
    this->mark_failed(LOG_STR("Failed to create display transfer locks"));
    return;
  }
  esp_lcd_dpi_panel_event_callbacks_t cbs = {
      .on_color_trans_done = notify_refresh_ready,
  };

  err = (esp_lcd_dpi_panel_register_event_callbacks(this->handle_, &cbs, this->io_lock_));
  if (err != ESP_OK) {
    this->smark_failed(LOG_STR("Failed to register callbacks"), err);
    return;
  }

  // The panel reads its frame buffer from PSRAM all the time through DW-GDMA.
  // With all AXI masters at the same priority, heavy PSRAM use by the CPU or
  // the PPA can hold those reads up until the panel runs out of data and shows
  // a corrupt frame. Raise the read priority on both DW-GDMA master ports; the
  // panel does not write.
  axi_icm_ll_set_dw_gdma_qos_arbiter_prio(0, 0, DSI_DMA_READ_PRIORITY);
  axi_icm_ll_set_dw_gdma_qos_arbiter_prio(1, 0, DSI_DMA_READ_PRIORITY);

#ifdef USE_MIPI_DSI_RGB565_OWNED_TRANSFORM
  if (this->color_depth_ == display::COLOR_BITNESS_565 && !this->big_endian_) {
    // DPI allocates this buffer; never export it or borrow a producer's output.
    const size_t line = CONFIG_CACHE_L2_CACHE_LINE_SIZE;
    if (line != 0 && reinterpret_cast<uintptr_t>(this->frame_buffer_) % line == 0 &&
        this->frame_buffer_size_ % line == 0) {
      ppa_client_config_t config{};
      config.oper_type = PPA_OPERATION_SRM;
      config.max_pending_trans_num = 1;
      if (ppa_register_client(&config, &this->transform_client_) != ESP_OK) {
        this->transform_client_ = nullptr;
        ESP_LOGW(TAG, "PPA transform unavailable; display compatibility path retained");
      }
    }
  }
#endif
  ESP_LOGCONFIG(TAG, "MIPI DSI setup complete");
}

bool MipiDsi::supports_rgb565_transform() const {
#ifdef USE_MIPI_DSI_RGB565_OWNED_TRANSFORM
  return !this->is_failed() && this->color_depth_ == display::COLOR_BITNESS_565 && !this->big_endian_ &&
         this->frame_buffer_ != nullptr && this->frame_buffer_size_ == this->width_ * this->height_ * 2 &&
         this->transform_client_ != nullptr;
#else
  return false;
#endif
}

void MipiDsi::mirror_native_rgb565_(int x, int y, int width, int height, const uint8_t *data, size_t stride) {
  if (this->buffer_ == nullptr)
    return;
  for (int row = 0; row < height; ++row) {
    const size_t offset = (size_t(y + row) * this->width_ + x) * sizeof(uint16_t);
    std::memcpy(this->buffer_ + offset, data + row * stride, width * sizeof(uint16_t));
  }
}

bool MipiDsi::submit_bitmap_(int x_start, int y_start, int x_end, int y_end, const uint8_t *ptr) {
  if (this->is_failed() || this->handle_ == nullptr || this->io_lock_ == nullptr || this->submit_mutex_ == nullptr) {
    return false;
  }
  // esp_lcd exposes one panel transfer completion callback. Serialize the
  // complete submit-and-wait transaction so LVGL and direct video cannot
  // consume each other's completion token.
  xSemaphoreTake(this->submit_mutex_, portMAX_DELAY);
  const bool success = this->submit_bitmap_locked_(x_start, y_start, x_end, y_end, ptr);
  xSemaphoreGive(this->submit_mutex_);
  return success;
}

bool MipiDsi::submit_bitmap_locked_(int x_start, int y_start, int x_end, int y_end, const uint8_t *ptr) {
  // A binary semaphore may still contain a completion edge from setup or a
  // prior transfer. Drain it before submitting so this transaction can only
  // consume the callback generated by its own DMA operation.
  xSemaphoreTake(this->io_lock_, 0);
  const esp_err_t err = esp_lcd_panel_draw_bitmap(this->handle_, x_start, y_start, x_end, y_end, ptr);
  if (err != ESP_OK) {
    this->smark_failed(LOG_STR("lcd_panel_draw_bitmap failed"), err);
    return false;
  }
  // A successful submit owns the caller's buffer until the transfer callback.
  // Waiting without a timeout preserves that ownership contract. The error
  // path above must return before waiting because no callback is guaranteed
  // when the driver rejects the submit.
  xSemaphoreTake(this->io_lock_, portMAX_DELAY);
  return true;
}

bool MipiDsi::copy_frame_buffer_rgb565(uint8_t *target, size_t capacity, display::DisplayRotation rotation) {
  if (in_isr_context() || !is_main_loop_thread() || this->updating_ ||
      (this->buffer_ != nullptr && this->x_low_ <= this->x_high_ && this->y_low_ <= this->y_high_))
    return false;
  const size_t required = this->width_ * this->height_ * sizeof(uint16_t);
  if ((rotation != display::DISPLAY_ROTATION_0_DEGREES && rotation != display::DISPLAY_ROTATION_90_DEGREES &&
       rotation != display::DISPLAY_ROTATION_180_DEGREES && rotation != display::DISPLAY_ROTATION_270_DEGREES) ||
      this->is_failed() || this->color_depth_ != display::COLOR_BITNESS_565 || target == nullptr ||
      reinterpret_cast<uintptr_t>(target) % alignof(uint16_t) != 0 || capacity < required ||
      this->frame_buffer_ == nullptr || this->frame_buffer_size_ != required || this->submit_mutex_ == nullptr ||
      xSemaphoreTake(this->submit_mutex_, 0) != pdTRUE)
    return false;
  const uintptr_t source_address = reinterpret_cast<uintptr_t>(this->frame_buffer_);
  const uintptr_t target_address = reinterpret_cast<uintptr_t>(target);
  if ((target_address >= source_address && target_address - source_address < required) ||
      (source_address > target_address && source_address - target_address < required)) {
    xSemaphoreGive(this->submit_mutex_);
    return false;
  }
  // Every panel writer already holds submit_mutex_ until its DMA completes.
  // Reading after M2C synchronization sees the final composed panel buffer,
  // including direct video, without exposing or borrowing a live pointer.
  const bool synced = esp_cache_msync(this->frame_buffer_, required, ESP_CACHE_MSYNC_FLAG_DIR_M2C) == ESP_OK;
  if (synced) {
    const auto *source = reinterpret_cast<const uint16_t *>(this->frame_buffer_);
    auto *output = reinterpret_cast<uint16_t *>(target);
    // Native-to-logical rotation is the inverse of the driver's mapping.
    // Compose it with the requested clockwise output rotation once.
    const unsigned output_rotation =
        (360U + static_cast<unsigned>(rotation) - static_cast<unsigned>(this->rotation_)) % 360U;
    const size_t pixels = required / sizeof(uint16_t);
    const bool byte_swap = this->big_endian_;
    if (output_rotation == 0) {
      if (!byte_swap) {
        std::memcpy(output, source, required);
      } else {
        for (size_t i = 0; i < pixels; ++i)
          output[i] = convert_big_endian(source[i]);
      }
    } else if (output_rotation == 180) {
      for (size_t i = 0; i < pixels; ++i) {
        const uint16_t pixel = source[pixels - i - 1];
        output[i] = byte_swap ? convert_big_endian(pixel) : pixel;
      }
    } else {
      const size_t output_width = this->height_;
      const size_t output_height = this->width_;
      const ptrdiff_t native_width = static_cast<ptrdiff_t>(this->width_);
      const ptrdiff_t source_origin =
          output_rotation == 90 ? (static_cast<ptrdiff_t>(this->height_) - 1) * native_width : native_width - 1;
      const ptrdiff_t source_x_step = output_rotation == 90 ? -native_width : native_width;
      const ptrdiff_t source_y_step = output_rotation == 90 ? 1 : -1;
      // A 32-pixel RGB565 span occupies one P4 cache line. Tiles reuse both
      // source and destination lines instead of walking an entire strided
      // PSRAM column before returning to its adjacent pixels. No scratch copy.
      static constexpr size_t TILE_SIZE = 32;
      for (size_t top = 0; top < output_height; top += TILE_SIZE) {
        const size_t bottom = std::min(top + TILE_SIZE, output_height);
        for (size_t left = 0; left < output_width; left += TILE_SIZE) {
          const size_t right = std::min(left + TILE_SIZE, output_width);
          for (size_t y = top; y < bottom; ++y) {
            ptrdiff_t source_index = source_origin + static_cast<ptrdiff_t>(y) * source_y_step +
                                     static_cast<ptrdiff_t>(left) * source_x_step;
            auto *row = output + y * output_width + left;
            for (size_t x = left; x < right; ++x) {
              const uint16_t pixel = source[source_index];
              *row++ = byte_swap ? convert_big_endian(pixel) : pixel;
              // Keep the stride as an integer: the last increment may be
              // outside the image, where forming a source pointer is invalid.
              source_index += source_x_step;
            }
          }
        }
      }
    }
  }
  xSemaphoreGive(this->submit_mutex_);
  return synced;
}

#ifdef USE_MIPI_DSI_RGB565_OWNED_TRANSFORM
static uint32_t ppa_input_bits(ppa_srm_color_mode_t mode) {
  // These public formats are present in the minimum IDF 5.5 SDK as well as current SDKs.
  switch (mode) {
    case PPA_SRM_COLOR_MODE_RGB565:
      return 16;
    case PPA_SRM_COLOR_MODE_RGB888:
    case PPA_SRM_COLOR_MODE_YUV444:
      return 24;
    case PPA_SRM_COLOR_MODE_ARGB8888:
      return 32;
    case PPA_SRM_COLOR_MODE_YUV420:
      return 12;
    default:
      return 0;
  }
}

#endif

bool MipiDsi::draw_rgb565_transformed(const ppa_srm_oper_config_t &operation, size_t input_bytes, int logical_x,
                                      int logical_y, int logical_width, int logical_height,
                                      WriteAdmission still_current, void *context) {
#ifdef USE_MIPI_DSI_RGB565_OWNED_TRANSFORM
  const auto &input = operation.in;
  const uint32_t bits = ppa_input_bits(input.srm_cm);
  if (!this->supports_rgb565_transform() || this->submit_mutex_ == nullptr || this->io_lock_ == nullptr ||
      this->handle_ == nullptr || input.buffer == nullptr || input.pic_w == 0 || input.pic_h == 0 || bits == 0 ||
      input_bytes > UINT32_MAX || uint64_t(input.pic_w) * input.pic_h > uint64_t(input_bytes) * 8 / bits ||
      input.block_w == 0 || input.block_h == 0 || input.block_offset_x > input.pic_w ||
      input.block_offset_y > input.pic_h || input.block_w > input.pic_w - input.block_offset_x ||
      input.block_h > input.pic_h - input.block_offset_y || operation.rotation_angle != PPA_SRM_ROTATION_ANGLE_0 ||
      logical_x < 0 || logical_y < 0 || logical_width <= 0 || logical_height <= 0 || logical_x > this->get_width() ||
      logical_y > this->get_height() || logical_width > this->get_width() - logical_x ||
      logical_height > this->get_height() - logical_y)
    return false;

  const auto scale_units = [](float scale) -> uint32_t {
    const float units = scale * 16.0f;
    return std::isfinite(units) && units >= 1.0f && units <= 4095.0f && std::floor(units) == units
               ? static_cast<uint32_t>(units)
               : 0;
  };
  const uint32_t scale_x = scale_units(operation.scale_x), scale_y = scale_units(operation.scale_y);
  if (scale_x == 0 || scale_y == 0 || uint64_t(input.block_w) * scale_x / 16 != size_t(logical_width) ||
      uint64_t(input.block_h) * scale_y / 16 != size_t(logical_height))
    return false;
  const uintptr_t source = reinterpret_cast<uintptr_t>(input.buffer);
  const uintptr_t target = reinterpret_cast<uintptr_t>(this->frame_buffer_);
  if (input_bytes > UINTPTR_MAX - source ||
      (source < target + this->frame_buffer_size_ && target < source + input_bytes))
    return false;

  int native_x = logical_x, native_y = logical_y, output_width = logical_width, output_height = logical_height;
  auto config = operation;
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_90_DEGREES:
      native_x = this->width_ - logical_y - logical_height;
      native_y = logical_x;
      std::swap(output_width, output_height);
      config.rotation_angle = PPA_SRM_ROTATION_ANGLE_270;
      break;
    case display::DISPLAY_ROTATION_180_DEGREES:
      native_x = this->width_ - logical_x - logical_width;
      native_y = this->height_ - logical_y - logical_height;
      config.rotation_angle = PPA_SRM_ROTATION_ANGLE_180;
      break;
    case display::DISPLAY_ROTATION_270_DEGREES:
      native_x = logical_y;
      native_y = this->height_ - logical_x - logical_width;
      std::swap(output_width, output_height);
      config.rotation_angle = PPA_SRM_ROTATION_ANGLE_90;
      break;
    default:
      break;
  }
  config.out = {};
  config.out.buffer = this->frame_buffer_;
  config.out.buffer_size = this->frame_buffer_size_;
  config.out.pic_w = this->width_;
  config.out.pic_h = this->height_;
  config.out.block_offset_x = native_x;
  config.out.block_offset_y = native_y;
  config.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  config.mode = PPA_TRANS_MODE_BLOCKING;
  config.user_data = nullptr;

  xSemaphoreTake(this->submit_mutex_, portMAX_DELAY);
  if (still_current != nullptr && !still_current(context)) {
    xSemaphoreGive(this->submit_mutex_);
    return false;
  }
  const esp_err_t error = ppa_do_scale_rotate_mirror(this->transform_client_, &config);
  bool success = false;
  if (error == ESP_OK) {
    const auto *pixels = this->frame_buffer_ + (size_t(native_y) * this->width_ + native_x) * sizeof(uint16_t);
    this->mirror_native_rgb565_(native_x, native_y, output_width, output_height, pixels,
                                this->width_ * sizeof(uint16_t));
    success =
        this->submit_bitmap_locked_(native_x, native_y, native_x + output_width, native_y + output_height, pixels);
  } else {
    ESP_LOGW(TAG, "PPA framebuffer transform failed: %s", esp_err_to_name(error));
  }
  if (still_current != nullptr && !still_current(context))
    success = false;
  xSemaphoreGive(this->submit_mutex_);
  return success;
#else
  return false;
#endif
}

bool MipiDsi::draw_rotated_rgb565_(int x, int y, int w, int h, const uint8_t *data, int x_offset, int y_offset,
                                   int x_pad) {
#ifdef USE_MIPI_DSI_RGB565_OWNED_TRANSFORM
  if (!this->supports_rgb565_transform() || data == nullptr || x_offset < 0 || y_offset < 0 || x_pad < 0)
    return false;
  const int64_t stride = int64_t(x_offset) + w + x_pad;
  const int64_t source_height = int64_t(y_offset) + h;
  if (stride <= 0 || source_height <= 0 || stride > UINT32_MAX || source_height > UINT32_MAX ||
      uint64_t(stride) * source_height > SIZE_MAX / sizeof(uint16_t))
    return false;
  const int64_t left = std::max<int64_t>(0, -int64_t(x)), top = std::max<int64_t>(0, -int64_t(y));
  const int64_t right = std::min<int64_t>(w, int64_t(this->get_width()) - x);
  const int64_t bottom = std::min<int64_t>(h, int64_t(this->get_height()) - y);
  if (left >= right || top >= bottom)
    return true;
  ppa_srm_oper_config_t config{};
  config.in.buffer = data;
  config.in.pic_w = stride;
  config.in.pic_h = source_height;
  config.in.block_offset_x = x_offset + left;
  config.in.block_offset_y = y_offset + top;
  config.in.block_w = right - left;
  config.in.block_h = bottom - top;
  config.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  config.scale_x = config.scale_y = 1.0f;
  config.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  return this->draw_rgb565_transformed(config, size_t(stride) * source_height * sizeof(uint16_t), x + left, y + top,
                                       right - left, bottom - top);
#else
  return false;
#endif
}

void MipiDsi::update() {
  if (this->auto_clear_enabled_) {
    this->clear();
  }
  this->updating_ = true;
  if (this->show_test_card_) {
    this->test_card();
  } else if (this->page_ != nullptr) {
    this->page_->get_writer()(*this);
  } else if (this->writer_.has_value()) {
    (*this->writer_)(*this);
  } else {
    this->stop_poller();
  }
  this->updating_ = false;
  this->flush_dirty_();
}

void MipiDsi::flush_dirty_() {
  // Inside update() the one flush at its end sends everything
  if (this->updating_ || this->buffer_ == nullptr || this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_)
    return;
  ESP_LOGV(TAG, "x_low %d, y_low %d, x_high %d, y_high %d", this->x_low_, this->y_low_, this->x_high_, this->y_high_);
  int w = this->x_high_ - this->x_low_ + 1;
  int h = this->y_high_ - this->y_low_ + 1;
  this->write_to_display_(this->x_low_, this->y_low_, w, h, this->buffer_, this->x_low_, this->y_low_,
                          this->width_ - w - this->x_low_);
  // invalidate watermarks
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

void MipiDsi::draw_pixels_internal(int x_start, int y_start, int w, int h, const uint8_t *ptr,
                                   display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset,
                                   int x_pad) {
  if (w <= 0 || h <= 0)
    return;
#ifdef USE_MIPI_DSI_RGB565_OWNED_TRANSFORM
  if (bitness == display::COLOR_BITNESS_565 && !big_endian && this->rotation_ != display::DISPLAY_ROTATION_0_DEGREES &&
      !this->is_clipping() && !this->updating_ &&
      this->draw_rotated_rgb565_(x_start, y_start, w, h, ptr, x_offset, y_offset, x_pad))
    return;
#endif
  if (bitness != this->color_depth_ || (bitness == display::COLOR_BITNESS_565 && big_endian != this->big_endian_) ||
      this->rotation_ != display::DISPLAY_ROTATION_0_DEGREES || this->is_clipping()) {
    display::Display::draw_pixels_internal(x_start, y_start, w, h, ptr, bitness, big_endian, x_offset, y_offset, x_pad);
    this->mark_dirty_(x_start, y_start, w, h);
    this->flush_dirty_();
    return;
  }
  if (this->buffer_ != nullptr) {
    // With a buffer the block goes through it, or a flush of the old contents
    // would paint over it
    const int first_col = std::max(0, -x_start), end_col = std::min(w, static_cast<int>(this->width_) - x_start);
    const int first_row = std::max(0, -y_start), end_row = std::min(h, static_cast<int>(this->height_) - y_start);
    if (first_col >= end_col || first_row >= end_row)
      return;
    const size_t bytes_per_pixel = 3 - this->color_depth_;
    const size_t stride = x_offset + w + x_pad;
    for (int y = first_row; y != end_row; y++) {
      memcpy(this->buffer_ + ((y_start + y) * this->width_ + x_start + first_col) * bytes_per_pixel,
             ptr + ((y_offset + y) * stride + x_offset + first_col) * bytes_per_pixel,
             (end_col - first_col) * bytes_per_pixel);
    }
    if (this->updating_) {
      this->mark_dirty_(x_start + first_col, y_start + first_row, end_col - first_col, end_row - first_row);
    } else {
      this->write_to_display_(x_start + first_col, y_start + first_row, end_col - first_col, end_row - first_row, ptr,
                              x_offset + first_col, y_offset + first_row, x_pad + w - end_col);
    }
    return;
  }
  this->write_to_display_(x_start, y_start, w, h, ptr, x_offset, y_offset, x_pad);
}

void MipiDsi::write_to_display_(int x_start, int y_start, int w, int h, const uint8_t *ptr, int x_offset, int y_offset,
                                int x_pad) {
  auto bytes_per_pixel = 3 - this->color_depth_;
  auto stride = (x_offset + w + x_pad) * bytes_per_pixel;
  ptr += y_offset * stride + x_offset * bytes_per_pixel;  // skip to the first pixel
  // x_ and y_offset are offsets into the source buffer, unrelated to our own
  // offsets into the display.
  if (x_offset == 0 && x_pad == 0) {
    if (!this->submit_bitmap_(x_start, y_start, x_start + w, y_start + h, ptr))
      return;

  } else {
    // draw line by line
    for (int y = 0; y != h; y++) {
      if (!this->submit_bitmap_(x_start, y + y_start, x_start + w, y + y_start + 1, ptr))
        return;
      ptr += stride;  // next line
    }
  }
}

bool MipiDsi::check_buffer_() {
  if (this->is_failed())
    return false;
  if (this->buffer_ != nullptr)
    return true;
  // this is dependent on the enum values.
  auto bytes_per_pixel = 3 - this->color_depth_;
  RAMAllocator<uint8_t> allocator;
  const size_t bytes = this->height_ * this->width_ * bytes_per_pixel;
  this->buffer_ = allocator.allocate(bytes);
  if (this->buffer_ == nullptr) {
    this->mark_failed(LOG_STR("Could not allocate buffer for display!"));
    return false;
  }
  // A scalar compatibility draw can begin after LVGL/video has already
  // painted the panel. Seed this existing fallback canvas from that image;
  // widening its dirty region must not publish uninitialized background.
  if (this->frame_buffer_ != nullptr && this->frame_buffer_size_ == bytes && this->submit_mutex_ != nullptr) {
    xSemaphoreTake(this->submit_mutex_, portMAX_DELAY);
    const bool synced = esp_cache_msync(this->frame_buffer_, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C) == ESP_OK;
    if (synced)
      std::memcpy(this->buffer_, this->frame_buffer_, bytes);
    xSemaphoreGive(this->submit_mutex_);
    if (!synced) {
      this->mark_failed(LOG_STR("Could not synchronize the display compatibility canvas"));
      return false;
    }
  } else {
    std::memset(this->buffer_, 0, bytes);
  }
  return true;
}

// Widen the dirty box to the drawn rectangle, even where the buffer already
// held the same values.
void MipiDsi::mark_dirty_(int x_start, int y_start, int w, int h) {
  int x1 = x_start, y1 = y_start;
  int x2 = x_start + w - 1, y2 = y_start + h - 1;
  this->rotate_(x1, y1);
  this->rotate_(x2, y2);
  const int right = static_cast<int>(this->width_) - 1;
  const int bottom = static_cast<int>(this->height_) - 1;
  const int left = std::clamp(std::min(x1, x2), 0, right);
  const int top = std::clamp(std::min(y1, y2), 0, bottom);
  x2 = std::clamp(std::max(x1, x2), 0, right);
  y2 = std::clamp(std::max(y1, y2), 0, bottom);
  // An empty box is low above high, so it is replaced rather than widened
  if (this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_) {
    this->x_low_ = left;
    this->x_high_ = x2;
    this->y_low_ = top;
    this->y_high_ = y2;
    return;
  }
  this->x_low_ = std::min<int>(this->x_low_, left);
  this->x_high_ = std::max<int>(this->x_high_, x2);
  this->y_low_ = std::min<int>(this->y_low_, top);
  this->y_high_ = std::max<int>(this->y_high_, y2);
}

void MipiDsi::draw_pixel_at(int x, int y, Color color) {
  if (this->is_point_clipped(x, y))
    return;

  this->rotate_(x, y);
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }
  if (!this->check_buffer_())
    return;
  size_t pos = (y * this->width_) + x;
  switch (this->color_depth_) {
    case display::COLOR_BITNESS_565: {
      auto *ptr_16 = reinterpret_cast<uint16_t *>(this->buffer_);
      const uint16_t new_color = this->to_panel_565_(color);
      if (ptr_16[pos] == new_color)
        return;
      ptr_16[pos] = new_color;
      break;
    }
    case display::COLOR_BITNESS_888:
      // The panel applies the colour order, so the buffer keeps the standard
      // b,g,r layout
      this->buffer_[pos * 3] = color.b;
      this->buffer_[pos * 3 + 1] = color.g;
      this->buffer_[pos * 3 + 2] = color.r;
      break;
    case display::COLOR_BITNESS_332:
      break;
  }
  // low and high watermark may speed up drawing from buffer
  if (x < this->x_low_)
    this->x_low_ = x;
  if (y < this->y_low_)
    this->y_low_ = y;
  if (x > this->x_high_)
    this->x_high_ = x;
  if (y > this->y_high_)
    this->y_high_ = y;
}
void MipiDsi::fill(Color color) {
  if (!this->check_buffer_())
    return;

  // If clipping is active, fall back to base implementation
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    return;
  }

  switch (this->color_depth_) {
    case display::COLOR_BITNESS_565: {
      auto *ptr_16 = reinterpret_cast<uint16_t *>(this->buffer_);
      const uint16_t new_color = this->to_panel_565_(color);
      std::fill_n(ptr_16, this->width_ * this->height_, new_color);
      break;
    }

    case display::COLOR_BITNESS_888:
      for (size_t i = 0; i != this->width_ * this->height_; i++) {
        this->buffer_[i * 3 + 0] = color.b;
        this->buffer_[i * 3 + 1] = color.g;
        this->buffer_[i * 3 + 2] = color.r;
      }
      break;

    default:
      break;
  }
  this->mark_dirty_(0, 0, this->get_width(), this->get_height());
}

int MipiDsi::get_width() {
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_90_DEGREES:
    case display::DISPLAY_ROTATION_270_DEGREES:
      return this->get_height_internal();
    case display::DISPLAY_ROTATION_0_DEGREES:
    case display::DISPLAY_ROTATION_180_DEGREES:
    default:
      return this->get_width_internal();
  }
}

int MipiDsi::get_height() {
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_0_DEGREES:
    case display::DISPLAY_ROTATION_180_DEGREES:
      return this->get_height_internal();
    case display::DISPLAY_ROTATION_90_DEGREES:
    case display::DISPLAY_ROTATION_270_DEGREES:
    default:
      return this->get_width_internal();
  }
}

static const uint8_t PIXEL_MODES[] = {0, 16, 18, 24};

void MipiDsi::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MIPI_DSI RGB LCD"
                "\n  Model: %s"
                "\n  Width: %u"
                "\n  Height: %u"
                "\n  Rotation: %d degrees"
                "\n  DSI Lanes: %u"
                "\n  Lane Bit Rate: %.0fMbps"
                "\n  HSync Pulse Width: %u"
                "\n  HSync Back Porch: %u"
                "\n  HSync Front Porch: %u"
                "\n  VSync Pulse Width: %u"
                "\n  VSync Back Porch: %u"
                "\n  VSync Front Porch: %u"
                "\n  Buffer Color Depth: %d bit"
                "\n  Display Pixel Mode: %d bit"
                "\n  Invert Colors: %s"
                "\n  Pixel Clock: %.1fMHz",
                this->model_, this->width_, this->height_, this->rotation_, this->lanes_, this->lane_bit_rate_,
                this->hsync_pulse_width_, this->hsync_back_porch_, this->hsync_front_porch_, this->vsync_pulse_width_,
                this->vsync_back_porch_, this->vsync_front_porch_, (3 - this->color_depth_) * 8, this->pixel_mode_,
                YESNO(this->invert_colors_), this->pclk_frequency_);
  LOG_PIN("  Reset Pin ", this->reset_pin_);
}
}  // namespace esphome::mipi_dsi
#endif  // USE_ESP32_VARIANT_ESP32P4
