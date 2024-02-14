#include "waveshare_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "weact_3c";

// General Commands
static const uint8_t SW_RESET = 0x12;
static const uint8_t ACTIVATE = 0x20;
static const uint8_t WRITE_BLACK = 0x24;
static const uint8_t WRITE_COLOR = 0x26;
static const uint8_t SLEEP[] = {0x10, 0x01};
static const uint8_t UPDATE_FULL[] = {0x22, 0xF7};

// Configuration commands
static const uint8_t DATA_ENTRY[] = {0x11, 0x03};            // data entry mode
static const uint8_t BORDER_FULL[] = {0x3C, 0x05};           // border waveform
static const uint8_t TEMP_SENS[] = {0x18, 0x80};             // use internal temp sensor
static const uint8_t DISPLAY_UPDATE[] = {0x21, 0x00, 0x80};  // display update control

// For controlling which part of the image we want to write
static const uint8_t RAM_X_POS[] = {0x4E, 0x00};  // Always start at 0
static const uint8_t RAM_Y_POS = 0x4F;

#define SEND(x) this->cmd_data(x, sizeof(x))

WeActEPaper3C::WeActEPaper3C(WeActEPaper3CModel model) : model_(model) {}

int WeActEPaper3C::get_width_internal() {
  switch (this->model_) {
    case WEACT_EPAPER_2_13_IN_3C:
      return 122;
    case WEACT_EPAPER_4_20_IN_3C:
      return 400;
    case WEACT_EPAPER_2_90_IN_3C:
    default:
      return 128;
  }
}

int WeActEPaper3C::get_height_internal() {
  switch (this->model_) {
    case WEACT_EPAPER_2_13_IN_3C:
      return 250;
    case WEACT_EPAPER_4_20_IN_3C:
      return 300;
    case WEACT_EPAPER_2_90_IN_3C:
    default:
      return 296;
  }
}

uint32_t WeActEPaper3C::idle_timeout_() { return 20000; }

void WeActEPaper3C::dump_config() {
  LOG_DISPLAY("", "WeAct E-Paper (3 Color)", this);
  switch (this->model_) {
    case WEACT_EPAPER_2_13_IN_3C:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in 3-Color");
      break;
    case WEACT_EPAPER_4_20_IN_3C:
      ESP_LOGCONFIG(TAG, "  Model: 4.20in 3-Color");
      break;
    case WEACT_EPAPER_2_90_IN_3C:
    default:
      ESP_LOGCONFIG(TAG, "  Model: 2.90in 3-Color");
      break;
  }
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WeActEPaper3C::setup() {
  this->init_internal_(this->get_buffer_length_());
  this->spi_setup();
  setup_pins_();

  if (this->buffer_ != nullptr) {
    memset(this->buffer_, 0x00, this->get_buffer_length_());
  }
}

void WeActEPaper3C::send_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(2);
    this->reset_pin_->digital_write(true);
  }
}

void WeActEPaper3C::initialize() {}

void WeActEPaper3C::deep_sleep() { SEND(SLEEP); }

void WeActEPaper3C::set_window_(int t, int b) {
  const uint8_t ram_x_range[] = {0x44, 0x00, (uint8_t) (this->get_width_internal() / 8u - 1)};
  const uint8_t ram_y_range[] = {0x45, 0x00, 0x00, (uint8_t) (this->get_height_internal() - 1),
                                 (uint8_t) ((this->get_height_internal() - 1) >> 8)};
  this->cmd_data(ram_x_range, sizeof(ram_x_range));
  this->cmd_data(ram_y_range, sizeof(ram_y_range));

  SEND(RAM_X_POS);

  uint8_t buffer[3];
  buffer[0] = RAM_Y_POS;
  buffer[1] = (uint8_t) t % 256;
  buffer[2] = (uint8_t) (t / 256);
  SEND(buffer);
}

void WeActEPaper3C::write_buffer_(int top, int bottom) {
  auto width_bytes = this->get_width_internal() / 8u;
  auto offset = top * width_bytes;
  auto length = (bottom - top) * width_bytes;
  auto red_offset = this->get_buffer_length_() / 2u;

  this->wait_until_idle_();
  this->set_window_(top, bottom);

  // RED plane first
  this->command(WRITE_COLOR);
  this->start_data_();
  this->write_array(this->buffer_ + offset + red_offset, length);
  this->end_data_();

  // BLACK plane second
  this->command(WRITE_BLACK);
  this->start_data_();
  this->write_array(this->buffer_ + offset, length);
  this->end_data_();
}

void HOT WeActEPaper3C::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || y < 0 || x >= this->get_width_internal() || y >= this->get_height_internal())
    return;

  const uint32_t pos = (x + y * this->get_width_internal()) / 8u;
  const uint8_t bit = 0x80 >> (x & 0x07);
  const uint32_t red_offset = this->get_buffer_length_() / 2u;
  bool is_red = ((color.red > 0) && (color.green == 0) && (color.blue == 0));

  // BLACK PLANE: 0=Black, 1=White
  // We want Black Ink if color is Active AND NOT Red.
  if (color.is_on() && !is_red) {
    this->buffer_[pos] &= ~bit;  // Black Ink
  } else {
    this->buffer_[pos] |= bit;  // White Paper
  }

  // RED PLANE: 1=Red, 0=None
  if (is_red) {
    this->buffer_[pos + red_offset] |= bit;
  } else {
    this->buffer_[pos + red_offset] &= ~bit;
  }
}

void WeActEPaper3C::full_update_() {
  ESP_LOGD(TAG, "Performing full e-paper update.");

  this->wait_until_idle_();

  this->write_buffer_(0, this->get_height_internal());
  SEND(UPDATE_FULL);
  this->command(ACTIVATE);
  // Non-blocking: wait in loop()
}

void WeActEPaper3C::display() {
  if (!this->initialized_) {
    this->initialized_ = true;

    this->send_reset_();
    delay(10);

    this->command(SW_RESET);
    this->wait_until_idle_();

    const uint8_t drv_out_ctl[] = {0x01, (uint8_t) ((this->get_height_internal() - 1) & 0xFF),
                                   (uint8_t) (((this->get_height_internal() - 1) >> 8) & 0xFF), 0x00};
    this->cmd_data(drv_out_ctl, sizeof(drv_out_ctl));

    SEND(DATA_ENTRY);
    SEND(BORDER_FULL);
    SEND(TEMP_SENS);
    SEND(DISPLAY_UPDATE);

    return;  // first display call is just init
  }

  if (this->is_busy_ || (this->busy_pin_ != nullptr && this->busy_pin_->digital_read()))
    return;

  this->is_busy_ = true;
  this->full_update_();
}

void WeActEPaper3C::loop() {
  if (this->is_busy_) {
    if (this->busy_pin_ == nullptr || !this->busy_pin_->digital_read()) {
      this->is_busy_ = false;
    }
  }
}

}  // namespace waveshare_epaper
}  // namespace esphome
