#include "gc9a01.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

#include "gc9a01_defines.h"

namespace esphome {
namespace gc9a01 {

static const uint8_t MADCTL_MY = 0x80;   ///< Bottom to top
static const uint8_t MADCTL_MX = 0x40;   ///< Right to left
static const uint8_t MADCTL_MV = 0x20;   ///< Reverse Mode
static const uint8_t MADCTL_ML = 0x10;   ///< LCD refresh Bottom to top
static const uint8_t MADCTL_RGB = 0x00;  ///< Red-Green-Blue pixel order
static const uint8_t MADCTL_BGR = 0x08;  ///< Blue-Green-Red pixel order
static const uint8_t MADCTL_MH = 0x04;   ///< LCD refresh right to left

// clang-format off
static const uint8_t PROGMEM INITCMD[] = {
  GC9A01_INREGEN2, 0,
  0xEB, 1, 0x14,
  GC9A01_INREGEN1, 0,
  GC9A01_INREGEN2, 0,
  0xEB, 1, 0x14,
  0x84, 1, 0x40,
  0x85, 1, 0xFF,
  0x86, 1, 0xFF,
  0x87, 1, 0xFF,
  0x88, 1, 0x0A,
  0x89, 1, 0x21,
  0x8A, 1, 0x00,
  0x8B, 1, 0x80,
  0x8C, 1, 0x01,
  0x8D, 1, 0x01,
  0x8E, 1, 0xFF,
  0x8F, 1, 0xFF,
  0xB6, 2, 0x00, 0x00,
  GC9A01_MADCTL, 1, MADCTL_MX | MADCTL_BGR,
  GC9A01_PIXFMT, 1, 0x05,
  0x90, 4, 0x08, 0x08, 0x08, 0x08,
  0xBD, 1, 0x06,
  0xBC, 1, 0x00,
  0xFF, 3, 0x60, 0x01, 0x04,
  GC9A011_VREG1A, 0x13,
  GC9A011_VREG1B, 0x13,
  GC9A011_VREG2A, 0x22,
  0xBE, 1, 0x11,
  GC9A01_GMCTRN1, 2, 0x10, 0x0E,
  0xDF, 3, 0x21, 0x0c, 0x02,
  GC9A01_GAMMA1, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
  GC9A01_GAMMA2, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
  GC9A01_GAMMA3, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
  GC9A01_GAMMA4, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
  0xED, 2, 0x1B, 0x0B,
  0xAE, 1, 0x77,
  0xCD, 1, 0x63,
  0x70, 9, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03,
  GC9A01_FRAMERATE, 1, 0x34,
  0x62, 12, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70,
            0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
  0x63, 12, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70,
            0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
  0x64, 7, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
  0x66, 10, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00,
  0x67, 10, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,
  0x74, 7, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00,
  0x98, 2, 0x3e, 0x07,
  GC9A01_TEON, 0,
  GC9A01_INVON, 0,
  GC9A01_SLPOUT, 0x80, // Exit sleep
  GC9A01_DISPON, 0x80, // Display on
  0x00                  // End of list
};
// clang-format on

static const char *const TAG = "gc9a01";

static const uint8_t PROGMEM gc9a01_rotations[] = {
  MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR, // display::DISPLAY_ROTATION_0_DEGREES
  MADCTL_MX | MADCTL_BGR, // display::DISPLAY_ROTATION_90_DEGREES
  MADCTL_MV | MADCTL_BGR, // display::DISPLAY_ROTATION_180_DEGREES
  MADCTL_MY | MADCTL_BGR, // display::DISPLAY_ROTATION_270_DEGREES
};

GC9A01::GC9A01(int width, int height, int colstart, int rowstart)
    : colstart_(colstart), rowstart_(rowstart), width_(width), height_(height) {}

void GC9A01::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GC9A01...");
  this->spi_setup();

  this->dc_pin_->setup();  // OUTPUT
  this->cs_->setup();      // OUTPUT

  this->dc_pin_->digital_write(true);
  this->cs_->digital_write(true);

  this->init_reset_();
  delay(100);  // NOLINT

  ESP_LOGD(TAG, "  START");
  dump_config();
  ESP_LOGD(TAG, "  END");

  display_init_(INITCMD);

