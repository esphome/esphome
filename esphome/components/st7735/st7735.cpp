#include "st7735.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace st7735 {

static const uint8_t ST_CMD_DELAY = 0x80;  // special signifier for command lists

static const uint8_t ST77XX_NOP = 0x00;
static const uint8_t ST77XX_SWRESET = 0x01;
static const uint8_t ST77XX_RDDID = 0x04;
static const uint8_t ST77XX_RDDST = 0x09;

static const uint8_t ST77XX_SLPIN = 0x10;
static const uint8_t ST77XX_SLPOUT = 0x11;
static const uint8_t ST77XX_PTLON = 0x12;
static const uint8_t ST77XX_NORON = 0x13;

static const uint8_t ST77XX_INVOFF = 0x20;
static const uint8_t ST77XX_INVON = 0x21;
static const uint8_t ST77XX_DISPOFF = 0x28;
static const uint8_t ST77XX_DISPON = 0x29;
static const uint8_t ST77XX_CASET = 0x2A;
static const uint8_t ST77XX_RASET = 0x2B;
static const uint8_t ST77XX_RAMWR = 0x2C;
static const uint8_t ST77XX_RAMRD = 0x2E;

static const uint8_t ST77XX_PTLAR = 0x30;
static const uint8_t ST77XX_TEOFF = 0x34;
static const uint8_t ST77XX_TEON = 0x35;
static const uint8_t ST77XX_MADCTL = 0x36;
static const uint8_t ST77XX_COLMOD = 0x3A;

static const uint8_t ST77XX_MADCTL_MY = 0x80;
static const uint8_t ST77XX_MADCTL_MX = 0x40;
static const uint8_t ST77XX_MADCTL_MV = 0x20;
static const uint8_t ST77XX_MADCTL_ML = 0x10;
static const uint8_t ST77XX_MADCTL_RGB = 0x00;

static const uint8_t ST77XX_RDID1 = 0xDA;
static const uint8_t ST77XX_RDID2 = 0xDB;
static const uint8_t ST77XX_RDID3 = 0xDC;
static const uint8_t ST77XX_RDID4 = 0xDD;

// Some register settings
static const uint8_t ST7735_MADCTL_BGR = 0x08;
static const uint8_t ST7735_MADCTL_MH = 0x04;

static const uint8_t ST7735_FRMCTR1 = 0xB1;
static const uint8_t ST7735_FRMCTR2 = 0xB2;
static const uint8_t ST7735_FRMCTR3 = 0xB3;
static const uint8_t ST7735_INVCTR = 0xB4;
static const uint8_t ST7735_DISSET5 = 0xB6;

static const uint8_t ST7735_PWCTR1 = 0xC0;
static const uint8_t ST7735_PWCTR2 = 0xC1;
static const uint8_t ST7735_PWCTR3 = 0xC2;
static const uint8_t ST7735_PWCTR4 = 0xC3;
static const uint8_t ST7735_PWCTR5 = 0xC4;
static const uint8_t ST7735_VMCTR1 = 0xC5;

static const uint8_t ST7735_PWCTR6 = 0xFC;

static const uint8_t ST7735_GMCTRP1 = 0xE0;
static const uint8_t ST7735_GMCTRN1 = 0xE1;

