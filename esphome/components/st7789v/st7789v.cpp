#include "st7789v.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7789v {

static const char *const TAG = "st7789v";

void ST7789V::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPI ST7789V...");
  this->spi_setup();
  this->dc_pin_->setup();  // OUTPUT

  this->init_reset_();

  this->write_command_(ST7789_SLPOUT);  // Sleep out
  delay(120);                           // NOLINT

  this->write_command_(ST7789_NORON);  // Normal display mode on

  // *** display and color format setting ***
  this->write_command_(ST7789_MADCTL);
  this->write_data_(ST7789_MADCTL_COLOR_ORDER);

  // JLX240 display datasheet
  this->write_command_(0xB6);
  this->write_data_(0x0A);
  this->write_data_(0x82);

  this->write_command_(ST7789_COLMOD);
  this->write_data_(0x55);
  delay(10);

  // *** ST7789V Frame rate setting ***
  this->write_command_(ST7789_PORCTRL);
  this->write_data_(0x0c);
  this->write_data_(0x0c);
  this->write_data_(0x00);
  this->write_data_(0x33);
  this->write_data_(0x33);

  this->write_command_(ST7789_GCTRL);  // Voltages: VGH / VGL
  this->write_data_(0x35);

  // *** ST7789V Power setting ***
  this->write_command_(ST7789_VCOMS);
  this->write_data_(0x28);  // JLX240 display datasheet

  this->write_command_(ST7789_LCMCTRL);
  this->write_data_(0x0C);

  this->write_command_(ST7789_VDVVRHEN);
  this->write_data_(0x01);
  this->write_data_(0xFF);

  this->write_command_(ST7789_VRHS);  // voltage VRHS
  this->write_data_(0x10);

  this->write_command_(ST7789_VDVS);
  this->write_data_(0x20);

  this->write_command_(ST7789_FRCTRL2);
  this->write_data_(0x0f);

  this->write_command_(ST7789_PWCTRL1);
  this->write_data_(0xa4);
  this->write_data_(0xa1);

  // *** ST7789V gamma setting ***
  this->write_command_(ST7789_PVGAMCTRL);
  this->write_data_(0xd0);
  this->write_data_(0x00);
  this->write_data_(0x02);
  this->write_data_(0x07);
  this->write_data_(0x0a);
  this->write_data_(0x28);
  this->write_data_(0x32);
  this->write_data_(0x44);
  this->write_data_(0x42);
  this->write_data_(0x06);
  this->write_data_(0x0e);
  this->write_data_(0x12);
  this->write_data_(0x14);
  this->write_data_(0x17);

  this->write_command_(ST7789_NVGAMCTRL);
  this->write_data_(0xd0);
  this->write_data_(0x00);
  this->write_data_(0x02);
  this->write_data_(0x07);
  this->write_data_(0x0a);
  this->write_data_(0x28);
  this->write_data_(0x31);
  this->write_data_(0x54);
  this->write_data_(0x47);
  this->write_data_(0x0e);
  this->write_data_(0x1c);
  this->write_data_(0x17);
  this->write_data_(0x1b);
  this->write_data_(0x1e);

  this->write_command_(ST7789_INVON);

  // Clear display - ensures we do not see garbage at power-on
  this->draw_filled_rect_(0, 0, this->get_width_internal(), this->get_height_internal(), 0x0000);

  delay(120);  // NOLINT

  this->write_command_(ST7789_DISPON);  // Display on
  delay(120);                           // NOLINT

  backlight_(true);

  this->init_internal_(this->get_buffer_length_());
  memset(this->buffer_, 0x00, this->get_buffer_length_());
}

