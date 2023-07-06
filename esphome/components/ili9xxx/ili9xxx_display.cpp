#include "ili9xxx_display.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ili9xxx {

static const char *const TAG = "ili9xxx";

void ILI9XXXDisplay::setup() {
  this->setup_pins_();
  this->initialize();

  this->watermark_reset_();

  if (this->buffer_color_mode_ == BITS_16) {
    ExternalRAMAllocator<uint16_t> allocator_16bit(ExternalRAMAllocator<uint16_t>::ALLOW_FAILURE);
    this->buffer_16bit_ = allocator_16bit.allocate(this->get_buffer_length_());
    if (this->buffer_16bit_ != nullptr) {
      this->clear();
      // Try allocating second optional framebuffer
      this->buffer_16bit_2_ = allocator_16bit.allocate(this->get_buffer_length_());
      if (this->buffer_16bit_2_ != nullptr) {
        // Swap buffers as the clear operation will fill first framebuffer only.
        std::swap(this->buffer_16bit_, this->buffer_16bit_2_);
        this->clear();
      }
    } else {
      // Cannot allocate memory for 16 bit color. Try 8 bit color.
      this->buffer_color_mode_ = BITS_8;
    }
  }

  if (this->buffer_16bit_ == nullptr) {
    // `buffer_color_mode_` must be `BITS_8` or `BITS_8_INDEXED`.
    this->init_internal_(this->get_buffer_length_());

    if (this->buffer_ != nullptr) {
      // Try allocating second optional framebuffer
      std::swap(this->buffer_, this->buffer_2_);
      this->init_internal_(this->get_buffer_length_());
      if (this->buffer_ == nullptr) {
        // Cannot allocate second framebuffer.
        std::swap(this->buffer_, this->buffer_2_);
      }
    }
  }

  if (this->buffer_16bit_ == nullptr && this->buffer_ == nullptr) {
    this->mark_failed();
  }
}

void ILI9XXXDisplay::setup_pins_() {
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }

  this->spi_setup();

  this->reset_();
}

void ILI9XXXDisplay::dump_config() {
  LOG_DISPLAY("", "ili9xxx", this);
  switch (this->buffer_color_mode_) {
    case BITS_8_INDEXED:
      ESP_LOGCONFIG(TAG, "  Color mode: 8bit Indexed");
      break;
    case BITS_16:
      ESP_LOGCONFIG(TAG, "  Color mode: 16bit");
      break;
    default:
      ESP_LOGCONFIG(TAG, "  Color mode: 8bit 332 mode");
      break;
  }
  if (this->is_18bitdisplay_) {
    ESP_LOGCONFIG(TAG, "  18-Bit Mode: YES");
  }
  if (this->use_second_buffer_()) {
    ESP_LOGCONFIG(TAG, "  Use 2 Framebuffer: YES");
  }

  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  => Failed to init Memory: YES!");
  }
  LOG_UPDATE_INTERVAL(this);
}

float ILI9XXXDisplay::get_setup_priority() const { return setup_priority::HARDWARE; }

void ILI9XXXDisplay::fill(Color color) {
  if (!this->use_second_buffer_()) {
    // Only use single buffer. A fill operation will force a full redraw.
    this->watermark_full_redraw_();
  }

  switch (this->buffer_color_mode_) {
    case BITS_8_INDEXED: {
      uint8_t new_color = display::ColorUtil::color_to_index8_palette888(color, this->palette_);
      memset(this->buffer_, new_color, this->get_buffer_length_());
    } break;
    case BITS_16: {
      uint16_t new_color = display::ColorUtil::color_to_565(color, display::ColorOrder::COLOR_ORDER_RGB);
      memset(this->buffer_16bit_, new_color, this->get_buffer_length_() * 2);
    } break;
    default: {
      uint8_t new_color = display::ColorUtil::color_to_332(color, display::ColorOrder::COLOR_ORDER_RGB);
      memset(this->buffer_, new_color, this->get_buffer_length_());
    } break;
  }
}