  ESP_LOGI(TAG, "Rotation: %d, %02x", this->rotation_, gc9a01_rotations[this->rotation_ / 90]);

  this->sendcommand_(GC9A01_MADCTL, &gc9a01_rotations[this->rotation_ / 90], 1);
}

void GC9A01::update() {
  this->do_update_();
}

void HOT GC9A01::draw_pixel_at(int x, int y, Color color) {
  if (x >= this->width_ || x < 0 || y >= this->height_ || y < 0)
    return;

  const uint32_t color565 = display::ColorUtil::color_to_565(color);

  uint8_t data[] = {
    uint8_t(color565 >> 8),
    uint8_t(color565)
  };

  this->draw_pixels_(x, y, 1, 1, data, sizeof(data), sizeof(data));
}

bool HOT GC9A01::draw_pixels_(int x, int y, int w, int h, const uint8_t *data, int data_line_size, int data_stride) {
  uint16_t x1 = this->colstart_ + x;
  uint16_t x2 = x1 + w - 1;
  uint16_t y1 = this->rowstart_ + y;
  uint16_t y2 = y1 + h - 1;

#if 0
  ESP_LOGI(TAG, "GC9A01 Draw Pixels: %dx%d/%dx%d, line_size=%d, stride=%d",
    x, y, w, h,
    data_line_size, data_stride);
#endif

  this->enable();

  // set column(x) address
  this->dc_pin_->digital_write(false);
  this->write_byte(GC9A01_CASET);
  this->dc_pin_->digital_write(true);
  this->spi_master_write_addr_(x1, x2);

  // set Page(y) address
  this->dc_pin_->digital_write(false);
  this->write_byte(GC9A01_PASET);
  this->dc_pin_->digital_write(true);
  this->spi_master_write_addr_(y1, y2);

  //  Memory Write
  this->dc_pin_->digital_write(false);
  this->write_byte(GC9A01_RAMWR);
  this->dc_pin_->digital_write(true);

  for (int i = 0; i < h; i++) {
    this->write_array(data + data_stride * i, data_line_size);
  }
  this->disable();

  return true;
}

void GC9A01::init_reset_() {
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

void GC9A01::display_init_(const uint8_t *addr) {
  uint8_t cmd, x, num_args;
  while ((cmd = progmem_read_byte(addr++)) > 0) {
    x = progmem_read_byte(addr++);
    num_args = x & 0x7F;
    this->sendcommand_(cmd, addr, num_args);
    addr += num_args;
    if (x & 0x80) {
      delay(150);  // NOLINT
    }
  }
}

void GC9A01::dump_config() {
  LOG_DISPLAY("", "GC9A01", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGD(TAG, "  Height: %d", this->height_);
  ESP_LOGD(TAG, "  Width: %d", this->width_);
  ESP_LOGD(TAG, "  OffsetX: %d", this->colstart_);
  ESP_LOGD(TAG, "  OffsetY: %d", this->rowstart_);
  LOG_UPDATE_INTERVAL(this);
}

void HOT GC9A01::writecommand_(uint8_t value) {
  this->enable();
  this->dc_pin_->digital_write(false);
  this->write_byte(value);
  this->dc_pin_->digital_write(true);
  this->disable();
}

void HOT GC9A01::writedata_(uint8_t value) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void HOT GC9A01::sendcommand_(uint8_t cmd, const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->writecommand_(cmd);
  this->senddata_(data_bytes, num_data_bytes);
}

void HOT GC9A01::senddata_(const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->dc_pin_->digital_write(true);  // pull DC high to indicate data
  this->cs_->digital_write(false);
  this->enable();
  for (uint8_t i = 0; i < num_data_bytes; i++) {
    this->write_byte(progmem_read_byte(data_bytes++));  // write byte - SPI library
  }
  this->cs_->digital_write(true);
  this->disable();
}

void GC9A01::spi_master_write_addr_(uint16_t addr1, uint16_t addr2) {
  static uint8_t byte[4];
  byte[0] = (addr1 >> 8) & 0xFF;
  byte[1] = addr1 & 0xFF;
  byte[2] = (addr2 >> 8) & 0xFF;
  byte[3] = addr2 & 0xFF;

  this->dc_pin_->digital_write(true);
  this->write_array(byte, 4);
}

}  // namespace gc9a01
}  // namespace esphome