// clang-format off
static const uint8_t PROGMEM
  BCMD[] = {                        // Init commands for 7735B screens
    18,                             // 18 commands in list:
    ST77XX_SWRESET,   ST_CMD_DELAY, //  1: Software reset, no args, w/delay
      50,                           //     50 ms delay
    ST77XX_SLPOUT,    ST_CMD_DELAY, //  2: Out of sleep mode, no args, w/delay
      255,                          //     255 = max (500 ms) delay
    ST77XX_COLMOD,  1+ST_CMD_DELAY, //  3: Set color mode, 1 arg + delay:
      0x05,                         //     16-bit color
      10,                           //     10 ms delay
    ST7735_FRMCTR1, 3+ST_CMD_DELAY, //  4: Frame rate control, 3 args + delay:
      0x00,                         //     fastest refresh
      0x06,                         //     6 lines front porch
      0x03,                         //     3 lines back porch
      10,                           //     10 ms delay
    ST77XX_MADCTL,  1,              //  5: Mem access ctl (directions), 1 arg:
      0x08,                         //     Row/col addr, bottom-top refresh
    ST7735_DISSET5, 2,              //  6: Display settings #5, 2 args:
      0x15,                         //     1 clk cycle nonoverlap, 2 cycle gate
                                    //     rise, 3 cycle osc equalize
      0x02,                         //     Fix on VTL
    ST7735_INVCTR,  1,              //  7: Display inversion control, 1 arg:
      0x0,                          //     Line inversion
    ST7735_PWCTR1,  2+ST_CMD_DELAY, //  8: Power control, 2 args + delay:
      0x02,                         //     GVDD = 4.7V
      0x70,                         //     1.0uA
      10,                           //     10 ms delay
    ST7735_PWCTR2,  1,              //  9: Power control, 1 arg, no delay:
      0x05,                         //     VGH = 14.7V, VGL = -7.35V
    ST7735_PWCTR3,  2,              // 10: Power control, 2 args, no delay:
      0x01,                         //     Opamp current small
      0x02,                         //     Boost frequency
    ST7735_VMCTR1,  2+ST_CMD_DELAY, // 11: Power control, 2 args + delay:
      0x3C,                         //     VCOMH = 4V
      0x38,                         //     VCOML = -1.1V
      10,                           //     10 ms delay
    ST7735_PWCTR6,  2,              // 12: Power control, 2 args, no delay:
      0x11, 0x15,
    ST7735_GMCTRP1,16,              // 13: Gamma Adjustments (pos. polarity), 16 args + delay:
      0x09, 0x16, 0x09, 0x20,       //     (Not entirely necessary, but provides
      0x21, 0x1B, 0x13, 0x19,       //      accurate colors)
      0x17, 0x15, 0x1E, 0x2B,
      0x04, 0x05, 0x02, 0x0E,
    ST7735_GMCTRN1,16+ST_CMD_DELAY, // 14: Gamma Adjustments (neg. polarity), 16 args + delay:
      0x0B, 0x14, 0x08, 0x1E,       //     (Not entirely necessary, but provides
      0x22, 0x1D, 0x18, 0x1E,       //      accurate colors)
      0x1B, 0x1A, 0x24, 0x2B,
      0x06, 0x06, 0x02, 0x0F,
      10,                           //     10 ms delay
    ST77XX_CASET,   4,              // 15: Column addr set, 4 args, no delay:
      0x00, 0x02,                   //     XSTART = 2
      0x00, 0x81,                   //     XEND = 129
    ST77XX_RASET,   4,              // 16: Row addr set, 4 args, no delay:
      0x00, 0x02,                   //     XSTART = 1
      0x00, 0x81,                   //     XEND = 160
    ST77XX_NORON,     ST_CMD_DELAY, // 17: Normal display on, no args, w/delay
      10,                           //     10 ms delay
    ST77XX_DISPON,    ST_CMD_DELAY, // 18: Main screen turn on, no args, delay
      255 },                        //     255 = max (500 ms) delay

  RCMD1[] = {                       // 7735R init, part 1 (red or green tab)
    15,                             // 15 commands in list:
    ST77XX_SWRESET,   ST_CMD_DELAY, //  1: Software reset, 0 args, w/delay
      150,                          //     150 ms delay
    ST77XX_SLPOUT,    ST_CMD_DELAY, //  2: Out of sleep mode, 0 args, w/delay
      255,                          //     500 ms delay
    ST7735_FRMCTR1, 3,              //  3: Framerate ctrl - normal mode, 3 arg:
      0x01, 0x2C, 0x2D,             //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR2, 3,              //  4: Framerate ctrl - idle mode, 3 args:
      0x01, 0x2C, 0x2D,             //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR3, 6,              //  5: Framerate - partial mode, 6 args:
      0x01, 0x2C, 0x2D,             //     Dot inversion mode
      0x01, 0x2C, 0x2D,             //     Line inversion mode
    ST7735_INVCTR,  1,              //  6: Display inversion ctrl, 1 arg:
      0x07,                         //     No inversion
    ST7735_PWCTR1,  3,              //  7: Power control, 3 args, no delay:
      0xA2,
      0x02,                         //     -4.6V
      0x84,                         //     AUTO mode
    ST7735_PWCTR2,  1,              //  8: Power control, 1 arg, no delay:
      0xC5,                         //     VGH25=2.4C VGSEL=-10 VGH=3 * AVDD
    ST7735_PWCTR3,  2,              //  9: Power control, 2 args, no delay:
      0x0A,                         //     Opamp current small
      0x00,                         //     Boost frequency
    ST7735_PWCTR4,  2,              // 10: Power control, 2 args, no delay:
      0x8A,                         //     BCLK/2,
      0x2A,                         //     opamp current small & medium low
    ST7735_PWCTR5,  2,              // 11: Power control, 2 args, no delay:
      0x8A, 0xEE,
    ST7735_VMCTR1,  1,              // 12: Power control, 1 arg, no delay:
      0x0E,
    ST77XX_INVOFF,  0,              // 13: Don't invert display, no args
    ST77XX_MADCTL,  1,              // 14: Mem access ctl (directions), 1 arg:
      0xC8,                         //     row/col addr, bottom-top refresh
    ST77XX_COLMOD,  1,              // 15: set color mode, 1 arg, no delay:
      0x05 },                       //     16-bit color

  RCMD2GREEN[] = {                  // 7735R init, part 2 (green tab only)
    2,                              //  2 commands in list:
    ST77XX_CASET,   4,              //  1: Column addr set, 4 args, no delay:
      0x00, 0x02,                   //     XSTART = 0
      0x00, 0x7F+0x02,              //     XEND = 127
    ST77XX_RASET,   4,              //  2: Row addr set, 4 args, no delay:
      0x00, 0x01,                   //     XSTART = 0
      0x00, 0x9F+0x01 },            //     XEND = 159

  RCMD2RED[] = {                    // 7735R init, part 2 (red tab only)
    2,                              //  2 commands in list:
    ST77XX_CASET,   4,              //  1: Column addr set, 4 args, no delay:
      0x00, 0x00,                   //     XSTART = 0
      0x00, 0x7F,                   //     XEND = 127
    ST77XX_RASET,   4,              //  2: Row addr set, 4 args, no delay:
      0x00, 0x00,                   //     XSTART = 0
      0x00, 0x9F },                 //     XEND = 159

  RCMD2GREEN144[] = {               // 7735R init, part 2 (green 1.44 tab)
    2,                              //  2 commands in list:
    ST77XX_CASET,   4,              //  1: Column addr set, 4 args, no delay:
      0x00, 0x00,                   //     XSTART = 0
      0x00, 0x7F,                   //     XEND = 127
    ST77XX_RASET,   4,              //  2: Row addr set, 4 args, no delay:
      0x00, 0x00,                   //     XSTART = 0
      0x00, 0x7F },                 //     XEND = 127

  RCMD2GREEN160X80[] = {            // 7735R init, part 2 (mini 160x80)
    2,                              //  2 commands in list:
    ST77XX_CASET,   4,              //  1: Column addr set, 4 args, no delay:
      0x00, 0x00,                   //     XSTART = 0
      0x00, 0x4F,                   //     XEND = 79
    ST77XX_RASET,   4,              //  2: Row addr set, 4 args, no delay:
      0x00, 0x00,                   //     XSTART = 0
      0x00, 0x9F },                 //     XEND = 159

  RCMD3[] = {                       // 7735R init, part 3 (red or green tab)
    4,                              //  4 commands in list:
    ST7735_GMCTRP1, 16      ,       //  1: Gamma Adjustments (pos. polarity), 16 args + delay:
      0x02, 0x1c, 0x07, 0x12,       //     (Not entirely necessary, but provides
      0x37, 0x32, 0x29, 0x2d,       //      accurate colors)
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16      ,       //  2: Gamma Adjustments (neg. polarity), 16 args + delay:
      0x03, 0x1d, 0x07, 0x06,       //     (Not entirely necessary, but provides
      0x2E, 0x2C, 0x29, 0x2D,       //      accurate colors)
      0x2E, 0x2E, 0x37, 0x3F,
      0x00, 0x00, 0x02, 0x10,
    ST77XX_NORON,     ST_CMD_DELAY, //  3: Normal display on, no args, w/delay
      10,                           //     10 ms delay
    ST77XX_DISPON,    ST_CMD_DELAY, //  4: Main screen turn on, no args w/delay
      100 };                        //     100 ms delay

  // Pre-computed lookup table for fast RGB332 to RGB565 color space
  static constexpr uint16_t RGB332_TO_565_LOOKUP_TABLE[256] = {
    0x0000, 0x000a, 0x0015, 0x001f, 0x0120, 0x012a, 0x0135, 0x013f, 0x0240, 0x024a, 0x0255, 0x025f, 0x0360, 0x036a,
    0x0375, 0x037f, 0x0480, 0x048a, 0x0495, 0x049f, 0x05a0, 0x05aa, 0x05b5, 0x05bf, 0x06c0, 0x06ca, 0x06d5, 0x06df,
    0x07e0, 0x07ea, 0x07f5, 0x07ff, 0x2000, 0x200a, 0x2015, 0x201f, 0x2120, 0x212a, 0x2135, 0x213f, 0x2240, 0x224a,
    0x2255, 0x225f, 0x2360, 0x236a, 0x2375, 0x237f, 0x2480, 0x248a, 0x2495, 0x249f, 0x25a0, 0x25aa, 0x25b5, 0x25bf,
    0x26c0, 0x26ca, 0x26d5, 0x26df, 0x27e0, 0x27ea, 0x27f5, 0x27ff, 0x4800, 0x480a, 0x4815, 0x481f, 0x4920, 0x492a,
    0x4935, 0x493f, 0x4a40, 0x4a4a, 0x4a55, 0x4a5f, 0x4b60, 0x4b6a, 0x4b75, 0x4b7f, 0x4c80, 0x4c8a, 0x4c95, 0x4c9f,
    0x4da0, 0x4daa, 0x4db5, 0x4dbf, 0x4ec0, 0x4eca, 0x4ed5, 0x4edf, 0x4fe0, 0x4fea, 0x4ff5, 0x4fff, 0x6800, 0x680a,
    0x6815, 0x681f, 0x6920, 0x692a, 0x6935, 0x693f, 0x6a40, 0x6a4a, 0x6a55, 0x6a5f, 0x6b60, 0x6b6a, 0x6b75, 0x6b7f,
    0x6c80, 0x6c8a, 0x6c95, 0x6c9f, 0x6da0, 0x6daa, 0x6db5, 0x6dbf, 0x6ec0, 0x6eca, 0x6ed5, 0x6edf, 0x6fe0, 0x6fea,
    0x6ff5, 0x6fff, 0x9000, 0x900a, 0x9015, 0x901f, 0x9120, 0x912a, 0x9135, 0x913f, 0x9240, 0x924a, 0x9255, 0x925f,
    0x9360, 0x936a, 0x9375, 0x937f, 0x9480, 0x948a, 0x9495, 0x949f, 0x95a0, 0x95aa, 0x95b5, 0x95bf, 0x96c0, 0x96ca,
    0x96d5, 0x96df, 0x97e0, 0x97ea, 0x97f5, 0x97ff, 0xb000, 0xb00a, 0xb015, 0xb01f, 0xb120, 0xb12a, 0xb135, 0xb13f,
    0xb240, 0xb24a, 0xb255, 0xb25f, 0xb360, 0xb36a, 0xb375, 0xb37f, 0xb480, 0xb48a, 0xb495, 0xb49f, 0xb5a0, 0xb5aa,
    0xb5b5, 0xb5bf, 0xb6c0, 0xb6ca, 0xb6d5, 0xb6df, 0xb7e0, 0xb7ea, 0xb7f5, 0xb7ff, 0xd800, 0xd80a, 0xd815, 0xd81f,
    0xd920, 0xd92a, 0xd935, 0xd93f, 0xda40, 0xda4a, 0xda55, 0xda5f, 0xdb60, 0xdb6a, 0xdb75, 0xdb7f, 0xdc80, 0xdc8a,
    0xdc95, 0xdc9f, 0xdda0, 0xddaa, 0xddb5, 0xddbf, 0xdec0, 0xdeca, 0xded5, 0xdedf, 0xdfe0, 0xdfea, 0xdff5, 0xdfff,
    0xf800, 0xf80a, 0xf815, 0xf81f, 0xf920, 0xf92a, 0xf935, 0xf93f, 0xfa40, 0xfa4a, 0xfa55, 0xfa5f, 0xfb60, 0xfb6a,
    0xfb75, 0xfb7f, 0xfc80, 0xfc8a, 0xfc95, 0xfc9f, 0xfda0, 0xfdaa, 0xfdb5, 0xfdbf, 0xfec0, 0xfeca, 0xfed5, 0xfedf,
    0xffe0, 0xffea, 0xfff5, 0xffff};

