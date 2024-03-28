#include "waveshare_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace waveshare_epaper {

// It's worth adding some notes for this implementation
// - This display doesn't ship with a LUT, instead it relies on the internal values set during OTP
// - This display inverts Black & White in memory, requiring a different implementation for draw_absolute_pixel_internal
// - The reference implementation by the vendor points to
// https://github.com/ZinggJM/GxEPD2/blob/220fc5845c08b83c8dbac63e0cb83e1a774071ca/src/epd3c/GxEPD2_290_C90c.cpp
// - The datasheet is here
// https://github.com/WeActStudio/WeActStudio.EpaperModule/blob/master/Doc/ZJY128296-029EAAMFGN.pdf

static const char *const TAG = "weact_2.90_3c";

static const uint16_t HEIGHT = 296;
static const uint16_t WIDTH = 128;

// General Commands
static const uint8_t SW_RESET = 0x12;
static const uint8_t ACTIVATE = 0x20;
static const uint8_t WRITE_BLACK = 0x24;
static const uint8_t WRITE_COLOR = 0x26;
static const uint8_t SLEEP[] = {0x10, 0x01};
static const uint8_t UPDATE_FULL[] = {0x22, 0xF7};

// Configuration commands
static const uint8_t DRV_OUT_CTL[] = {0x01, 0x27, 0x01, 0x00};  // driver output control
static const uint8_t DATA_ENTRY[] = {0x11, 0x03};               // data entry mode
static const uint8_t BORDER_FULL[] = {0x3C, 0x05};              // border waveform
static const uint8_t TEMP_SENS[] = {0x18, 0x80};                // use internal temp sensor
static const uint8_t DISPLAY_UPDATE[] = {0x21, 0x00, 0x80};     // display update control

// For controlling which part of the image we want to write
static const uint8_t RAM_X_RANGE[] = {0x44, 0x00, WIDTH / 8u - 1};
static const uint8_t RAM_Y_RANGE[] = {0x45, 0x00, 0x00, (uint8_t) HEIGHT - 1, (uint8_t) (HEIGHT >> 8)};
static const uint8_t RAM_X_POS[] = {0x4E, 0x00};  // Always start at 0
static const uint8_t RAM_Y_POS = 0x4F;

#define SEND(x) this->cmd_data(x, sizeof(x))

// Basics

int WeActEPaper2P9In3C::get_width_internal() { return WIDTH; }
int WeActEPaper2P9In3C::get_height_internal() { return HEIGHT; }
uint32_t WeActEPaper2P9In3C::idle_timeout_() { return 2500; }

void WeActEPaper2P9In3C::dump_config() {
  LOG_DISPLAY("", "WeAct E-Paper (3 Color)", this)
  ESP_LOGCONFIG(TAG, "  Model: 2.90in Red+Black");
  LOG_PIN("  CS Pin: ", this->cs_)
  LOG_PIN("  Reset Pin: ", this->reset_pin_)
  LOG_PIN("  DC Pin: ", this->dc_pin_)
  LOG_PIN("  Busy Pin: ", this->busy_pin_)
  LOG_UPDATE_INTERVAL(this)
}

// Device lifecycle

void WeActEPaper2P9In3C::setup() {
  setup_pins_();
  delay(20);
  this->send_reset_();
  // as a one-off delay this is not worth working around.
  delay(100);  // NOLINT
  this->wait_until_idle_();
  this->command(SW_RESET);
  this->wait_until_idle_();

  SEND(DRV_OUT_CTL);
  SEND(DATA_ENTRY);
  SEND(BORDER_FULL);
  SEND(TEMP_SENS);
  SEND(DISPLAY_UPDATE);

  this->wait_until_idle_();
}

void WeActEPaper2P9In3C::send_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(2);
    this->reset_pin_->digital_write(true);
  }
}

// must implement, but we override setup to have more control
void WeActEPaper2P9In3C::initialize() {}

void WeActEPaper2P9In3C::deep_sleep() { SEND(SLEEP); }

// Pixel stuff

// t and b are y positions, i.e. line numbers.
void WeActEPaper2P9In3C::set_window_(int t, int b) {
  SEND(RAM_X_RANGE);
  SEND(RAM_Y_RANGE);
  SEND(RAM_X_POS);

  uint8_t buffer[3];
  buffer[0] = RAM_Y_POS;
  buffer[1] = (uint8_t) t % 256;
  buffer[2] = (uint8_t) (t / 256);
  SEND(buffer);
}

// send the buffer starting on line `top`, up to line `bottom`.
void WeActEPaper2P9In3C::write_buffer_(int top, int bottom) {
  auto width_bytes = this->get_width_internal() / 8u;
  auto offset = top * width_bytes;
  auto length = (bottom - top) * width_bytes;

  this->wait_until_idle_();
  this->set_window_(top, bottom);

  this->command(WRITE_BLACK);
  this->start_data_();
  this->write_array(this->buffer_ + offset, length);
  this->end_data_();

  offset += this->get_buffer_length_() / 2u;
  this->command(WRITE_COLOR);
  this->start_data_();
  this->write_array(this->buffer_ + offset, length);
  this->end_data_();
}

void HOT WeActEPaper2P9In3C::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  const uint32_t pos = (x + y * this->get_width_internal()) / 8u;
  const uint8_t subpos = 0x80 >> (x & 0x07);

  // flip logic
  if (color == display::COLOR_OFF) {
    this->buffer_[pos] |= subpos;
  } else {
    this->buffer_[pos] &= ~subpos;
  }

  // draw red pixels only if the color contains red only
  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;
  if (((color.red > 0) && (color.green == 0) && (color.blue == 0))) {
    this->buffer_[pos + buf_half_len] |= subpos;
  } else {
    this->buffer_[pos + buf_half_len] &= ~subpos;
  }
}

void WeActEPaper2P9In3C::full_update_() {
  ESP_LOGI(TAG, "Performing full e-paper update.");
  this->write_buffer_(0, this->get_height_internal());
  SEND(UPDATE_FULL);
  this->command(ACTIVATE);  // don't wait here
  this->is_busy_ = false;
}

void WeActEPaper2P9In3C::display() {
  if (this->is_busy_ || (this->busy_pin_ != nullptr && this->busy_pin_->digital_read()))
    return;
  this->is_busy_ = true;
  this->full_update_();
}

}  // namespace waveshare_epaper
}  // namespace esphome
