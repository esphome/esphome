#include "st7920.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7920 {

static const char *const TAG = "st7920";

// ST7920 COMMANDS
static const uint8_t LCD_DATA = 0xFA;
static const uint8_t LCD_COMMAND = 0xF8;
static const uint8_t LCD_CLS = 0x01;
static const uint8_t LCD_HOME = 0x02;
static const uint8_t LCD_ADDRINC = 0x06;
static const uint8_t LCD_DISPLAYON = 0x0C;
static const uint8_t LCD_DISPLAYOFF = 0x08;
static const uint8_t LCD_CURSORON = 0x0E;
static const uint8_t LCD_CURSORBLINK = 0x0F;
static const uint8_t LCD_BASIC = 0x30;
static const uint8_t LCD_GFXMODE = 0x36;
static const uint8_t LCD_EXTEND = 0x34;
static const uint8_t LCD_TXTMODE = 0x34;
static const uint8_t LCD_STANDBY = 0x01;
static const uint8_t LCD_SCROLL = 0x03;
static const uint8_t LCD_SCROLLADDR = 0x40;
static const uint8_t LCD_ADDR = 0x80;
static const uint8_t LCD_LINE0 = 0x80;
static const uint8_t LCD_LINE1 = 0x90;
static const uint8_t LCD_LINE2 = 0x88;
static const uint8_t LCD_LINE3 = 0x98;

void ST7920::setup() {
  this->dump_config();
  this->spi_setup();
  this->init_internal_(this->get_buffer_length_());
  display_init_();
}

void ST7920::command_(uint8_t value) {
  this->enable();
  this->send_(LCD_COMMAND, value);
  this->disable();
}

void ST7920::data_(uint8_t value) {
  this->enable();
  this->send_(LCD_DATA, value);
  this->disable();
}

void ST7920::send_(uint8_t type, uint8_t value) {
  this->write_byte(type);
  this->write_byte(value & 0xF0);
  this->write_byte(value << 4);
}

void ST7920::goto_xy_(uint16_t x, uint16_t y) {
  if (y >= 32 && y < 64) {
    y -= 32;
    x += 8;
  } else if (y >= 64 && y < 64 + 32) {
    y -= 32;
    x += 0;
  } else if (y >= 64 + 32 && y < 64 + 64) {
    y -= 64;
    x += 8;
  }
  this->command_(LCD_ADDR | y);  // 6-bit (0..63)
  this->command_(LCD_ADDR | x);  // 4-bit (0..15)
}

void HOT ST7920::write_display_data() {
  uint8_t i, j, b;
  for (j = 0; j < (uint8_t) (this->get_height_internal() / 2); j++) {
    this->goto_xy_(0, j);
    this->enable();
    for (i = 0; i < 16; i++) {  // 16 bytes from line #0+
      b = this->buffer_[i + j * 16];
      this->send_(LCD_DATA, b);
    }
    for (i = 0; i < 16; i++) {  // 16 bytes from line #32+
      b = this->buffer_[i + (j + 32) * 16];
      this->send_(LCD_DATA, b);
    }
    this->disable();
    App.feed_wdt();
  }
}

void ST7920::fill(Color color) {
  const uint8_t fill = color.is_on() ? 0xFF : 0x00;
  const int16_t w = this->get_width_internal();
  const int16_t h = this->get_height_internal();
  const int16_t w_bytes = w / 8;  // 8 horizontal pixels per byte

  // Calculate fill region, respecting clipping
  display::Rect fill_rect(0, 0, w, h);
  display::Rect clip = this->get_clipping();
  if (clip.is_set()) {
    fill_rect.shrink(clip);
    if (!fill_rect.is_set())
      return;  // Completely clipped
  }

  // Check if filling entire display
  if (fill_rect.x == 0 && fill_rect.y == 0 && fill_rect.w == w && fill_rect.h == h) {
    memset(this->buffer_, fill, this->get_buffer_length_());
    return;
  }

  // Partial fill - horizontal layout (8 horizontal pixels per byte, MSB first)
  const int16_t x_start = fill_rect.x;
  const int16_t x_end = fill_rect.x + fill_rect.w;
  const int16_t y_start = fill_rect.y;
  const int16_t y_end = fill_rect.y + fill_rect.h;

  for (int16_t y = y_start; y < y_end; y++) {
    const uint32_t row_offset = y * w_bytes;

    // Calculate byte boundaries
    const int16_t byte_start = x_start / 8;
    const int16_t byte_end = (x_end - 1) / 8;

    for (int16_t byte_idx = byte_start; byte_idx <= byte_end; byte_idx++) {
      const int16_t byte_x_start = byte_idx * 8;
      const int16_t byte_x_end = byte_x_start + 8;

      // Calculate which bits in this byte are affected
      const int16_t bit_start = (x_start > byte_x_start) ? (x_start - byte_x_start) : 0;
      const int16_t bit_end = (x_end < byte_x_end) ? (x_end - byte_x_start) : 8;

      // Create mask for affected bits (MSB first: bit 0 is 0x80)
      uint8_t mask = 0;
      for (int16_t bit = bit_start; bit < bit_end; bit++) {
        mask |= (0x80 >> bit);
      }

      if (mask == 0xFF) {
        this->buffer_[row_offset + byte_idx] = fill;
      } else {
        if (fill) {
          this->buffer_[row_offset + byte_idx] |= mask;
        } else {
          this->buffer_[row_offset + byte_idx] &= ~mask;
        }
      }
    }
  }
}

void ST7920::dump_config() {
  LOG_DISPLAY("", "ST7920", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG,
                "  Height: %d\n"
                "  Width: %d",
                this->height_, this->width_);
}

float ST7920::get_setup_priority() const { return setup_priority::PROCESSOR; }

void ST7920::update() {
  this->clear();
  if (this->writer_local_.has_value())  // call lambda function if available
    (*this->writer_local_)(*this);
  this->write_display_data();
}

int ST7920::get_width_internal() { return this->width_; }

int ST7920::get_height_internal() { return this->height_; }

size_t ST7920::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
}

void HOT ST7920::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }
  int width = this->get_width_internal() / 8u;
  if (color.is_on()) {
    this->buffer_[y * width + x / 8] |= (0x80 >> (x & 7));
  } else {
    this->buffer_[y * width + x / 8] &= ~(0x80 >> (x & 7));
  }
}

void ST7920::display_init_() {
  ESP_LOGD(TAG, "Initializing display");
  this->command_(LCD_BASIC);      // 8bit mode
  this->command_(LCD_BASIC);      // 8bit mode
  this->command_(LCD_CLS);        // clear screen
  delay(12);                      // >10 ms delay
  this->command_(LCD_ADDRINC);    // cursor increment right no shift
  this->command_(LCD_DISPLAYON);  // D=1, C=0, B=0
  this->command_(LCD_EXTEND);     // LCD_EXTEND);
  this->command_(LCD_GFXMODE);    // LCD_GFXMODE);
  this->write_display_data();
}

}  // namespace st7920
}  // namespace esphome