void ST7789V::dump_config() {
  LOG_DISPLAY("", "SPI ST7789V", this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
  if (this->model_ == ST7789V_MODEL_CUSTOM) {
    ESP_LOGCONFIG(TAG, "  Height Offset: %u", this->offset_height_);
    ESP_LOGCONFIG(TAG, "  Width Offset: %u", this->offset_width_);
  }
  ESP_LOGCONFIG(TAG, "  8-bit color mode: %s", YESNO(this->eightbitcolor_));
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  B/L Pin: ", this->backlight_pin_);
  LOG_UPDATE_INTERVAL(this);
}

float ST7789V::get_setup_priority() const { return setup_priority::PROCESSOR; }

void ST7789V::update() {
  this->do_update_();
  this->write_display_data();
}

void ST7789V::set_model(ST7789VModel model) {
  this->model_ = model;

  switch (this->model_) {
    case ST7789V_MODEL_TTGO_TDISPLAY_135_240:
      this->height_ = 240;
      this->width_ = 135;
      this->offset_height_ = 52;
      this->offset_width_ = 40;
      break;

    case ST7789V_MODEL_ADAFRUIT_FUNHOUSE_240_240:
      this->height_ = 240;
      this->width_ = 240;
      this->offset_height_ = 0;
      this->offset_width_ = 0;
      break;

    case ST7789V_MODEL_ADAFRUIT_RR_280_240:
      this->height_ = 280;
      this->width_ = 240;
      this->offset_height_ = 0;
      this->offset_width_ = 20;
      break;

    default:
      break;
  }
}

void ST7789V::write_display_data() {
  uint16_t x1 = this->offset_height_;
  uint16_t x2 = x1 + get_width_internal() - 1;
  uint16_t y1 = this->offset_width_;
  uint16_t y2 = y1 + get_height_internal() - 1;

  this->enable();

  // set column(x) address
  this->dc_pin_->digital_write(false);
  this->write_byte(ST7789_CASET);
  this->dc_pin_->digital_write(true);
  this->write_addr_(x1, x2);
  // set page(y) address
  this->dc_pin_->digital_write(false);
  this->write_byte(ST7789_RASET);
  this->dc_pin_->digital_write(true);
  this->write_addr_(y1, y2);
  // write display memory
  this->dc_pin_->digital_write(false);
  this->write_byte(ST7789_RAMWR);
  this->dc_pin_->digital_write(true);

  if (this->eightbitcolor_) {
    for (int line = 0; line < this->get_buffer_length_(); line = line + this->get_width_internal()) {
      for (int index = 0; index < this->get_width_internal(); ++index) {
        auto color = display::ColorUtil::color_to_565(
            display::ColorUtil::to_color(this->buffer_[index + line], display::ColorOrder::COLOR_ORDER_RGB,
                                         display::ColorBitness::COLOR_BITNESS_332, true));
        this->write_byte((color >> 8) & 0xff);
        this->write_byte(color & 0xff);
      }
    }
  } else {
    this->write_array(this->buffer_, this->get_buffer_length_());
  }

  this->disable();
}

void ST7789V::init_reset_() {
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

void ST7789V::backlight_(bool onoff) {
  if (this->backlight_pin_ != nullptr) {
    this->backlight_pin_->setup();
    this->backlight_pin_->digital_write(onoff);
  }
}

void ST7789V::write_command_(uint8_t value) {
  this->enable();
  this->dc_pin_->digital_write(false);
  this->write_byte(value);
  this->dc_pin_->digital_write(true);
  this->disable();
}

void ST7789V::write_data_(uint8_t value) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void ST7789V::write_addr_(uint16_t addr1, uint16_t addr2) {
  static uint8_t byte[4];
  byte[0] = (addr1 >> 8) & 0xFF;
  byte[1] = addr1 & 0xFF;
  byte[2] = (addr2 >> 8) & 0xFF;
  byte[3] = addr2 & 0xFF;

  this->dc_pin_->digital_write(true);
  this->write_array(byte, 4);
}

void ST7789V::write_color_(uint16_t color, uint16_t size) {
  static uint8_t byte[1024];
  int index = 0;
  for (int i = 0; i < size; i++) {
    byte[index++] = (color >> 8) & 0xFF;
    byte[index++] = color & 0xFF;
  }

  this->dc_pin_->digital_write(true);
  return write_array(byte, size * 2);
}

size_t ST7789V::get_buffer_length_() {
  if (this->eightbitcolor_) {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal());
  }
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) * 2;
}

// Draw a filled rectangle
// x1: Start X coordinate
// y1: Start Y coordinate
// x2: End X coordinate
// y2: End Y coordinate
// color: color
void ST7789V::draw_filled_rect_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
  this->enable();
  this->dc_pin_->digital_write(false);
  this->write_byte(ST7789_CASET);  // set column(x) address
  this->dc_pin_->digital_write(true);
  this->write_addr_(x1, x2);

  this->dc_pin_->digital_write(false);
  this->write_byte(ST7789_RASET);  // set Page(y) address
  this->dc_pin_->digital_write(true);
  this->write_addr_(y1, y2);
  this->dc_pin_->digital_write(false);
  this->write_byte(ST7789_RAMWR);  // begin a write to memory
  this->dc_pin_->digital_write(true);
  for (int i = x1; i <= x2; i++) {
    uint16_t size = y2 - y1 + 1;
    this->write_color_(color, size);
  }
  this->disable();
}

void HOT ST7789V::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;

  if (this->eightbitcolor_) {
    auto color332 = display::ColorUtil::color_to_332(color);
    uint32_t pos = (x + y * this->get_width_internal());
    this->buffer_[pos] = color332;
  } else {
    auto color565 = display::ColorUtil::color_to_565(color);
    uint32_t pos = (x + y * this->get_width_internal()) * 2;
    this->buffer_[pos++] = (color565 >> 8) & 0xff;
    this->buffer_[pos] = color565 & 0xff;
  }
}

const char *ST7789V::model_str_() {
  switch (this->model_) {
    case ST7789V_MODEL_TTGO_TDISPLAY_135_240:
      return "TTGO T-Display 135x240";
    case ST7789V_MODEL_ADAFRUIT_FUNHOUSE_240_240:
      return "Adafruit Funhouse 240x240";
    case ST7789V_MODEL_ADAFRUIT_RR_280_240:
      return "Adafruit Round-Rectangular 280x240";
    default:
      return "Custom";
  }
}

}  // namespace st7789v
}  // namespace esphome
