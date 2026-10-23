#if defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S31)
#include "mipi_rgb.h"
#include "esphome/components/display/init_sequence.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>
#include <driver/gpio.h>
#include <esp_lcd_panel_ops.h>
#include <span>

namespace esphome::mipi_rgb {

// Maximum bytes to log for init commands (truncated if larger)
static constexpr size_t MIPI_RGB_MAX_CMD_LOG_BYTES = 64;
static constexpr uint8_t MADCTL_MY = 0x80;     // Bit 7 Bottom to top
static constexpr uint8_t MADCTL_MX = 0x40;     // Bit 6 Right to left
static constexpr uint8_t MADCTL_MV = 0x20;     // Bit 5 Swap axes
static constexpr uint8_t MADCTL_ML = 0x10;     // Bit 4 Refresh bottom to top
static constexpr uint8_t MADCTL_BGR = 0x08;    // Bit 3 Blue-Green-Red pixel order
static constexpr uint8_t MADCTL_XFLIP = 0x02;  // Mirror the display horizontally
static constexpr uint8_t MADCTL_YFLIP = 0x01;  // Mirror the display vertically

void MipiRgb::setup_enables_() {
  if (!this->enable_pins_.empty()) {
    for (auto *pin : this->enable_pins_) {
      pin->setup();
      pin->digital_write(true);
    }
    delay(10);
  }
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
  }
}

#ifdef USE_SPI
void MipiRgbSpi::setup() {
  this->setup_enables_();
  this->spi_setup();
  this->common_setup_();
  if (this->is_failed())
    return;
  this->write_init_sequence_();
}
void MipiRgbSpi::write_command_(uint8_t value) {
  this->enable();
  if (this->dc_pin_ == nullptr) {
    this->write(value, 9);
  } else {
    this->dc_pin_->digital_write(false);
    this->write_byte(value);
    this->dc_pin_->digital_write(true);
  }
  this->disable();
}

void MipiRgbSpi::write_data_(uint8_t value) {
  this->enable();
  if (this->dc_pin_ == nullptr) {
    this->write(value | 0x100, 9);
  } else {
    this->dc_pin_->digital_write(true);
    this->write_byte(value);
  }
  this->disable();
}

void MipiRgbSpi::write_init_sequence_() {
  display::InitSequenceReader reader(this->init_sequence_, this->init_sequence_len_);
  display::InitSequenceEntry entry;
  while (reader.next(entry)) {
    if (entry.is_delay()) {
      ESP_LOGD(TAG, "Delay %dms", entry.value);
      delay(entry.value);
      continue;
    }
    const uint8_t cmd = entry.value;
    if (cmd == SLEEP_OUT)
      delay(120);  // NOLINT
    char hex_buf[format_hex_pretty_size(MIPI_RGB_MAX_CMD_LOG_BYTES)];
    ESP_LOGD(TAG, "Write command %02X, length %d, byte(s) %s", cmd, entry.count,
             format_hex_pretty_to(hex_buf, entry.args, entry.count, '.'));
    this->write_command_(cmd);
    for (uint8_t i = 0; i < entry.count; i++)
      this->write_data_(entry.args[i]);
    if (cmd == SLEEP_OUT)
      delay(10);
  }
  if (reader.malformed()) {
    this->mark_failed(LOG_STR("Malformed init sequence"));
    return;
  }
  // this->spi_teardown();  // SPI not needed after this
  delay(10);
}

void MipiRgbSpi::dump_config() {
  MipiRgb::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %uMHz", (unsigned) (this->data_rate_ / 1000000));
}

#endif  // USE_SPI

void MipiRgb::setup() {
  this->setup_enables_();
  this->common_setup_();
}

