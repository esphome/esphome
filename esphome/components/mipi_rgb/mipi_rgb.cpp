#ifdef USE_ESP32_VARIANT_ESP32S3
#include "mipi_rgb.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esp_lcd_panel_rgb.h"

namespace esphome {
namespace mipi_rgb {

static const uint8_t DELAY_FLAG = 0xFF;
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
  this->write_init_sequence_();
  this->common_setup_();
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

/**
 * this relies upon the init sequence being well-formed, which is guaranteed by the Python init code.
 */

void MipiRgbSpi::write_init_sequence_() {
  size_t index = 0;
  auto &vec = this->init_sequence_;
  while (index != vec.size()) {
    if (vec.size() - index < 2) {
      this->mark_failed(LOG_STR("Malformed init sequence"));
      return;
    }
    uint8_t cmd = vec[index++];
    uint8_t x = vec[index++];
    if (x == DELAY_FLAG) {
      ESP_LOGD(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      uint8_t num_args = x & 0x7F;
      if (vec.size() - index < num_args) {
        this->mark_failed(LOG_STR("Malformed init sequence"));
        return;
      }
      if (cmd == SLEEP_OUT) {
        delay(120);  // NOLINT
      }
      const auto *ptr = vec.data() + index;
      ESP_LOGD(TAG, "Write command %02X, length %d, byte(s) %s", cmd, num_args,
               format_hex_pretty(ptr, num_args, '.', false).c_str());
      index += num_args;
      this->write_command_(cmd);
      while (num_args-- != 0)
        this->write_data_(*ptr++);
      if (cmd == SLEEP_OUT)
        delay(10);
    }
  }
  // this->spi_teardown();  // SPI not needed after this
  this->init_sequence_.clear();
  delay(10);
}

void MipiRgbSpi::dump_config() {
  MipiRgb::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  ESP_LOGCONFIG(TAG,
                "  SPI Data rate: %uMHz"
                "\n  Mirror X: %s"
                "\n  Mirror Y: %s"
                "\n  Swap X/Y: %s"
                "\n  Color Order: %s",
                (unsigned) (this->data_rate_ / 1000000), YESNO(this->madctl_ & (MADCTL_XFLIP | MADCTL_MX)),
                YESNO(this->madctl_ & (MADCTL_YFLIP | MADCTL_MY | MADCTL_ML)), YESNO(this->madctl_ & MADCTL_MV),
                this->madctl_ & MADCTL_BGR ? "BGR" : "RGB");
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
    config.data_gpio_nums[i] = this->data_pins_[i]->get_pin();
  }
  config.data_width = data_pin_count;
  config.disp_gpio_num = -1;
  config.hsync_gpio_num = this->hsync_pin_->get_pin();
  config.vsync_gpio_num = this->vsync_pin_->get_pin();
  if (this->de_pin_) {
    config.de_gpio_num = this->de_pin_->get_pin();
  } else {
    config.de_gpio_num = -1;
  }
  config.pclk_gpio_num = this->pclk_pin_->get_pin();
  esp_err_t err = esp_lcd_new_rgb_panel(&config, &this->handle_);
  if (err == ESP_OK)
    err = esp_lcd_panel_reset(this->handle_);
  if (err == ESP_OK)
    err = esp_lcd_panel_init(this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "lcd setup failed: %s", esp_err_to_name(err));
    this->mark_failed(LOG_STR("lcd setup failed"));
  }
  ESP_LOGCONFIG(TAG, "MipiRgb setup complete");
}

void MipiRgb::loop() {
  if (this->handle_ != nullptr)
    esp_lcd_rgb_panel_restart(this->handle_);
}

void MipiRgb::update() {
  if (this->is_failed())
    return;
  if (this->auto_clear_enabled_) {
    this->clear();
  }
  if (this->show_test_card_) {
    this->test_card();
  } else if (this->page_ != nullptr) {
    this->page_->get_writer()(*this);
  } else if (this->writer_.has_value()) {
    (*this->writer_)(*this);
  } else {
    this->stop_poller();
  }
  if (this->buffer_ == nullptr || this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_)
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

void MipiRgb::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                             display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (w <= 0 || h <= 0 || this->is_failed())
    return;
  // if color mapping is required, pass the buck.
  // note that endianness is not considered here - it is assumed to match!
  if (bitness != display::COLOR_BITNESS_565) {
    Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
    this->write_to_display_(x_start, y_start, w, h, reinterpret_cast<const uint8_t *>(this->buffer_), x_start, y_start,
                            this->width_ - w - x_start);
  } else {
    this->write_to_display_(x_start, y_start, w, h, ptr, x_offset, y_offset, x_pad);
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
  if (err != ESP_OK)
    ESP_LOGE(TAG, "lcd_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(err));
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

void MipiRgb::draw_pixel_at(int x, int y, Color color) {
  if (!this->get_clipping().inside(x, y) || this->is_failed())
    return;

  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_0_DEGREES:
      break;
    case display::DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = this->width_ - x - 1;
      break;
    case display::DISPLAY_ROTATION_180_DEGREES:
      x = this->width_ - x - 1;
      y = this->height_ - y - 1;
      break;
    case display::DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = this->height_ - y - 1;
      break;
  }
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }
  if (!this->check_buffer_())
    return;
  size_t pos = (y * this->width_) + x;
  uint8_t hi_byte = static_cast<uint8_t>(color.r & 0xF8) | (color.g >> 5);
  uint8_t lo_byte = static_cast<uint8_t>((color.g & 0x1C) << 3) | (color.b >> 3);
  uint16_t new_color = hi_byte | (lo_byte << 8);  // big endian
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
  auto *ptr_16 = reinterpret_cast<uint16_t *>(this->buffer_);
  uint8_t hi_byte = static_cast<uint8_t>(color.r & 0xF8) | (color.g >> 5);
  uint8_t lo_byte = static_cast<uint8_t>((color.g & 0x1C) << 3) | (color.b >> 3);
  uint16_t new_color = lo_byte | (hi_byte << 8);  // little endian
  std::fill_n(ptr_16, this->width_ * this->height_, new_color);
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

static std::string get_pin_name(GPIOPin *pin) {
  if (pin == nullptr)
    return "None";
  return pin->dump_summary();
}

void MipiRgb::dump_pins_(uint8_t start, uint8_t end, const char *name, uint8_t offset) {
  for (uint8_t i = start; i != end; i++) {
    ESP_LOGCONFIG(TAG, "  %s pin %d: %s", name, offset++, this->data_pins_[i]->dump_summary().c_str());
  }
}

void MipiRgb::dump_config() {
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
                (unsigned) (this->pclk_frequency_ / 1000000), get_pin_name(this->reset_pin_).c_str(),
                get_pin_name(this->de_pin_).c_str(), get_pin_name(this->pclk_pin_).c_str(),
                get_pin_name(this->hsync_pin_).c_str(), get_pin_name(this->vsync_pin_).c_str());

  this->dump_pins_(8, 13, "Blue", 0);
  this->dump_pins_(13, 16, "Green", 0);
  this->dump_pins_(0, 3, "Green", 3);
  this->dump_pins_(3, 8, "Red", 0);
}

}  // namespace mipi_rgb
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32S3
