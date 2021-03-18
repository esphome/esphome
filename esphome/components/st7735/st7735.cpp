#include "st7735.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/display/bufferex_565.h"

namespace esphome {
namespace st7735 {

// clang-format on
static const char *TAG = "st7735";

ST7735::ST7735(ST7735Model model, int width, int height, int colstart, int rowstart, boolean usebgr) {
  this->model_ = model;
  this->width_ = width;
  this->height_ = height;
  this->colstart_ = colstart;
  this->rowstart_ = rowstart;
  this->usebgr_ = usebgr;
}

void ST7735::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ST7735... %d %d", this->width_, this->height_);

  this->spi_setup();

  this->dc_pin_->setup();
  this->cs_->setup();

  this->dc_pin_->digital_write(true);
  this->cs_->digital_write(true);

  this->init_reset_();
  delay(100);  // NOLINT

  if (this->is_18bit_()) {
    this->driver_right_bit_aligned_ = false;
    display_init_(RCMD18);
  } else {
    this->driver_right_bit_aligned_ = true;
    display_init_(RCMD1);
  }

  if (this->model_ == INITR_GREENTAB) {
    display_init_(RCMD2GREEN);
    colstart_ == 0 ? colstart_ = 2 : colstart_;
    rowstart_ == 0 ? rowstart_ = 1 : rowstart_;
  } else if ((this->model_ == INITR_144GREENTAB) || (this->model_ == INITR_HALLOWING)) {
    height_ == 0 ? height_ = ST7735_TFTHEIGHT_128 : height_;
    width_ == 0 ? width_ = ST7735_TFTWIDTH_128 : width_;
    display_init_(RCMD2GREEN144);
    colstart_ == 0 ? colstart_ = 2 : colstart_;
    rowstart_ == 0 ? rowstart_ = 3 : rowstart_;
  } else if (this->model_ == INITR_MINI_160X80) {
    height_ == 0 ? height_ = ST7735_TFTHEIGHT_160 : height_;
    width_ == 0 ? width_ = ST7735_TFTWIDTH_80 : width_;
    display_init_(RCMD2GREEN160X80);
    colstart_ = 24;
    rowstart_ = 0;  // For default rotation 0
  } else {
    // colstart, rowstart left at default '0' values
    display_init_(RCMD2RED);
  }
  display_init_(RCMD3);

  uint8_t data = 0;
  if (this->model_ != INITR_HALLOWING) {
    uint8_t data = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY;
  }
  if (this->usebgr_) {
    data = data | ST7735_MADCTL_BGR;
  } else {
    data = data | ST77XX_MADCTL_RGB;
  }
  sendcommand_(ST77XX_MADCTL, &data, 1);

  this->set_driver_right_bit_aligned(this->driver_right_bit_aligned_);

  bool res = this->init_buffer(this->width_, this->height_);
  if (!res) {
    ESP_LOGE(TAG, "Could not allocate buffer space. Consider changing the buffer type or resolution!");
    this->mark_failed();
    return;
  }

  this->fill(COLOR_BLACK);
}

bool HOT ST7735::is_18bit_() { return this->get_buffer_type() != display::BufferType::BUFFER_TYPE_565; }

void ST7735::update() {
  this->do_update_();
  this->display_buffer_();
  this->bufferex_base_->display();
}

int ST7735::get_height_internal() { return height_; }

int ST7735::get_width_internal() { return width_; }

void ST7735::init_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(1);
    // Trigger Reset
    this->reset_pin_->digital_write(false);
    delay(10);
    // Wake up
    this->reset_pin_->digital_write(true);
  }
}
const char *ST7735::model_str_() {
  switch (this->model_) {
    case INITR_GREENTAB:
      return "ST7735 GREENTAB";
    case INITR_REDTAB:
      return "ST7735 REDTAB";
    case INITR_BLACKTAB:
      return "ST7735 BLACKTAB";
    case INITR_MINI_160X80:
      return "ST7735 MINI160x80";
    default:
      return "Unknown";
  }
}

