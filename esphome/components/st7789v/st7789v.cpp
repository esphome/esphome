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
  this->draw_filled_rect_(0, 0, 239, 319, 0x0000);

  delay(120);  // NOLINT

  this->write_command_(ST7789_DISPON);  // Display on
  delay(120);                           // NOLINT

  backlight_(true);

  this->init_internal_(this->get_buffer_length_());
  memset(this->buffer_, 0x00, this->get_buffer_length_());
}

void ST7789V::dump_config() {
  LOG_DISPLAY("", "SPI ST7789V", this);
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

void ST7789V::loop() {}

void ST7789V::write_display_data() {
  uint16_t x1 = 52;   // _offsetx
  uint16_t x2 = 186;  // _offsetx
  uint16_t y1 = 40;   // _offsety
  uint16_t y2 = 279;  // _offsety

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

  this->write_array(this->buffer_, this->get_buffer_length_());

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

int ST7789V::get_height_internal() {
  return 240;  // 320;
}

int ST7789V::get_width_internal() {
  return 135;  // 240;
}

size_t ST7789V::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) * 2;
}

// Draw a filled rectangle
// x1: Start X coordinate
// y1: Start Y coordinate
// x2: End X coordinate
// y2: End Y coordinate
// color: color
void ST7789V::draw_filled_rect_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
  // ESP_LOGD(TAG,"offset(x)=%d offset(y)=%d",dev->_offsetx,dev->_offsety);
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

  auto color565 = display::ColorUtil::color_to_565(color);

  uint16_t pos = (x + y * this->get_width_internal()) * 2;
  this->buffer_[pos++] = (color565 >> 8) & 0xff;
  this->buffer_[pos] = color565 & 0xff;
}

}  // namespace st7789v
}  // namespace esphome