void HOT ILI9XXXDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }
  uint32_t pos = (y * this->width_) + x;
  bool updated = false;
  switch (this->buffer_color_mode_) {
    case BITS_8_INDEXED: {
      uint8_t new_color = display::ColorUtil::color_to_index8_palette888(color, this->palette_);
      if (this->buffer_[pos] != new_color) {
        this->buffer_[pos] = new_color;
        updated = true;
      }
    } break;
    case BITS_16: {
      uint16_t new_color = display::ColorUtil::color_to_565(color, display::ColorOrder::COLOR_ORDER_RGB);
      if (this->buffer_16bit_[pos] != new_color) {
        this->buffer_16bit_[pos] = new_color;
        updated = true;
      }
    } break;
    default: {
      uint8_t new_color = display::ColorUtil::color_to_332(color, display::ColorOrder::COLOR_ORDER_RGB);
      if (this->buffer_[pos] != new_color) {
        this->buffer_[pos] = new_color;
        updated = true;
      }
    } break;
  }

  if (!this->use_second_buffer_() && updated) {
    // low and high watermark may speed up drawing from buffer
    this->x_low_ = (x < this->x_low_) ? x : this->x_low_;
    this->y_low_ = (y < this->y_low_) ? y : this->y_low_;
    this->x_high_ = (x > this->x_high_) ? x : this->x_high_;
    this->y_high_ = (y > this->y_high_) ? y : this->y_high_;
    // ESP_LOGVV(TAG, "=>>> pixel (x:%d, y:%d) (xl:%d, xh:%d, yl:%d, yh:%d", x, y, this->x_low_, this->x_high_,
    //           this->y_low_, this->y_high_);
  }
}

void ILI9XXXDisplay::update() {
  if (this->prossing_update_) {
    this->need_update_ = true;
    return;
  }
  this->prossing_update_ = true;
  do {
    this->need_update_ = false;
    this->do_update_();
  } while (this->need_update_);
  this->prossing_update_ = false;
  this->display_();
}

void ILI9XXXDisplay::display_() {
  if (this->use_second_buffer_()) {
    // Use double framebuffer. Determine screen changes between `buffer_` and `buffer_2_` or `buffer_16bit_` and
    // `buffer_16bit_2_`.
    if (this->first_update_) {
      this->first_update_ = false;
      // On Device reset the FB of the screen may contain invalid data. Push full screen update.
      this->watermark_full_redraw_();
    } else {
      // Compare first and second framebuffer for changes.
      const auto buffer_length = this->get_buffer_length_();
      bool y_low_changed = false;
      uint16_t y_change = 0, x_change = 0;

      // Seperate loops for `buffer_16bit_2_` and `buffer_2_` for performance reasons. Saves ~10ms @320x480px.
      if (this->buffer_16bit_2_ != nullptr) {
        // Full `buffer_16bit_2_` scan takes ~40-50ms @320x480px.
        if (this->buffer_16bit_[0] != this->buffer_16bit_2_[0] &&
            this->buffer_16bit_[buffer_length - 1] != this->buffer_16bit_2_[buffer_length - 1]) {
          // Shortcut for full screen changes.
          this->watermark_full_redraw_();
        } else {
          for (uint32_t pos = 0; pos < buffer_length; pos++) {
            if (this->buffer_16bit_[pos] != this->buffer_16bit_2_[pos]) {
              if (!y_low_changed) {
                // search for y_low value
                this->y_low_ = y_change;
                y_low_changed = true;
              } else {
                // search for y_high value
                this->y_high_ = y_change;
              }
              this->x_low_ = std::min(this->x_low_, x_change);
              this->x_high_ = std::max(this->x_high_, x_change);
            }
            // Keep track of position to avoid FPU calculation per pixel.
            x_change++;
            if (x_change >= this->width_) {
              y_change++;
              x_change = 0;
            }
          }
        }
      } else if (this->buffer_2_ != nullptr) {
        // Full `buffer_2_` scan takes ~30-40ms @320x480px.
        if (this->buffer_[0] != this->buffer_2_[0] &&
            this->buffer_[buffer_length - 1] != this->buffer_2_[buffer_length - 1]) {
          // Shortcut for full screen changes.
          this->watermark_full_redraw_();
        } else {
          for (uint32_t pos = 0; pos < buffer_length; pos++) {
            if (this->buffer_[pos] != this->buffer_2_[pos]) {
              if (!y_low_changed) {
                // search for y_low value
                this->y_low_ = y_change;
                y_low_changed = true;
              } else {
                // search for y_high value
                this->y_high_ = y_change;
              }
              this->x_low_ = std::min(this->x_low_, x_change);
              this->x_high_ = std::max(this->x_high_, x_change);
            }
            // Keep track of position to avoid FPU calculation per pixel.
            x_change++;
            if (x_change >= this->width_) {
              y_change++;
              x_change = 0;
            }
          }
        }
      }
    }
  }

  // we will only update the changed window to the display
  uint16_t w = this->x_high_ - this->x_low_ + 1;  // NOLINT
  uint16_t h = this->y_high_ - this->y_low_ + 1;  // NOLINT
  uint32_t start_pos = ((this->y_low_ * this->width_) + this->x_low_);

  // check if something was displayed
  if ((this->x_high_ < this->x_low_) || (this->y_high_ < this->y_low_)) {
    ESP_LOGV(TAG, "Nothing to display");
    return;
  }

  set_addr_window_(this->x_low_, this->y_low_, w, h);

  ESP_LOGV(TAG,
           "Start display(xlow:%d, ylow:%d, xhigh:%d, yhigh:%d, width:%d, "
           "heigth:%d, start_pos:%d)",
           this->x_low_, this->y_low_, this->x_high_, this->y_high_, w, h, start_pos);

  this->start_data_();
  for (uint16_t row = 0; row < h; row++) {
    uint32_t pos = start_pos + (row * this->width_);
    uint32_t rem = w;

    while (rem > 0) {
      uint32_t sz = std::min(rem, ILI9XXX_TRANSFER_BUFFER_SIZE);
      // ESP_LOGVV(TAG, "Send to display(pos:%d, rem:%d, zs:%d)", pos, rem, sz);
      this->buffer_to_transfer_(pos, sz);
      if (this->is_18bitdisplay_) {
        for (uint32_t i = 0; i < sz; ++i) {
          uint16_t color_val = this->transfer_buffer_[i];

          uint8_t red = color_val & 0x1F;
          uint8_t green = (color_val & 0x7E0) >> 5;
          uint8_t blue = (color_val & 0xF800) >> 11;

          uint8_t pass_buff[3];

          pass_buff[2] = (uint8_t) ((red / 32.0) * 64) << 2;
          pass_buff[1] = (uint8_t) green << 2;
          pass_buff[0] = (uint8_t) ((blue / 32.0) * 64) << 2;

          this->write_array(pass_buff, sizeof(pass_buff));
        }
      } else {
        // Write 16 bit transfer buffer using 8 bit array SPI operation. Ensure correct endian before send.
        for (uint32_t i = 0; i < sz; ++i) {
          this->transfer_buffer_[i] = convert_big_endian(this->transfer_buffer_[i]);
        }
        this->write_array((uint8_t *) (this->transfer_buffer_), sz + sz);
      }
      pos += sz;
      rem -= sz;
    }
    App.feed_wdt();
  }
  this->end_data_();

  this->watermark_reset_();
}