// clang-format on
static const char *TAG = "st7735";

ST7735::ST7735(ST7735Model model, int width, int height, int colstart, int rowstart, boolean eightbitcolor) {
  model_ = model;
  this->width_ = width;
  this->height_ = height;
  this->colstart_ = colstart;
  this->rowstart_ = rowstart;
  this->eightbitcolor_ = eightbitcolor;
}

void ST7735::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ST7735...");
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

  display_init_(RCMD1);

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

  // Black tab, change MADCTL color filter
  if ((this->model_ == INITR_BLACKTAB) || (this->model_ == INITR_MINI_160X80)) {
    uint8_t data = 0xC0;
    sendcommand_(ST77XX_MADCTL, &data, 1);
  }

  this->init_internal_(this->get_buffer_length());
  memset(this->buffer_, 0x00, this->get_buffer_length());
}

void ST7735::update() {
  this->do_update_();
  this->write_display_data_();
}

int ST7735::get_height_internal() { return height_; }

int ST7735::get_width_internal() { return width_; }

size_t ST7735::get_buffer_length() {
  if (eightbitcolor_) {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal());
  }
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) * 2;
}

void HOT ST7735::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;

  if (this->eightbitcolor_) {
    // 8-Bit color space is in BGR332
    const uint32_t color332 = color.to_rgb_332();
    uint16_t pos = (x + y * this->get_width_internal());
    this->buffer_[pos++] = color332;
  } else {
    // 16-bit color-space is in BGR565
    const uint32_t color565 = color.to_bgr_565();
    uint16_t pos = (x + y * this->get_width_internal()) * 2;
    this->buffer_[pos++] = (color565 >> 8) & 0xff;
    this->buffer_[pos] = color565 & 0xff;
  }
}

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
  ESP_LOGD(TAG, "  Buffer Size: %zu", this->get_buffer_length());
  ESP_LOGD(TAG, "  Height: %d", this->height_);
  ESP_LOGD(TAG, "  Width: %d", this->width_);
  ESP_LOGD(TAG, "  ColStart: %d", this->colstart_);
  ESP_LOGD(TAG, "  RowStart: %d", this->rowstart_);
  LOG_UPDATE_INTERVAL(this);
}

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
    this->transfer_byte(pgm_read_byte(data_bytes++));  // write byte - SPI library
  }
  this->cs_->digital_write(true);
  this->disable();
}

