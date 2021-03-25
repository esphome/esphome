#include "ili9341_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/components/display/buffer_565.h"

namespace esphome {
namespace ili9341 {

static const char *TAG = "ili9341";

void ILI9341Display::setup_pins_() {
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }
  if (this->led_pin_ != nullptr) {
    this->led_pin_->setup();
    this->led_pin_->digital_write(true);
  }
  this->spi_setup();

  this->reset_();
  ESP_LOGD(TAG, "setup_pins_ done");
}

void ILI9341Display::dump_config() {
  LOG_DISPLAY("", "ili9341", this);
  ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d,  Rotation: %d", this->get_width(), this->get_height(), this->rotation_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  Backlight Pin: ", this->led_pin_);
  LOG_UPDATE_INTERVAL(this);
}

float ILI9341Display::get_setup_priority() const { return setup_priority::PROCESSOR; }

void ILI9341Display::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void ILI9341Display::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void ILI9341Display::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

void ILI9341Display::send_command(uint8_t command_byte, const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->command(command_byte);  // Send the command byte

  this->start_data_();
  for (uint8_t i = 0; i < num_data_bytes; i++) {
    this->write_byte(pgm_read_byte(data_bytes++));  // write byte - SPI library
  }
  this->end_data_();
}
bool first_run = true;

void ILI9341Display::update() {
  ESP_LOGD(TAG, "update");
  this->do_update_();
  // #ifndef NO_PARTIAL
  //   if (first_run) {
  //     first_run = false;
  //     this->buffer_base_->reset_partials();
  //   }
  // #endif
  this->display_();
  this->buffer_base_->display_end();
}

void ILI9341Display::display_() {
#ifdef USE_BUFFER_RGB565
  auto buff = static_cast<display::Buffer565 *>(this->buffer_base_);
  set_addr_window_(0, 0, this->get_width(), this->get_height());
  this->start_data_();
  this->write_array16(buff->buffer_, this->buffer_base_->get_buffer_length());  // Ma
#else

#ifdef NO_PARTIAL
  const uint32_t start_pos = 0;
  const uint32_t w = this->get_width();
  const uint32_t h = this->get_height();
  set_addr_window_(0, 0, w, h);
  ESP_LOGD(TAG, "Asked to update %d/%d to %d/%d", 0, 0, w, h);

#else
  int w = this->buffer_base_->get_partial_update_x();
  int h = this->buffer_base_->get_partial_update_y();

  if (w == this->buffer_base_->get_partial_update_x_low() && h == this->buffer_base_->get_partial_update_y_low()) {
    return;
  }

  ESP_LOGD(TAG, "Asked to update %d/%d to %d/%d",
           this->buffer_base_->get_partial_update_x_low() + this->get_col_start(),
           this->buffer_base_->get_partial_update_y_low() + this->get_row_start(), w, h);

  const uint32_t start_pos = ((this->buffer_base_->get_partial_update_y_low() * this->get_width()) +
                              this->buffer_base_->get_partial_update_x_low());

  set_addr_window_(this->buffer_base_->get_partial_update_x_low() + this->get_col_start(),
                   this->buffer_base_->get_partial_update_y_low() + this->get_row_start(), w, h);

#endif
  this->start_data_();
  uint8_t transfer_index = 0;

  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      uint32_t pos = start_pos + (row * get_width_internal()) + col;

      uint16_t color_to_write = this->buffer_base_->get_pixel_to_565(pos);
      if (this->buffer_base_->transfer_buffer != nullptr) {
        if (this->is_18bit_()) {
          this->buffer_base_->transfer_buffer[transfer_index++] = (uint8_t)(color_to_write >> 14);
          this->buffer_base_->transfer_buffer[transfer_index++] = (uint8_t)(color_to_write >> 6);
          this->buffer_base_->transfer_buffer[transfer_index++] = (uint8_t)(color_to_write >> 2);
        } else {
          this->buffer_base_->transfer_buffer[transfer_index++] = (uint8_t)(color_to_write >> 8);
          this->buffer_base_->transfer_buffer[transfer_index++] = (uint8_t) color_to_write;
        }

        if (transfer_index == this->buffer_base_->transfer_buffer_size) {
          this->write_array(this->buffer_base_->transfer_buffer, transfer_index);
          transfer_index = 0;
        }

      } else {
        if (this->is_18bit_()) {
          this->write_byte(color_to_write >> 14);
          this->write_byte(color_to_write >> 6);
          this->write_byte(color_to_write << 2);
        } else {
          this->write_byte16(color_to_write);
        }
      }
    }
  }
  if (transfer_index != 0) {
    this->write_array(this->buffer_base_->transfer_buffer, transfer_index);
  }