void MipiRgb::common_setup_() {
  esp_lcd_rgb_panel_config_t config{};
  config.flags.fb_in_psram = 1;
  config.bounce_buffer_size_px = this->width_ * 10;
  config.num_fbs = 1;
  config.timings.h_res = this->width_;
  config.timings.v_res = this->height_;
  config.timings.hsync_pulse_width = this->hsync_pulse_width_;
  config.timings.hsync_back_porch = this->hsync_back_porch_;
  config.timings.hsync_front_porch = this->hsync_front_porch_;
  config.timings.vsync_pulse_width = this->vsync_pulse_width_;
  config.timings.vsync_back_porch = this->vsync_back_porch_;
  config.timings.vsync_front_porch = this->vsync_front_porch_;
  config.timings.flags.pclk_active_neg = this->pclk_inverted_;
  config.timings.pclk_hz = this->pclk_frequency_;
  config.clk_src = LCD_CLK_SRC_PLL160M;
  size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
  for (size_t i = 0; i != data_pin_count; i++) {
    config.data_gpio_nums[i] = static_cast<gpio_num_t>(this->data_pins_[i]->get_pin());
  }
  config.data_width = data_pin_count;
  config.disp_gpio_num = GPIO_NUM_NC;
  if (this->hsync_pin_) {
    config.hsync_gpio_num = static_cast<gpio_num_t>(this->hsync_pin_->get_pin());
  } else {
    config.hsync_gpio_num = GPIO_NUM_NC;
  }
  if (this->vsync_pin_) {
    config.vsync_gpio_num = static_cast<gpio_num_t>(this->vsync_pin_->get_pin());
  } else {
    config.vsync_gpio_num = GPIO_NUM_NC;
  }
  if (this->de_pin_) {
    config.de_gpio_num = static_cast<gpio_num_t>(this->de_pin_->get_pin());
  } else {
    config.de_gpio_num = GPIO_NUM_NC;
  }
  config.pclk_gpio_num = static_cast<gpio_num_t>(this->pclk_pin_->get_pin());
  esp_err_t err = esp_lcd_new_rgb_panel(&config, &this->handle_);
  if (err == ESP_OK)
    err = esp_lcd_panel_reset(this->handle_);
  if (err == ESP_OK)
    err = esp_lcd_panel_init(this->handle_);
  if (err == ESP_OK) {
    void *fb = nullptr;
    err = esp_lcd_rgb_panel_get_frame_buffer(this->handle_, 1, &fb);
    if (err == ESP_OK && fb != nullptr) {
      this->fb_ = reinterpret_cast<uint16_t *>(fb);
    } else if (err == ESP_OK) {
      err = ESP_ERR_NO_MEM;
    }
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "lcd setup failed: %s", esp_err_to_name(err));
    this->mark_failed(LOG_STR("lcd setup failed"));
  }
  ESP_LOGCONFIG(TAG, "MipiRgb setup complete");
}