uint32_t ILI9XXXDisplay::buffer_to_transfer_(uint32_t pos, uint32_t sz) {
  switch (this->buffer_color_mode_) {
    case BITS_8_INDEXED:
      for (uint32_t i = 0; i < sz; ++i) {
        this->transfer_buffer_[i] = display::ColorUtil::color_to_565(
            display::ColorUtil::index8_to_color_palette888(this->buffer_[pos + i], this->palette_));
      }
      break;
    case BITS_16:
      memcpy(this->transfer_buffer_, this->buffer_16bit_ + pos, sz + sz);
      if (this->buffer_16bit_2_ != nullptr) {
        // Update second framebuffer. Update here as data is in cache.
        memcpy(this->buffer_16bit_2_ + pos, this->buffer_16bit_ + pos, sz + sz);
      }
      break;
    default:
      for (uint32_t i = 0; i < sz; ++i) {
        this->transfer_buffer_[i] =
            display::ColorUtil::color_to_565(display::ColorUtil::rgb332_to_color(this->buffer_[pos + i]));
      }
      break;
  }
  if (this->buffer_color_mode_ != BITS_16 && this->buffer_2_ != nullptr) {
    // Update second framebuffer. Update here as data is in cache.
    memcpy(this->buffer_2_ + pos, this->buffer_ + pos, sz);
  }
  return sz;
}

uint32_t ILI9XXXDisplay::get_buffer_length_() { return this->get_width_internal() * this->get_height_internal(); }

void ILI9XXXDisplay::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void ILI9XXXDisplay::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

void ILI9XXXDisplay::send_command(uint8_t command_byte, const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->command(command_byte);  // Send the command byte
  this->start_data_();
  this->write_array(data_bytes, num_data_bytes);
  this->end_data_();
}

uint8_t ILI9XXXDisplay::read_command(uint8_t command_byte, uint8_t index) {
  uint8_t data = 0x10 + index;
  this->send_command(0xD9, &data, 1);  // Set Index Register
  uint8_t result;
  this->start_command_();
  this->write_byte(command_byte);
  this->start_data_();
  do {
    result = this->read_byte();
  } while (index--);
  this->end_data_();
  return result;
}

void ILI9XXXDisplay::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}
void ILI9XXXDisplay::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}

void ILI9XXXDisplay::end_command_() { this->disable(); }
void ILI9XXXDisplay::end_data_() { this->disable(); }