void ST7735::display_init_(const uint8_t *addr) {
  uint8_t num_commands, cmd, num_args;
  uint16_t ms;

  num_commands = pgm_read_byte(addr++);  // Number of commands to follow
  while (num_commands--) {               // For each command...
    cmd = pgm_read_byte(addr++);         // Read command
    num_args = pgm_read_byte(addr++);    // Number of args to follow
    ms = num_args & ST_CMD_DELAY;        // If hibit set, delay follows args
    num_args &= ~ST_CMD_DELAY;           // Mask out delay bit
    this->sendcommand_(cmd, addr, num_args);
    addr += num_args;

    if (ms) {
      ms = pgm_read_byte(addr++);  // Read post-command delay time (ms)
      if (ms == 255)
        ms = 500;  // If 255, delay for 500 ms
      delay(ms);
    }
  }
}

void ST7735::dump_config() {
  LOG_DISPLAY("", "ST7735", this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void ST7735::display_clear() { this->fill(display::COLOR_OFF); }

void HOT ST7735::writecommand_(uint8_t value) {
  this->enable();
  this->dc_pin_->digital_write(false);
  this->write_byte(value);
  this->dc_pin_->digital_write(true);
  this->disable();
}

void HOT ST7735::writedata_(uint8_t value) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void HOT ST7735::sendcommand_(uint8_t cmd, const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->writecommand_(cmd);
  this->senddata_(data_bytes, num_data_bytes);
}

void HOT ST7735::senddata_(const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->dc_pin_->digital_write(true);  // pull DC high to indicate data
  this->cs_->digital_write(false);
  this->enable();
  for (uint8_t i = 0; i < num_data_bytes; i++) {
    this->write_byte(pgm_read_byte(data_bytes++));  // write byte - SPI library
  }
  this->cs_->digital_write(true);
  this->disable();
}

void HOT ST7735::display_buffer_() {
  // ESP_LOGD(TAG, "Asked to write %d pixels", this->bufferex_base_->pixel_count_);

  const int w = this->bufferex_base_->x_high_ - this->bufferex_base_->x_low_ + 1;
  const int h = this->bufferex_base_->y_high_ - this->bufferex_base_->y_low_ + 1;
  const uint32_t start_pos = ((this->bufferex_base_->y_low_ * this->width_) + this->bufferex_base_->x_low_);

  set_addr_window_(this->bufferex_base_->x_low_ + colstart_, this->bufferex_base_->y_low_ + rowstart_, w, h);

  this->start_data_();

#ifdef USE_BUFFER_RGB565
  auto buff = static_cast<display::Bufferex565 *>(this->bufferex_base_);
  this->write_array16(buff->buffer_, this->bufferex_base_->get_buffer_length());
#else
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      uint32_t pos = start_pos + (row * width_) + col;

      uint32_t color_to_write = this->bufferex_base_->get_pixel_to_666(pos);

      if (this->is_18bit_()) {
        this->write_byte(color_to_write >> 14);
        this->write_byte(color_to_write >> 6);
        this->write_byte(color_to_write << 2);
      } else {
        this->write_byte16(color_to_write);
      }
    }
  }
#endif

  this->end_data_();

  this->bufferex_base_->pixel_count_ = 0;
}

void HOT ST7735::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void HOT ST7735::end_data_() { this->disable(); }

void HOT ST7735::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h) {
  uint16_t x2 = (x1 + w - 1), y2 = (y1 + h - 1);

  this->enable();

  // set column(x) address
  this->dc_pin_->digital_write(false);
  this->write_byte(ST77XX_CASET);
  this->dc_pin_->digital_write(true);
  this->spi_master_write_addr_(x1, x2);

  // set Page(y) address
  this->dc_pin_->digital_write(false);
  this->write_byte(ST77XX_RASET);
  this->dc_pin_->digital_write(true);
  this->spi_master_write_addr_(y1, y2);

  //  Memory Write
  this->dc_pin_->digital_write(false);
  this->write_byte(ST77XX_RAMWR);
  this->dc_pin_->digital_write(true);

  this->disable();
}

void ST7735::spi_master_write_addr_(uint16_t addr1, uint16_t addr2) {
  static uint8_t BYTE[4];
  BYTE[0] = (addr1 >> 8) & 0xFF;
  BYTE[1] = addr1 & 0xFF;
  BYTE[2] = (addr2 >> 8) & 0xFF;
  BYTE[3] = addr2 & 0xFF;

  this->dc_pin_->digital_write(true);
  this->write_array(BYTE, 4);
}

}  // namespace st7735
}  // namespace esphome