void HOT ST7735::write_display_data_() {
  uint16_t offsetx = colstart_;
  uint16_t offsety = rowstart_;

  uint16_t x1 = offsetx;
  uint16_t x2 = x1 + get_width_internal() - 1;
  uint16_t y1 = offsety;
  uint16_t y2 = y1 + get_height_internal() - 1;

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

  if (this->eightbitcolor_) {
    // In 8-bit color mode (BGR332) we can't just write the array, but have to convert the 8-bit colorspace buffer back
    // to BGR565 16-bit colorspace. Because this method is called every time the display refreshes, we have to be
    // performant, hence a precomputed lookup table is used here.
    for (int line = 0; line < this->get_buffer_length(); line = line + this->get_width_internal()) {
      for (int index = 0; index < this->get_width_internal(); ++index) {
        auto color = RGB332_TO_565_LOOKUP_TABLE[this->buffer_[index + line]];
        this->write_byte((color >> 8) & 0xff);
        this->write_byte(color & 0xff);
      }
    }
  } else {
    this->write_array(this->buffer_, this->get_buffer_length());
  }

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

void ST7735::spi_master_write_color_(uint16_t color, uint16_t size) {
  static uint8_t BYTE[1024];
  int index = 0;
  for (int i = 0; i < size; i++) {
    BYTE[index++] = (color >> 8) & 0xFF;
    BYTE[index++] = color & 0xFF;
  }

  this->dc_pin_->digital_write(true);
  return write_array(BYTE, size * 2);
}

}  // namespace st7735
}  // namespace esphome