void ILI9XXXDisplay::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void ILI9XXXDisplay::init_lcd_(const uint8_t *init_cmd) {
  uint8_t cmd, x, num_args;
  const uint8_t *addr = init_cmd;
  while ((cmd = progmem_read_byte(addr++)) > 0) {
    x = progmem_read_byte(addr++);
    num_args = x & 0x7F;
    send_command(cmd, addr, num_args);
    addr += num_args;
    if (x & 0x80)
      delay(150);  // NOLINT
  }
}

void ILI9XXXDisplay::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h) {
  uint16_t x2 = (x1 + w - 1), y2 = (y1 + h - 1);
  this->command(ILI9XXX_CASET);  // Column address set
  this->start_data_();
  this->write_byte(x1 >> 8);
  this->write_byte(x1);
  this->write_byte(x2 >> 8);
  this->write_byte(x2);
  this->end_data_();
  this->command(ILI9XXX_PASET);  // Row address set
  this->start_data_();
  this->write_byte(y1 >> 8);
  this->write_byte(y1);
  this->write_byte(y2 >> 8);
  this->write_byte(y2);
  this->end_data_();
  this->command(ILI9XXX_RAMWR);  // Write to RAM
}

void ILI9XXXDisplay::invert_display_(bool invert) { this->command(invert ? ILI9XXX_INVON : ILI9XXX_INVOFF); }

int ILI9XXXDisplay::get_width_internal() { return this->width_; }
int ILI9XXXDisplay::get_height_internal() { return this->height_; }

//   M5Stack display
void ILI9XXXM5Stack::initialize() {
  this->init_lcd_(INITCMD_M5STACK);
  if (this->width_ == 0)
    this->width_ = 320;
  if (this->height_ == 0)
    this->height_ = 240;
  this->invert_display_(true);
}

//   M5CORE display // Based on the configuration settings of M5stact's M5GFX code.
void ILI9XXXM5CORE::initialize() {
  this->init_lcd_(INITCMD_M5CORE);
  if (this->width_ == 0)
    this->width_ = 320;
  if (this->height_ == 0)
    this->height_ = 240;
  this->invert_display_(true);
}

//   24_TFT display
void ILI9XXXILI9341::initialize() {
  this->init_lcd_(INITCMD_ILI9341);
  if (this->width_ == 0)
    this->width_ = 240;
  if (this->height_ == 0)
    this->height_ = 320;
}
//   24_TFT rotated display
void ILI9XXXILI9342::initialize() {
  this->init_lcd_(INITCMD_ILI9341);
  if (this->width_ == 0) {
    this->width_ = 320;
  }
  if (this->height_ == 0) {
    this->height_ = 240;
  }
}

//   35_TFT display
void ILI9XXXILI9481::initialize() {
  this->init_lcd_(INITCMD_ILI9481);
  if (this->width_ == 0) {
    this->width_ = 480;
  }
  if (this->height_ == 0) {
    this->height_ = 320;
  }
}

//   35_TFT display
void ILI9XXXILI9486::initialize() {
  this->init_lcd_(INITCMD_ILI9486);
  if (this->width_ == 0) {
    this->width_ = 480;
  }
  if (this->height_ == 0) {
    this->height_ = 320;
  }
}
//    40_TFT display
void ILI9XXXILI9488::initialize() {
  this->init_lcd_(INITCMD_ILI9488);
  if (this->width_ == 0) {
    this->width_ = 480;
  }
  if (this->height_ == 0) {
    this->height_ = 320;
  }
  this->is_18bitdisplay_ = true;
}
//    40_TFT display
void ILI9XXXILI9488A::initialize() {
  this->init_lcd_(INITCMD_ILI9488_A);
  if (this->width_ == 0) {
    this->width_ = 480;
  }
  if (this->height_ == 0) {
    this->height_ = 320;
  }
  this->is_18bitdisplay_ = true;
}
//    40_TFT display
void ILI9XXXST7796::initialize() {
  this->init_lcd_(INITCMD_ST7796);
  if (this->width_ == 0) {
    this->width_ = 320;
  }
  if (this->height_ == 0) {
    this->height_ = 480;
  }
}

//   24_TFT rotated display
void ILI9XXXS3Box::initialize() {
  this->init_lcd_(INITCMD_S3BOX);
  if (this->width_ == 0) {
    this->width_ = 320;
  }
  if (this->height_ == 0) {
    this->height_ = 240;
  }
}

//   24_TFT rotated display
void ILI9XXXS3BoxLite::initialize() {
  this->init_lcd_(INITCMD_S3BOXLITE);
  if (this->width_ == 0) {
    this->width_ = 320;
  }
  if (this->height_ == 0) {
    this->height_ = 240;
  }
  this->invert_display_(true);
}

}  // namespace ili9xxx
}  // namespace esphome