void MipiRgb::update() {
  if (this->is_failed())
    return;
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

void MipiRgb::flush_dirty_() {
  // Inside update() the one flush at its end sends everything
  if (this->updating_ || this->buffer_ == nullptr || this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_)
    return;
  ESP_LOGV(TAG, "x_low %d, y_low %d, x_high %d, y_high %d", this->x_low_, this->y_low_, this->x_high_, this->y_high_);
  int w = this->x_high_ - this->x_low_ + 1;
  int h = this->y_high_ - this->y_low_ + 1;
  this->write_to_display_(this->x_low_, this->y_low_, w, h, reinterpret_cast<const uint8_t *>(this->buffer_),
                          this->x_low_, this->y_low_, this->width_ - w - this->x_low_);
  // invalidate watermarks
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

void MipiRgb::draw_pixels_internal(int x_start, int y_start, int w, int h, const uint8_t *ptr,
                                   display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset,
                                   int x_pad) {
  if (w <= 0 || h <= 0 || this->is_failed())
    return;
  if (bitness != display::COLOR_BITNESS_565 || big_endian != this->big_endian_ || this->is_clipping()) {
    Display::draw_pixels_internal(x_start, y_start, w, h, ptr, bitness, big_endian, x_offset, y_offset, x_pad);
    this->mark_dirty_(x_start, y_start, w, h);
    this->flush_dirty_();
    return;
  }

  const int log_w = this->get_width();
  const int log_h = this->get_height();

  if (x_start < 0 || y_start < 0 || x_start + w > log_w || y_start + h > log_h) {
    return;
  }

  uint16_t *dst = this->buffer_ != nullptr ? this->buffer_ : this->fb_;
  const size_t stride = static_cast<size_t>(x_offset) + w + x_pad;
  const auto *src_origin =
      reinterpret_cast<const uint16_t *>(ptr) + (static_cast<size_t>(y_offset) * stride + x_offset);

  blit_rotated_565(dst, this->width_, this->height_, this->rotation_, src_origin, x_start, y_start, w, h, stride);
  if (dst == this->buffer_) {
    this->mark_dirty_(x_start, y_start, w, h);
    if (!this->updating_) {
      this->flush_dirty_();
    }
  }
}

void MipiRgb::write_to_display_(int x_start, int y_start, int w, int h, const uint8_t *ptr, int x_offset, int y_offset,
                                int x_pad) {
  esp_err_t err = ESP_OK;
  auto stride = (x_offset + w + x_pad) * 2;
  ptr += y_offset * stride + x_offset * 2;  // skip to the first pixel
  // x_ and y_offset are offsets into the source buffer, unrelated to our own offsets into the display.
  if (x_offset == 0 && x_pad == 0) {
    err = esp_lcd_panel_draw_bitmap(this->handle_, x_start, y_start, x_start + w, y_start + h, ptr);
  } else {
    // draw line by line
    for (int y = 0; y != h; y++) {
      err = esp_lcd_panel_draw_bitmap(this->handle_, x_start, y + y_start, x_start + w, y + y_start + 1, ptr);
      if (err != ESP_OK)
        break;
      ptr += stride;  // next line
    }
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "lcd_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(err));
  }
}

bool MipiRgb::check_buffer_() {
  if (this->is_failed())
    return false;
  if (this->buffer_ != nullptr)
    return true;
  // this is dependent on the enum values.
  RAMAllocator<uint16_t> allocator;
  this->buffer_ = allocator.allocate(this->height_ * this->width_);
  if (this->buffer_ == nullptr) {
    this->mark_failed(LOG_STR("Could not allocate buffer for display!"));
    return false;
  }
  return true;
}

// Widen the dirty box to the drawn rectangle, even where the buffer already held the same values.
void MipiRgb::mark_dirty_(int x_start, int y_start, int w, int h) {
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

void MipiRgb::draw_pixel_at(int x, int y, Color color) {
  if (this->is_point_clipped(x, y) || this->is_failed())
    return;

  this->rotate_(x, y);
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }
  if (!this->check_buffer_())
    return;
  size_t pos = (y * this->width_) + x;
  uint16_t new_color = this->to_panel_565_(color);
  if (this->buffer_[pos] == new_color)
    return;
  this->buffer_[pos] = new_color;
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
void MipiRgb::fill(Color color) {
  if (!this->check_buffer_())
    return;

  // If clipping is active, fall back to base implementation
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    return;
  }

  auto *ptr_16 = reinterpret_cast<uint16_t *>(this->buffer_);
  uint16_t new_color = this->to_panel_565_(color);
  std::fill_n(ptr_16, this->width_ * this->height_, new_color);
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_ - 1;
  this->y_high_ = this->height_ - 1;
}

int MipiRgb::get_width() {
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

int MipiRgb::get_height() {
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

[[maybe_unused]] static const char *get_pin_name(GPIOPin *pin, std::span<char, GPIO_SUMMARY_MAX_LEN> buffer) {
  if (pin == nullptr)
    return "None";
  pin->dump_summary(buffer.data(), buffer.size());
  return buffer.data();
}

void MipiRgb::dump_pins_(uint8_t start, uint8_t end, const char *name, uint8_t offset) {
  char pin_summary[GPIO_SUMMARY_MAX_LEN];
  for (uint8_t i = start; i != end; i++) {
    this->data_pins_[i]->dump_summary(pin_summary, sizeof(pin_summary));
    ESP_LOGCONFIG(TAG, "  %s pin %d: %s", name, offset++, pin_summary);
  }
}

void MipiRgb::dump_config() {
  char reset_buf[GPIO_SUMMARY_MAX_LEN];
  char de_buf[GPIO_SUMMARY_MAX_LEN];
  char pclk_buf[GPIO_SUMMARY_MAX_LEN];
  char hsync_buf[GPIO_SUMMARY_MAX_LEN];
  char vsync_buf[GPIO_SUMMARY_MAX_LEN];
  ESP_LOGCONFIG(TAG,
                "MIPI_RGB LCD"
                "\n  Model: %s"
                "\n  Width: %u"
                "\n  Height: %u"
                "\n  Rotation: %d degrees"
                "\n  PCLK Inverted: %s"
                "\n  HSync Pulse Width: %u"
                "\n  HSync Back Porch: %u"
                "\n  HSync Front Porch: %u"
                "\n  VSync Pulse Width: %u"
                "\n  VSync Back Porch: %u"
                "\n  VSync Front Porch: %u"
                "\n  Invert Colors: %s"
                "\n  Pixel Clock: %uMHz"
                "\n  Reset Pin: %s"
                "\n  DE Pin: %s"
                "\n  PCLK Pin: %s"
                "\n  HSYNC Pin: %s"
                "\n  VSYNC Pin: %s",
                this->model_, this->width_, this->height_, this->rotation_, YESNO(this->pclk_inverted_),
                this->hsync_pulse_width_, this->hsync_back_porch_, this->hsync_front_porch_, this->vsync_pulse_width_,
                this->vsync_back_porch_, this->vsync_front_porch_, YESNO(this->invert_colors_),
                (unsigned) (this->pclk_frequency_ / 1000000), get_pin_name(this->reset_pin_, reset_buf),
                get_pin_name(this->de_pin_, de_buf), get_pin_name(this->pclk_pin_, pclk_buf),
                get_pin_name(this->hsync_pin_, hsync_buf), get_pin_name(this->vsync_pin_, vsync_buf));

  this->dump_pins_(8, 13, "Blue", 0);
  this->dump_pins_(13, 16, "Green", 0);
  this->dump_pins_(0, 3, "Green", 3);
  this->dump_pins_(3, 8, "Red", 0);
}

}  // namespace esphome::mipi_rgb
#endif  // defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4) ||
        // defined(USE_ESP32_VARIANT_ESP32S31)