#endif

  this->end_data_();
}

bool HOT ILI9341Display::is_18bit_() {
  return false;
  //  return this->get_buffer_type() != display::BufferType::BUFFER_TYPE_565;
}

void ILI9341Display::display_clear() { this->fill(display::COLOR_OFF); }
void ILI9341Display::fill(Color color) { this->fill_buffer(color); }

void ILI9341Display::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}

void ILI9341Display::end_command_() { this->disable(); }
void ILI9341Display::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void ILI9341Display::end_data_() { this->disable(); }

void ILI9341Display::init_lcd_(const uint8_t *init_cmd) {
  uint8_t cmd, x, num_args;
  const uint8_t *addr = init_cmd;
  while ((cmd = pgm_read_byte(addr++)) > 0) {
    x = pgm_read_byte(addr++);
    num_args = x & 0x7F;
    send_command(cmd, addr, num_args);
    addr += num_args;
    if (x & 0x80)
      delay(150);  // NOLINT
  }
}

void ILI9341Display::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h) {
  uint16_t x2 = (x1 + w - 1), y2 = (y1 + h - 1);
  this->command(ILI9341_CASET);  // Column address set
  this->start_data_();
  this->write_byte(x1 >> 8);
  this->write_byte(x1);
  this->write_byte(x2 >> 8);
  this->write_byte(x2);
  this->end_data_();
  this->command(ILI9341_PASET);  // Row address set
  this->start_data_();
  this->write_byte(y1 >> 8);
  this->write_byte(y1);
  this->write_byte(y2 >> 8);
  this->write_byte(y2);
  this->end_data_();
  this->command(ILI9341_RAMWR);  // Write to RAM
}

void ILI9341Display::invert_display_(bool invert) { this->command(invert ? ILI9341_INVON : ILI9341_INVOFF); }

int ILI9341Display::get_width_internal() { return this->get_device_width(); }
int ILI9341Display::get_height_internal() { return this->get_device_height(); }

//   M5Stack display
void ILI9341M5Stack::initialize() {
  this->init_lcd_(INITCMD_M5STACK);
  if (this->get_width_internal() == 0)
    this->set_device_width(240);

  if (this->get_height_internal() == 0)
    this->set_device_height(320);

  this->invert_display_(true);
  this->fill(COLOR_BLACK);
}

//   24_TFT display
void ILI9341TFT24::initialize() {
  if (this->is_18bit_()) {
    this->driver_right_bit_aligned_ = false;
    this->init_lcd_(INITCMD_TFT_18);
  } else {
    this->driver_right_bit_aligned_ = true;
    this->init_lcd_(INITCMD_TFT);
  }

  if (this->get_width_internal() == 0)
    this->set_device_width(240);

  if (this->get_height_internal() == 0)
    this->set_device_height(320);

  ESP_LOGD(TAG, "initialize %d/%d", this->get_width_internal(), this->get_height_internal());

  if (!this->buffer_base_->colors_is_set)
    this->buffer_base_->set_colors(this->get_model_colors());

  if (!this->buffer_base_->index_size_is_set)
    this->buffer_base_->set_index_size(this->buffer_base_->get_color_count());

  this->set_driver_right_bit_aligned(this->driver_right_bit_aligned_);

  bool res = this->init_buffer(this->get_width_internal(), this->get_height_internal());
  if (!res) {
    ESP_LOGE(TAG, "Could not allocate buffer space. Consider changing the buffer type or resolution!");
    this->mark_failed();
    return;
  }

  if (this->is_18bit_() && this->buffer_base_->transfer_buffer_size % 3 != 0) {
    uint8_t quotient = (this->buffer_base_->transfer_buffer_size / 3) + 1;
    this->buffer_base_->transfer_buffer_size = quotient * 3;
  }

  this->buffer_base_->transfer_buffer = new_buffer<uint8_t>(this->buffer_base_->transfer_buffer_size);
  this->fill(COLOR_BLACK);
}

}  // namespace ili9341
}  // namespace esphome
