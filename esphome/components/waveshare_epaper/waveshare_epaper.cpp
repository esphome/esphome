#include "waveshare_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include <cinttypes>

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_epaper";

static const uint8_t LUT_SIZE_WAVESHARE = 30;

static const uint8_t FULL_UPDATE_LUT[LUT_SIZE_WAVESHARE] = {0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 0x66, 0x69,
                                                            0x69, 0x59, 0x58, 0x99, 0x99, 0x88, 0x00, 0x00, 0x00, 0x00,
                                                            0xF8, 0xB4, 0x13, 0x51, 0x35, 0x51, 0x51, 0x19, 0x01, 0x00};

static const uint8_t PARTIAL_UPDATE_LUT[LUT_SIZE_WAVESHARE] = {
    0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x44, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t LUT_SIZE_TTGO = 70;

static const uint8_t FULL_UPDATE_LUT_TTGO[LUT_SIZE_TTGO] = {
    0x80, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00,  // LUT0: BB:     VS 0 ~7
    0x10, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00,  // LUT1: BW:     VS 0 ~7
    0x80, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00,  // LUT2: WB:     VS 0 ~7
    0x10, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00,  // LUT3: WW:     VS 0 ~7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT4: VCOM:   VS 0 ~7
    0x03, 0x03, 0x00, 0x00, 0x02,              // TP0 A~D RP0
    0x09, 0x09, 0x00, 0x00, 0x02,              // TP1 A~D RP1
    0x03, 0x03, 0x00, 0x00, 0x02,              // TP2 A~D RP2
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP3 A~D RP3
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP4 A~D RP4
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP5 A~D RP5
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP6 A~D RP6
};

static const uint8_t PARTIAL_UPDATE_LUT_TTGO[LUT_SIZE_TTGO] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT0: BB:     VS 0 ~7
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT1: BW:     VS 0 ~7
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT2: WB:     VS 0 ~7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT3: WW:     VS 0 ~7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT4: VCOM:   VS 0 ~7
    0x0A, 0x00, 0x00, 0x00, 0x00,              // TP0 A~D RP0
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP1 A~D RP1
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP2 A~D RP2
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP3 A~D RP3
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP4 A~D RP4
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP5 A~D RP5
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP6 A~D RP6
};

static const uint8_t LUT_SIZE_TTGO_B73 = 100;

static const uint8_t FULL_UPDATE_LUT_TTGO_B73[LUT_SIZE_TTGO_B73] = {
    0xA0, 0x90, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x90, 0xA0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xA0, 0x90, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x90, 0xA0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x0F, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x03, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t PARTIAL_UPDATE_LUT_TTGO_B73[LUT_SIZE_TTGO_B73] = {
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_SIZE_TTGO_B1 = 29;

static const uint8_t FULL_UPDATE_LUT_TTGO_B1[LUT_SIZE_TTGO_B1] = {
    0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x01, 0x00, 0x00, 0x00, 0x00};

static const uint8_t PARTIAL_UPDATE_LUT_TTGO_B1[LUT_SIZE_TTGO_B1] = {
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// clang-format off
// Disable formatting to preserve the same look as in Waveshare examples
static const uint8_t PARTIAL_UPD_2IN9_LUT_SIZE = 159;
static const uint8_t PARTIAL_UPD_2IN9_LUT[PARTIAL_UPD_2IN9_LUT_SIZE] =
{
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    0x22, 0x17, 0x41, 0xB0, 0x32, 0x36,
};
// clang-format on

void WaveshareEPaperBase::setup_pins_() {
  this->init_internal_(this->get_buffer_length_());
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }
  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();  // INPUT
  }
  this->spi_setup();

  this->reset_();
}
float WaveshareEPaperBase::get_setup_priority() const { return setup_priority::PROCESSOR; }
void WaveshareEPaperBase::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}
void WaveshareEPaperBase::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

// write a command followed by one or more bytes of data.
// The command is the first byte, length is the total including cmd.
void WaveshareEPaperBase::cmd_data(const uint8_t *c_data, size_t length) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(c_data[0]);
  this->dc_pin_->digital_write(true);
  this->write_array(c_data + 1, length - 1);
  this->disable();
}

bool WaveshareEPaperBase::wait_until_idle_() {
  if (this->busy_pin_ == nullptr || !this->busy_pin_->digital_read()) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    delay(1);
  }
  return true;
}
void WaveshareEPaperBase::update() {
  this->do_update_();
  this->display();
}
void WaveshareEPaper::fill(Color color) {
  // flip logic
  const uint8_t fill = color.is_on() ? 0x00 : 0xFF;
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void HOT WaveshareEPaper::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  const uint32_t pos = (x + y * this->get_width_controller()) / 8u;
  const uint8_t subpos = x & 0x07;
  // flip logic
  if (!color.is_on()) {
    this->buffer_[pos] |= 0x80 >> subpos;
  } else {
    this->buffer_[pos] &= ~(0x80 >> subpos);
  }
}

uint32_t WaveshareEPaper::get_buffer_length_() {
  return this->get_width_controller() * this->get_height_internal() / 8u;
}  // just a black buffer
uint32_t WaveshareEPaperBWR::get_buffer_length_() {
  return this->get_width_controller() * this->get_height_internal() / 4u;
}  // black and red buffer

void WaveshareEPaperBWR::fill(Color color) {
  this->filled_rectangle(0, 0, this->get_width(), this->get_height(), color);
}
void HOT WaveshareEPaperBWR::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;

  const uint32_t pos = (x + y * this->get_width_internal()) / 8u;
  const uint8_t subpos = x & 0x07;
  // flip logic
  if (color.is_on()) {
    this->buffer_[pos] |= 0x80 >> subpos;
  } else {
    this->buffer_[pos] &= ~(0x80 >> subpos);
  }

  // draw red pixels only, if the color contains red only
  if (((color.red > 0) && (color.green == 0) && (color.blue == 0))) {
    this->buffer_[pos + buf_half_len] |= 0x80 >> subpos;
  } else {
    this->buffer_[pos + buf_half_len] &= ~(0x80 >> subpos);
  }
}

void WaveshareEPaperBase::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}
void WaveshareEPaperBase::end_command_() { this->disable(); }
void WaveshareEPaperBase::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void WaveshareEPaperBase::end_data_() { this->disable(); }
void WaveshareEPaperBase::on_safe_shutdown() { this->deep_sleep(); }

// ========================================================
//                          Type A
// ========================================================

void WaveshareEPaperTypeA::initialize() {
  // Achieve display intialization
  this->init_display_();
  // If a reset pin is configured, eligible displays can be set to deep sleep
  // between updates, as recommended by the hardware provider
  if (this->reset_pin_ != nullptr) {
    switch (this->model_) {
      // More models can be added here to enable deep sleep if eligible
      case WAVESHARE_EPAPER_1_54_IN:
      case WAVESHARE_EPAPER_1_54_IN_V2:
        this->deep_sleep_between_updates_ = true;
        ESP_LOGI(TAG, "Set the display to deep sleep");
        this->deep_sleep();
        break;
      default:
        break;
    }
  }
}
void WaveshareEPaperTypeA::init_display_() {
  if (this->model_ == TTGO_EPAPER_2_13_IN_B74 || this->model_ == WAVESHARE_EPAPER_2_13_IN_V2) {
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->digital_write(false);
      delay(10);
      this->reset_pin_->digital_write(true);
      delay(10);
      this->wait_until_idle_();
    }

    this->command(0x12);  // SWRESET
    this->wait_until_idle_();
  }

  // COMMAND DRIVER OUTPUT CONTROL
  this->command(0x01);
  this->data(this->get_height_internal() - 1);
  this->data((this->get_height_internal() - 1) >> 8);
  this->data(0x00);  // ? GD = 0, SM = 0, TB = 0

  // COMMAND BOOSTER SOFT START CONTROL
  this->command(0x0C);
  this->data(0xD7);
  this->data(0xD6);
  this->data(0x9D);

  // COMMAND WRITE VCOM REGISTER
  this->command(0x2C);
  this->data(0xA8);

  // COMMAND SET DUMMY LINE PERIOD
  this->command(0x3A);
  this->data(0x1A);

  // COMMAND SET GATE TIME
  this->command(0x3B);
  this->data(0x08);  // 2µs per row

  // COMMAND DATA ENTRY MODE SETTING
  this->command(0x11);
  switch (this->model_) {
    case TTGO_EPAPER_2_13_IN_B1:
      this->data(0x01);  // x increase, y decrease : as in demo code
      break;
    case TTGO_EPAPER_2_13_IN_B74:
    case WAVESHARE_EPAPER_2_9_IN_V2:
      this->data(0x03);  // from top left to bottom right
      // RAM content option for Display Update
      this->command(0x21);
      this->data(0x00);
      this->data(0x80);
      break;
    default:
      this->data(0x03);  // from top left to bottom right
  }
}
void WaveshareEPaperTypeA::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
      ESP_LOGCONFIG(TAG, "  Model: 1.54in");
      break;
    case WAVESHARE_EPAPER_1_54_IN_V2:
      ESP_LOGCONFIG(TAG, "  Model: 1.54inV2");
      break;
    case WAVESHARE_EPAPER_2_13_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in");
      break;
    case WAVESHARE_EPAPER_2_13_IN_V2:
      ESP_LOGCONFIG(TAG, "  Model: 2.13inV2");
      break;
    case TTGO_EPAPER_2_13_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in (TTGO)");
      break;
    case TTGO_EPAPER_2_13_IN_B73:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in (TTGO B73)");
      break;
    case TTGO_EPAPER_2_13_IN_B74:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in (TTGO B74)");
      break;
    case TTGO_EPAPER_2_13_IN_B1:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in (TTGO B1)");
      break;
    case WAVESHARE_EPAPER_2_9_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.9in");
      break;
    case WAVESHARE_EPAPER_2_9_IN_V2:
      ESP_LOGCONFIG(TAG, "  Model: 2.9inV2");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void HOT WaveshareEPaperTypeA::display() {
  bool full_update = this->at_update_ == 0;
  bool prev_full_update = this->at_update_ == 1;

  if (this->deep_sleep_between_updates_) {
    ESP_LOGI(TAG, "Wake up the display");
    this->reset_();
    this->wait_until_idle_();
    this->init_display_();
  }

  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    return;
  }

  if (this->full_update_every_ >= 1) {
    if (full_update != prev_full_update) {
      switch (this->model_) {
        case TTGO_EPAPER_2_13_IN:
        case WAVESHARE_EPAPER_2_13_IN_V2:
          // Waveshare 2.13" V2 uses the same LUTs as TTGO
          this->write_lut_(full_update ? FULL_UPDATE_LUT_TTGO : PARTIAL_UPDATE_LUT_TTGO, LUT_SIZE_TTGO);
          break;
        case TTGO_EPAPER_2_13_IN_B73:
          this->write_lut_(full_update ? FULL_UPDATE_LUT_TTGO_B73 : PARTIAL_UPDATE_LUT_TTGO_B73, LUT_SIZE_TTGO_B73);
          break;
        case TTGO_EPAPER_2_13_IN_B74:
          // there is no LUT
          break;
        case TTGO_EPAPER_2_13_IN_B1:
          this->write_lut_(full_update ? FULL_UPDATE_LUT_TTGO_B1 : PARTIAL_UPDATE_LUT_TTGO_B1, LUT_SIZE_TTGO_B1);
          break;
        default:
          this->write_lut_(full_update ? FULL_UPDATE_LUT : PARTIAL_UPDATE_LUT, LUT_SIZE_WAVESHARE);
      }
    }
    this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
  }

  if (this->model_ == WAVESHARE_EPAPER_2_13_IN_V2) {
    // Set VCOM for full or partial update
    this->command(0x2C);
    this->data(full_update ? 0x55 : 0x26);

    if (!full_update) {
      // Enable "ping-pong"
      this->command(0x37);
      this->data(0x00);
      this->data(0x00);
      this->data(0x00);
      this->data(0x00);
      this->data(0x40);
      this->data(0x00);
      this->data(0x00);
      this->command(0x22);
      this->data(0xc0);
      this->command(0x20);
    }
  }

  // Border waveform
  switch (this->model_) {
    case TTGO_EPAPER_2_13_IN_B74:
      this->command(0x3C);
      this->data(full_update ? 0x05 : 0x80);
      break;
    case WAVESHARE_EPAPER_2_13_IN_V2:
      this->command(0x3C);
      this->data(full_update ? 0x03 : 0x01);
      break;
    default:
      break;
  }

  // Set x & y regions we want to write to (full)
  switch (this->model_) {
    case TTGO_EPAPER_2_13_IN_B1:
      // COMMAND SET RAM X ADDRESS START END POSITION
      this->command(0x44);
      this->data(0x00);
      this->data((this->get_width_controller() - 1) >> 3);
      // COMMAND SET RAM Y ADDRESS START END POSITION
      this->command(0x45);
      this->data(this->get_height_internal() - 1);
      this->data((this->get_height_internal() - 1) >> 8);
      this->data(0x00);
      this->data(0x00);

      // COMMAND SET RAM X ADDRESS COUNTER
      this->command(0x4E);
      this->data(0x00);
      // COMMAND SET RAM Y ADDRESS COUNTER
      this->command(0x4F);
      this->data(this->get_height_internal() - 1);
      this->data((this->get_height_internal() - 1) >> 8);

      break;
    default:
      // COMMAND SET RAM X ADDRESS START END POSITION
      this->command(0x44);
      this->data(0x00);
      this->data((this->get_width_internal() - 1) >> 3);
      // COMMAND SET RAM Y ADDRESS START END POSITION
      this->command(0x45);
      this->data(0x00);
      this->data(0x00);
      this->data(this->get_height_internal() - 1);
      this->data((this->get_height_internal() - 1) >> 8);

      // COMMAND SET RAM X ADDRESS COUNTER
      this->command(0x4E);
      this->data(0x00);
      // COMMAND SET RAM Y ADDRESS COUNTER
      this->command(0x4F);
      this->data(0x00);
      this->data(0x00);
  }

  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    return;
  }

  // COMMAND WRITE RAM
  this->command(0x24);
  this->start_data_();
  switch (this->model_) {
    case TTGO_EPAPER_2_13_IN_B1: {  // block needed because of variable initializations
      int16_t wb = ((this->get_width_controller()) >> 3);
      for (int i = 0; i < this->get_height_internal(); i++) {
        for (int j = 0; j < wb; j++) {
          int idx = j + (this->get_height_internal() - 1 - i) * wb;
          this->write_byte(this->buffer_[idx]);
        }
      }
      break;
    }
    default:
      this->write_array(this->buffer_, this->get_buffer_length_());
  }
  this->end_data_();

  if (this->model_ == WAVESHARE_EPAPER_2_13_IN_V2 && full_update) {
    // Write base image again on full refresh
    this->command(0x26);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();
  }

  // COMMAND DISPLAY UPDATE CONTROL 2
  this->command(0x22);
  switch (this->model_) {
    case WAVESHARE_EPAPER_2_9_IN_V2:
    case WAVESHARE_EPAPER_1_54_IN_V2:
    case TTGO_EPAPER_2_13_IN_B74:
      this->data(full_update ? 0xF7 : 0xFF);
      break;
    case TTGO_EPAPER_2_13_IN_B73:
      this->data(0xC7);
      break;
    case WAVESHARE_EPAPER_2_13_IN_V2:
      this->data(full_update ? 0xC7 : 0x0C);
      break;
    default:
      this->data(0xC4);
      break;
  }

  // COMMAND MASTER ACTIVATION
  this->command(0x20);
  // COMMAND TERMINATE FRAME READ WRITE
  this->command(0xFF);

  this->status_clear_warning();

  if (this->deep_sleep_between_updates_) {
    ESP_LOGI(TAG, "Set the display back to deep sleep");
    this->deep_sleep();
  }
}
int WaveshareEPaperTypeA::get_width_internal() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
    case WAVESHARE_EPAPER_1_54_IN_V2:
      return 200;
    case WAVESHARE_EPAPER_2_13_IN:
    case WAVESHARE_EPAPER_2_13_IN_V2:
    case TTGO_EPAPER_2_13_IN:
    case TTGO_EPAPER_2_13_IN_B73:
    case TTGO_EPAPER_2_13_IN_B74:
    case TTGO_EPAPER_2_13_IN_B1:
      return 122;
    case WAVESHARE_EPAPER_2_9_IN:
    case WAVESHARE_EPAPER_2_9_IN_V2:
      return 128;
  }
  return 0;
}
// The controller of the 2.13" displays has a buffer larger than screen size
int WaveshareEPaperTypeA::get_width_controller() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_2_13_IN:
    case WAVESHARE_EPAPER_2_13_IN_V2:
    case TTGO_EPAPER_2_13_IN:
    case TTGO_EPAPER_2_13_IN_B73:
    case TTGO_EPAPER_2_13_IN_B74:
    case TTGO_EPAPER_2_13_IN_B1:
      return 128;
    default:
      return this->get_width_internal();
  }
}
int WaveshareEPaperTypeA::get_height_internal() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
    case WAVESHARE_EPAPER_1_54_IN_V2:
      return 200;
    case WAVESHARE_EPAPER_2_13_IN:
    case WAVESHARE_EPAPER_2_13_IN_V2:
    case TTGO_EPAPER_2_13_IN:
    case TTGO_EPAPER_2_13_IN_B73:
    case TTGO_EPAPER_2_13_IN_B74:
    case TTGO_EPAPER_2_13_IN_B1:
      return 250;
    case WAVESHARE_EPAPER_2_9_IN:
    case WAVESHARE_EPAPER_2_9_IN_V2:
      return 296;
  }
  return 0;
}
void WaveshareEPaperTypeA::write_lut_(const uint8_t *lut, const uint8_t size) {
  // COMMAND WRITE LUT REGISTER
  this->command(0x32);
  for (uint8_t i = 0; i < size; i++)
    this->data(lut[i]);
}
WaveshareEPaperTypeA::WaveshareEPaperTypeA(WaveshareEPaperTypeAModel model) : model_(model) {}
void WaveshareEPaperTypeA::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

uint32_t WaveshareEPaperTypeA::idle_timeout_() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
    case WAVESHARE_EPAPER_1_54_IN_V2:
    case WAVESHARE_EPAPER_2_13_IN_V2:
    case TTGO_EPAPER_2_13_IN_B1:
      return 2500;
    default:
      return WaveshareEPaperBase::idle_timeout_();
  }
}

// ========================================================
//                          Type B
// ========================================================
// Datasheet:
//  - https://www.waveshare.com/w/upload/7/7f/4.2inch-e-paper-b-specification.pdf
//  - https://github.com/soonuse/epd-library-arduino/blob/master/4.2inch_e-paper/epd4in2/

static const uint8_t LUT_VCOM_DC_2_7[44] = {
    0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x00, 0x32, 0x32, 0x00, 0x00, 0x02, 0x00,
    0x0F, 0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_WHITE_TO_WHITE_2_7[42] = {
    0x50, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32, 0x32, 0x00, 0x00, 0x02, 0xA0, 0x0F,
    0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_BLACK_TO_WHITE_2_7[42] = {
    0x50, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32, 0x32, 0x00, 0x00, 0x02, 0xA0, 0x0F,
    0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_WHITE_TO_BLACK_2_7[] = {
    0xA0, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32, 0x32, 0x00, 0x00, 0x02, 0x50, 0x0F,
    0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_BLACK_TO_BLACK_2_7[42] = {
    0xA0, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32, 0x32, 0x00, 0x00, 0x02, 0x50, 0x0F,
    0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void WaveshareEPaper2P7In::initialize() {
  // command power setting
  this->command(0x01);
  this->data(0x03);  // VDS_EN, VDG_EN
  this->data(0x00);  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
  this->data(0x2B);  // VDH
  this->data(0x2B);  // VDL
  this->data(0x09);  // VDHR

  // command booster soft start
  this->command(0x06);
  this->data(0x07);
  this->data(0x07);
  this->data(0x17);

  // Power optimization - ???
  this->command(0xF8);
  this->data(0x60);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0x89);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0x90);
  this->data(0x00);
  this->command(0xF8);
  this->data(0x93);
  this->data(0x2A);
  this->command(0xF8);
  this->data(0xA0);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0xA1);
  this->data(0x00);
  this->command(0xF8);
  this->data(0x73);
  this->data(0x41);

  // command partial display refresh
  this->command(0x16);
  this->data(0x00);

  // command power on
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);

  // Command panel setting
  this->command(0x00);
  this->data(0xAF);  // KW-BF   KWR-AF    BWROTP 0f
  // command pll control
  this->command(0x30);
  this->data(0x3A);  // 3A 100HZ   29 150Hz 39 200HZ    31 171HZ
  // COMMAND VCM DC SETTING
  this->command(0x82);
  this->data(0x12);

  delay(2);
  // COMMAND LUT FOR VCOM
  this->command(0x20);
  for (uint8_t i : LUT_VCOM_DC_2_7)
    this->data(i);

  // COMMAND LUT WHITE TO WHITE
  this->command(0x21);
  for (uint8_t i : LUT_WHITE_TO_WHITE_2_7)
    this->data(i);
  // COMMAND LUT BLACK TO WHITE
  this->command(0x22);
  for (uint8_t i : LUT_BLACK_TO_WHITE_2_7)
    this->data(i);
  // COMMAND LUT WHITE TO BLACK
  this->command(0x23);
  for (uint8_t i : LUT_WHITE_TO_BLACK_2_7)
    this->data(i);
  // COMMAND LUT BLACK TO BLACK
  this->command(0x24);
  for (uint8_t i : LUT_BLACK_TO_BLACK_2_7)
    this->data(i);
}
void HOT WaveshareEPaper2P7In::display() {
  uint32_t buf_len = this->get_buffer_length_();

  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i]);
  }
  delay(2);

  // COMMAND DATA START TRANSMISSION 2
  this->command(0x13);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i]);
  }

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper2P7In::get_width_internal() { return 176; }
int WaveshareEPaper2P7In::get_height_internal() { return 264; }
void WaveshareEPaper2P7In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.7in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper2P7InV2::initialize() {
  this->reset_();
  this->wait_until_idle_();

  this->command(0x12);  // SWRESET
  this->wait_until_idle_();

  // SET WINDOWS
  // XRAM_START_AND_END_POSITION
  this->command(0x44);
  this->data(0x00);
  this->data(((this->get_width_controller() - 1) >> 3) & 0xFF);
  // YRAM_START_AND_END_POSITION
  this->command(0x45);
  this->data(0x00);
  this->data(0x00);
  this->data((get_height_internal() - 1) & 0xFF);
  this->data(((get_height_internal() - 1) >> 8) & 0xFF);

  // SET CURSOR
  // XRAM_ADDRESS
  this->command(0x4E);
  this->data(0x00);
  // YRAM_ADDRESS
  this->command(0x4F);
  this->data(0x00);
  this->data(0x00);

  this->command(0x11);  // data entry mode
  this->data(0x03);
}
void HOT WaveshareEPaper2P7InV2::display() {
  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // COMMAND DISPLAY REFRESH
  this->command(0x22);
  this->data(0xF7);
  this->command(0x20);
}
int WaveshareEPaper2P7InV2::get_width_internal() { return 176; }
int WaveshareEPaper2P7InV2::get_height_internal() { return 264; }
void WaveshareEPaper2P7InV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.7in V2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//                          2.7inch_e-paper_b
// ========================================================
// Datasheet:
//  - https://www.waveshare.com/w/upload/d/d8/2.7inch-e-paper-b-specification.pdf
//  - https://github.com/waveshare/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_2in7b.c

static const uint8_t LUT_VCOM_DC_2_7B[44] = {0x00, 0x00, 0x00, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x00, 0x0A, 0x0A,
                                             0x00, 0x00, 0x08, 0x00, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x00, 0x0A,
                                             0x0A, 0x00, 0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00,
                                             0x03, 0x0E, 0x00, 0x00, 0x0A, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01};

static const uint8_t LUT_WHITE_TO_WHITE_2_7B[42] = {0x90, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x40, 0x0A, 0x0A, 0x00, 0x00,
                                                    0x08, 0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x80, 0x0A, 0x0A, 0x00,
                                                    0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0E,
                                                    0x00, 0x00, 0x0A, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01};

static const uint8_t LUT_BLACK_TO_WHITE_2_7B[42] = {0xA0, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x00, 0x0A, 0x0A, 0x00, 0x00,
                                                    0x08, 0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x90, 0x0A, 0x0A, 0x00,
                                                    0x00, 0x08, 0xB0, 0x04, 0x10, 0x00, 0x00, 0x05, 0xB0, 0x03, 0x0E,
                                                    0x00, 0x00, 0x0A, 0xC0, 0x23, 0x00, 0x00, 0x00, 0x01};

static const uint8_t LUT_WHITE_TO_BLACK_2_7B[] = {0x90, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x20, 0x0A, 0x0A, 0x00, 0x00,
                                                  0x08, 0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x10, 0x0A, 0x0A, 0x00,
                                                  0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0E,
                                                  0x00, 0x00, 0x0A, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01};

static const uint8_t LUT_BLACK_TO_BLACK_2_7B[42] = {0x90, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x40, 0x0A, 0x0A, 0x00, 0x00,
                                                    0x08, 0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x80, 0x0A, 0x0A, 0x00,
                                                    0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0E,
                                                    0x00, 0x00, 0x0A, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01};

void WaveshareEPaper2P7InB::initialize() {
  this->reset_();

  // command power on
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);

  // Command panel setting
  this->command(0x00);
  this->data(0xAF);  // KW-BF   KWR-AF    BWROTP 0f
  // command pll control
  this->command(0x30);
  this->data(0x3A);  // 3A 100HZ   29 150Hz 39 200HZ    31 171HZ

  // command power setting
  this->command(0x01);
  this->data(0x03);  // VDS_EN, VDG_EN
  this->data(0x00);  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
  this->data(0x2B);  // VDH
  this->data(0x2B);  // VDL
  this->data(0x09);  // VDHR

  // command booster soft start
  this->command(0x06);
  this->data(0x07);
  this->data(0x07);
  this->data(0x17);

  // Power optimization - ???
  this->command(0xF8);
  this->data(0x60);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0x89);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0x90);
  this->data(0x00);
  this->command(0xF8);
  this->data(0x93);
  this->data(0x2A);
  this->command(0xF8);
  this->data(0x73);
  this->data(0x41);

  // COMMAND VCM DC SETTING
  this->command(0x82);
  this->data(0x12);

  // VCOM_AND_DATA_INTERVAL_SETTING
  this->command(0x50);
  this->data(0x87);  // define by OTP

  delay(2);
  // COMMAND LUT FOR VCOM
  this->command(0x20);
  for (uint8_t i : LUT_VCOM_DC_2_7B)
    this->data(i);
  // COMMAND LUT WHITE TO WHITE
  this->command(0x21);
  for (uint8_t i : LUT_WHITE_TO_WHITE_2_7B)
    this->data(i);
  // COMMAND LUT BLACK TO WHITE
  this->command(0x22);
  for (uint8_t i : LUT_BLACK_TO_WHITE_2_7B)
    this->data(i);
  // COMMAND LUT WHITE TO BLACK
  this->command(0x23);
  for (uint8_t i : LUT_WHITE_TO_BLACK_2_7B) {
    this->data(i);
  }
  // COMMAND LUT BLACK TO BLACK
  this->command(0x24);

  for (uint8_t i : LUT_BLACK_TO_BLACK_2_7B) {
    this->data(i);
  }

  delay(2);
}

void HOT WaveshareEPaper2P7InB::display() {
  uint32_t buf_len_half = this->get_buffer_length_() >> 1;
  this->initialize();

  // TCON_RESOLUTION
  this->command(0x61);
  this->data(this->get_width_controller() >> 8);
  this->data(this->get_width_controller() & 0xff);  // 176
  this->data(this->get_height_internal() >> 8);
  this->data(this->get_height_internal() & 0xff);  // 264

  // COMMAND DATA START TRANSMISSION 1 (BLACK)
  this->command(0x10);
  delay(2);
  for (uint32_t i = 0; i < buf_len_half; i++) {
    this->data(this->buffer_[i]);
  }
  this->command(0x11);
  delay(2);

  // COMMAND DATA START TRANSMISSION 2  (RED)
  this->command(0x13);
  delay(2);
  for (uint32_t i = buf_len_half; i < buf_len_half * 2u; i++) {
    this->data(this->buffer_[i]);
  }
  this->command(0x11);

  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  this->wait_until_idle_();

  this->deep_sleep();
}
int WaveshareEPaper2P7InB::get_width_internal() { return 176; }
int WaveshareEPaper2P7InB::get_height_internal() { return 264; }
void WaveshareEPaper2P7InB::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.7in B");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//                          2.7inch_e-paper_b_v2
// ========================================================
// Datasheet:
//  - https://www.waveshare.com/w/upload/7/7b/2.7inch-e-paper-b-v2-specification.pdf
//  - https://github.com/waveshare/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_2in7b_V2.c

void WaveshareEPaper2P7InBV2::initialize() {
  this->reset_();

  this->wait_until_idle_();
  this->command(0x12);
  this->wait_until_idle_();

  this->command(0x00);
  this->data(0x27);
  this->data(0x01);
  this->data(0x00);

  this->command(0x11);
  this->data(0x03);

  // self.SetWindows(0, 0, self.width-1, self.height-1)
  // SetWindows(self, Xstart, Ystart, Xend, Yend):

  uint32_t xend = this->get_width_controller() - 1;
  uint32_t yend = this->get_height_internal() - 1;
  this->command(0x44);
  this->data(0x00);
  this->data((xend >> 3) & 0xff);

  this->command(0x45);
  this->data(0x00);
  this->data(0x00);
  this->data(yend & 0xff);
  this->data((yend >> 8) & 0xff);

  // SetCursor(self, Xstart, Ystart):
  this->command(0x4E);
  this->data(0x00);
  this->command(0x4F);
  this->data(0x00);
  this->data(0x00);
}

void HOT WaveshareEPaper2P7InBV2::display() {
  uint32_t buf_len = this->get_buffer_length_();
  // COMMAND DATA START TRANSMISSION 1 (BLACK)
  this->command(0x24);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i]);
  }
  delay(2);

  // COMMAND DATA START TRANSMISSION 2  (RED)
  this->command(0x26);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i]);
  }

  delay(2);

  this->command(0x20);

  this->wait_until_idle_();
}
int WaveshareEPaper2P7InBV2::get_width_internal() { return 176; }
int WaveshareEPaper2P7InBV2::get_height_internal() { return 264; }
void WaveshareEPaper2P7InBV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.7in B V2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//               2.90in Type B (LUT from OTP)
// Datasheet:
//  - https://www.waveshare.com/w/upload/b/bb/2.9inch-e-paper-b-specification.pdf
//  - https://github.com/soonuse/epd-library-arduino/blob/master/2.9inch_e-paper_b/epd2in9b/epd2in9b.cpp
// ========================================================

void WaveshareEPaper2P9InB::initialize() {
  // from https://www.waveshare.com/w/upload/b/bb/2.9inch-e-paper-b-specification.pdf, page 37
  // EPD hardware init start
  this->reset_();

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0x17);
  this->data(0x17);
  this->data(0x17);

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  // 128x296 resolution:        10
  // LUT from OTP:              0
  // B/W mode (doesn't work):   1
  // scan-up:                   1
  // shift-right:               1
  // booster ON:                1
  // no soft reset:             1
  this->data(0x9F);

  // COMMAND RESOLUTION SETTING
  // set to 128x296 by COMMAND PANEL SETTING

  // COMMAND VCOM AND DATA INTERVAL SETTING
  // use defaults for white border and ESPHome image polarity

  // EPD hardware init end
}
void HOT WaveshareEPaper2P9InB::display() {
  // COMMAND DATA START TRANSMISSION 1 (B/W data)
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);

  // COMMAND DATA START TRANSMISSION 2 (RED data)
  this->command(0x13);
  delay(2);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0x00);
  this->end_data_();
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(2);
  this->wait_until_idle_();

  // COMMAND POWER OFF
  // NOTE: power off < deep sleep
  this->command(0x02);
}
int WaveshareEPaper2P9InB::get_width_internal() { return 128; }
int WaveshareEPaper2P9InB::get_height_internal() { return 296; }
void WaveshareEPaper2P9InB::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in (B)");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// DKE 2.9
// https://www.badge.team/docs/badges/sha2017/hardware/#e-ink-display-the-dke-group-depg0290b1
// https://www.badge.team/docs/badges/sha2017/hardware/DEPG0290B01V3.0.pdf
static const uint8_t LUT_SIZE_DKE = 70;
static const uint8_t UPDATE_LUT_DKE[LUT_SIZE_DKE] = {
    0xA0, 0x90, 0x50, 0x0,  0x0,  0x0,  0x0, 0x50, 0x90, 0xA0, 0x0,  0x0,  0x0,  0x0, 0xA0, 0x90, 0x50, 0x0,
    0x0,  0x0,  0x0,  0x50, 0x90, 0xA0, 0x0, 0x0,  0x0,  0x0,  0x00, 0x00, 0x00, 0x0, 0x0,  0x0,  0x0,  0xF,
    0xF,  0x0,  0x0,  0x0,  0xF,  0xF,  0x0, 0x0,  0x02, 0xF,  0xF,  0x0,  0x0,  0x0, 0x0,  0x0,  0x0,  0x0,
    0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0,  0x0,
};
static const uint8_t PART_UPDATE_LUT_DKE[LUT_SIZE_DKE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xa0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x50, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0x05, 0x00, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t FULL_UPDATE_LUT_DKE[LUT_SIZE_DKE] = {
    0x90, 0x50, 0xa0, 0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0x10, 0xa0, 0xa0, 0x80, 0x00, 0x90, 0x50, 0xa0, 0x50,
    0x50, 0x00, 0x00, 0x00, 0x00, 0x10, 0xa0, 0xa0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17,
    0x04, 0x00, 0x00, 0x00, 0x0b, 0x04, 0x00, 0x00, 0x00, 0x06, 0x05, 0x00, 0x00, 0x00, 0x04, 0x05, 0x00, 0x00,
    0x00, 0x01, 0x0e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void WaveshareEPaper2P9InDKE::initialize() {
  // Hardware reset
  delay(10);
  this->reset_pin_->digital_write(false);
  delayMicroseconds(200);
  this->reset_pin_->digital_write(true);
  delayMicroseconds(200);
  // Wait for busy low
  this->wait_until_idle_();
  // Software reset
  this->command(0x12);
  // Wait for busy low
  this->wait_until_idle_();
  // Set Analog Block Control
  this->command(0x74);
  this->data(0x54);
  // Set Digital Block Control
  this->command(0x7E);
  this->data(0x3B);
  // Set display size and driver output control
  this->command(0x01);
  // this->data(0x27);
  // this->data(0x01);
  // this->data(0x00);
  this->data(this->get_height_internal() - 1);
  this->data((this->get_height_internal() - 1) >> 8);
  this->data(0x00);  // ? GD = 0, SM = 0, TB = 0
  // Ram data entry mode
  this->command(0x11);
  this->data(0x03);
  // Set Ram X address
  this->command(0x44);
  this->data(0x00);
  this->data(0x0F);
  // Set Ram Y address
  this->command(0x45);
  this->data(0x00);
  this->data(0x00);
  this->data(0x27);
  this->data(0x01);
  // Set border
  this->command(0x3C);
  // this->data(0x80);
  this->data(0x01);
  // Set VCOM value
  this->command(0x2C);
  this->data(0x26);
  // Gate voltage setting
  this->command(0x03);
  this->data(0x17);
  // Source voltage setting
  this->command(0x04);
  this->data(0x41);
  this->data(0x00);
  this->data(0x32);
  // Frame setting 50hz
  this->command(0x3A);
  this->data(0x30);
  this->command(0x3B);
  this->data(0x0A);
  // Load LUT
  this->command(0x32);
  for (uint8_t v : FULL_UPDATE_LUT_DKE)
    this->data(v);
}

void HOT WaveshareEPaper2P9InDKE::display() {
  ESP_LOGI(TAG, "Performing e-paper update.");
  // Set Ram X address counter
  this->command(0x4e);
  this->data(0);
  // Set Ram Y address counter
  this->command(0x4f);
  this->data(0);
  this->data(0);
  // Load image (128/8*296)
  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  // Image update
  this->command(0x22);
  this->data(0xC7);
  this->command(0x20);
  // Wait for busy low
  this->wait_until_idle_();
  // Enter deep sleep mode
  this->command(0x10);
  this->data(0x01);
}
int WaveshareEPaper2P9InDKE::get_width_internal() { return 128; }
int WaveshareEPaper2P9InDKE::get_height_internal() { return 296; }
void WaveshareEPaper2P9InDKE::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in DKE");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void WaveshareEPaper2P9InDKE::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

// ========================================================
//               2.90in Type B (LUT from OTP)
// Datasheet:
//  - https://files.waveshare.com/upload/a/af/2.9inch-e-paper-b-v3-specification.pdf
// ========================================================

void WaveshareEPaper2P9InBV3::initialize() {
  // from https://github.com/waveshareteam/e-Paper/blob/master/Arduino/epd2in9b_V3/epd2in9b_V3.cpp
  this->reset_();

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x0F);
  this->data(0x89);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x80);
  this->data(0x01);
  this->data(0x28);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x77);
}
void HOT WaveshareEPaper2P9InBV3::display() {
  // COMMAND DATA START TRANSMISSION 1 (B/W data)
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  this->command(0x92);
  delay(2);

  // COMMAND DATA START TRANSMISSION 2 (RED data)
  this->command(0x13);
  delay(2);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0xFF);
  this->end_data_();
  this->command(0x92);
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(2);
  this->wait_until_idle_();

  // COMMAND POWER OFF
  // NOTE: power off < deep sleep
  this->command(0x02);
}
int WaveshareEPaper2P9InBV3::get_width_internal() { return 128; }
int WaveshareEPaper2P9InBV3::get_height_internal() { return 296; }
void WaveshareEPaper2P9InBV3::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in (B) V3");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//               2.90in v2 rev2
// based on SDK and examples in ZIP file from:
// https://www.waveshare.com/pico-epaper-2.9.htm
// ========================================================

void WaveshareEPaper2P9InV2R2::initialize() {
  this->reset_();
  this->wait_until_idle_();

  this->command(0x12);  // SWRESET
  this->wait_until_idle_();

  this->command(0x01);
  this->data(0x27);
  this->data(0x01);
  this->data(0x00);

  this->command(0x11);
  this->data(0x03);

  // SetWindows(0, 0, w, h)
  this->command(0x44);
  this->data(0x00);
  this->data(((this->get_width_controller() - 1) >> 3) & 0xFF);

  this->command(0x45);
  this->data(0x00);
  this->data(0x00);
  this->data((this->get_height_internal() - 1) & 0xFF);
  this->data(((this->get_height_internal() - 1) >> 8) & 0xFF);

  this->command(0x21);
  this->data(0x00);
  this->data(0x80);

  // SetCursor(0, 0)
  this->command(0x4E);
  this->data(0x00);
  this->command(0x4f);
  this->data(0x00);
  this->data(0x00);

  this->wait_until_idle_();
}

WaveshareEPaper2P9InV2R2::WaveshareEPaper2P9InV2R2() { this->reset_duration_ = 10; }

void WaveshareEPaper2P9InV2R2::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(reset_duration_);  // NOLINT
    this->reset_pin_->digital_write(true);
    delay(reset_duration_);  // NOLINT
  }
}

void WaveshareEPaper2P9InV2R2::display() {
  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    ESP_LOGE(TAG, "fail idle 1");
    return;
  }

  if (this->full_update_every_ == 1) {
    // do single full update
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // TurnOnDisplay
    this->command(0x22);
    this->data(0xF7);
    this->command(0x20);
    return;
  }

  // if (this->full_update_every_ == 1 ||
  if (this->at_update_ == 0) {
    // do base update
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    this->command(0x26);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // TurnOnDisplay
    this->command(0x22);
    this->data(0xF7);
    this->command(0x20);
  } else {
    // do partial update
    this->reset_();

    this->write_lut_(PARTIAL_UPD_2IN9_LUT, PARTIAL_UPD_2IN9_LUT_SIZE);

    this->command(0x37);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x40);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);

    this->command(0x3C);
    this->data(0x80);

    this->command(0x22);
    this->data(0xC0);
    this->command(0x20);

    if (!this->wait_until_idle_()) {
      ESP_LOGE(TAG, "fail idle 2");
    }

    // SetWindows(0, 0, w, h)
    this->command(0x44);
    this->data(0x00);
    this->data(((this->get_width_controller() - 1) >> 3) & 0xFF);

    this->command(0x45);
    this->data(0x00);
    this->data(0x00);
    this->data((this->get_height_internal() - 1) & 0xFF);
    this->data(((this->get_height_internal() - 1) >> 8) & 0xFF);

    // SetCursor(0, 0)
    this->command(0x4E);
    this->data(0x00);
    this->command(0x4f);
    this->data(0x00);
    this->data(0x00);

    // write b/w
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // TurnOnDisplayPartial
    this->command(0x22);
    this->data(0x0F);
    this->command(0x20);
  }

  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
}

void WaveshareEPaper2P9InV2R2::write_lut_(const uint8_t *lut, const uint8_t size) {
  // COMMAND WRITE LUT REGISTER
  this->command(0x32);
  for (uint8_t i = 0; i < size; i++)
    this->data(lut[i]);
}

void WaveshareEPaper2P9InV2R2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9inV2R2");
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper2P9InV2R2::deep_sleep() {
  this->command(0x10);
  this->data(0x01);
}

int WaveshareEPaper2P9InV2R2::get_width_internal() { return 128; }
int WaveshareEPaper2P9InV2R2::get_height_internal() { return 296; }
int WaveshareEPaper2P9InV2R2::get_width_controller() { return this->get_width_internal(); }
void WaveshareEPaper2P9InV2R2::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

// ========================================================
//     Good Display 2.9in black/white/grey
// Datasheet:
//  - https://v4.cecdn.yun300.cn/100001_1909185148/SSD1680.pdf
//  - https://github.com/adafruit/Adafruit_EPD/blob/master/src/panels/ThinkInk_290_Grayscale4_T5.h
// ========================================================

void GDEW029T5::initialize() {
  // from https://www.waveshare.com/w/upload/b/bb/2.9inch-e-paper-b-specification.pdf, page 37
  // EPD hardware init start
  this->reset_();

  // COMMAND POWER SETTINGS
  this->command(0x00);
  this->data(0x03);
  this->data(0x00);
  this->data(0x2b);
  this->data(0x2b);
  this->data(0x03); /* for b/w */

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0x17);
  this->data(0x17);
  this->data(0x17);

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();

  // Not sure what this does but it's in the Adafruit EPD library
  this->command(0xFF);
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  // 128x296 resolution:        10
  // LUT from OTP:              0
  // B/W mode (doesn't work):   1
  // scan-up:                   1
  // shift-right:               1
  // booster ON:                1
  // no soft reset:             1
  this->data(0b10011111);

  // COMMAND RESOLUTION SETTING
  // set to 128x296 by COMMAND PANEL SETTING

  // COMMAND VCOM AND DATA INTERVAL SETTING
  // use defaults for white border and ESPHome image polarity

  // EPD hardware init end
}
void HOT GDEW029T5::display() {
  // COMMAND DATA START TRANSMISSION 2 (B/W only)
  this->command(0x13);
  delay(2);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    this->write_byte(this->buffer_[i]);
  }
  this->end_data_();
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(2);
  this->wait_until_idle_();

  // COMMAND POWER OFF
  // NOTE: power off < deep sleep
  this->command(0x02);
}
int GDEW029T5::get_width_internal() { return 128; }
int GDEW029T5::get_height_internal() { return 296; }
void GDEW029T5::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper (Good Display)", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in Greyscale GDEW029T5");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//     Good Display 1.54in black/white/grey GDEW0154M09
// As used in M5Stack Core Ink
// Datasheet:
//  - https://v4.cecdn.yun300.cn/100001_1909185148/GDEW0154M09-200709.pdf
//  - https://github.com/m5stack/M5Core-Ink
// Reference code from GoodDisplay:
//  - https://github.com/GoodDisplay/E-paper-Display-Library-of-GoodDisplay/
//  -> /Monochrome_E-paper-Display/1.54inch_JD79653_GDEW0154M09_200x200/ESP32-Arduino%20IDE/GDEW0154M09_Arduino.ino
// M5Stack Core Ink spec:
//  - https://docs.m5stack.com/en/core/coreink
// ========================================================

void GDEW0154M09::initialize() {
  this->init_internal_();
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->lastbuff_ = allocator.allocate(this->get_buffer_length_());
  if (this->lastbuff_ != nullptr) {
    memset(this->lastbuff_, 0xff, sizeof(uint8_t) * this->get_buffer_length_());
  }
  this->clear_();
}

void GDEW0154M09::reset_() {
  // RST is inverse from other einks in this project
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void GDEW0154M09::init_internal_() {
  this->reset_();

  // clang-format off
  // 200x200 resolution:        11
  // LUT from OTP:              0
  // B/W mode (doesn't work):   1
  // scan-up:                   1
  // shift-right:               1
  // booster ON:                1
  // no soft reset:             1
  const uint8_t panel_setting_1 = 0b11011111;

  // VCOM status off            0
  // Temp sensing default       1
  // VGL Power Off Floating     1
  // NORG expect refresh        1
  // VCOM Off on displ off      0
  const uint8_t panel_setting_2 = 0b01110;

  const uint8_t wf_t0154_cz_b3_list[] = {
      11, //  11 commands in list
      CMD_PSR_PANEL_SETTING, 2, panel_setting_1, panel_setting_2,
      CMD_UNDOCUMENTED_0x4D, 1, 0x55,
      CMD_UNDOCUMENTED_0xAA, 1, 0x0f,
      CMD_UNDOCUMENTED_0xE9, 1, 0x02,
      CMD_UNDOCUMENTED_0xB6, 1, 0x11,
      CMD_UNDOCUMENTED_0xF3, 1, 0x0a,
      CMD_TRES_RESOLUTION_SETTING, 3, 0xc8, 0x00, 0xc8,
      CMD_TCON_TCONSETTING, 1, 0x00,
      CMD_CDI_VCOM_DATA_INTERVAL, 1, 0xd7,
      CMD_PWS_POWER_SAVING, 1, 0x00,
      CMD_PON_POWER_ON, 0
  };
  // clang-format on

  this->write_init_list_(wf_t0154_cz_b3_list);
  delay(100);  // NOLINT
  this->wait_until_idle_();
}

void GDEW0154M09::write_init_list_(const uint8_t *list) {
  uint8_t list_limit = list[0];
  uint8_t *start_ptr = ((uint8_t *) list + 1);
  for (uint8_t i = 0; i < list_limit; i++) {
    this->command(*(start_ptr + 0));
    for (uint8_t dnum = 0; dnum < *(start_ptr + 1); dnum++) {
      this->data(*(start_ptr + 2 + dnum));
    }
    start_ptr += (*(start_ptr + 1) + 2);
  }
}

void GDEW0154M09::clear_() {
  uint32_t pixsize = this->get_buffer_length_();
  for (uint8_t j = 0; j < 2; j++) {
    this->command(CMD_DTM1_DATA_START_TRANS);
    for (int count = 0; count < pixsize; count++) {
      this->data(0x00);
    }
    this->command(CMD_DTM2_DATA_START_TRANS2);
    for (int count = 0; count < pixsize; count++) {
      this->data(0xff);
    }
    this->command(CMD_DISPLAY_REFRESH);
    delay(10);
    this->wait_until_idle_();
  }
}

void HOT GDEW0154M09::display() {
  this->init_internal_();
  // "Mode 0 display" for now
  this->command(CMD_DTM1_DATA_START_TRANS);
  for (int i = 0; i < this->get_buffer_length_(); i++) {
    this->data(0xff);
  }
  this->command(CMD_DTM2_DATA_START_TRANS2);  // write 'new' data to SRAM
  for (int i = 0; i < this->get_buffer_length_(); i++) {
    this->data(this->buffer_[i]);
  }
  this->command(CMD_DISPLAY_REFRESH);
  delay(10);
  this->wait_until_idle_();
  this->deep_sleep();
}

void GDEW0154M09::deep_sleep() {
  // COMMAND DEEP SLEEP
  this->command(CMD_POF_POWER_OFF);
  this->wait_until_idle_();
  delay(1000);  // NOLINT
  this->command(CMD_DSLP_DEEP_SLEEP);
  this->data(DATA_DSLP_DEEP_SLEEP);
}

int GDEW0154M09::get_width_internal() { return 200; }
int GDEW0154M09::get_height_internal() { return 200; }
void GDEW0154M09::dump_config() {
  LOG_DISPLAY("", "M5Stack CoreInk E-Paper (Good Display)", this);
  ESP_LOGCONFIG(TAG, "  Model: 1.54in Greyscale GDEW0154M09");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

static const uint8_t LUT_VCOM_DC_4_2[] = {
    0x00, 0x17, 0x00, 0x00, 0x00, 0x02, 0x00, 0x17, 0x17, 0x00, 0x00, 0x02, 0x00, 0x0A, 0x01,
    0x00, 0x00, 0x01, 0x00, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t LUT_WHITE_TO_WHITE_4_2[] = {
    0x40, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00, 0x02, 0x40, 0x0A,
    0x01, 0x00, 0x00, 0x01, 0xA0, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t LUT_BLACK_TO_WHITE_4_2[] = {
    0x40, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00, 0x02, 0x40, 0x0A,
    0x01, 0x00, 0x00, 0x01, 0xA0, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_BLACK_TO_BLACK_4_2[] = {
    0x80, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00, 0x02, 0x80, 0x0A,
    0x01, 0x00, 0x00, 0x01, 0x50, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_WHITE_TO_BLACK_4_2[] = {
    0x80, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00, 0x02, 0x80, 0x0A,
    0x01, 0x00, 0x00, 0x01, 0x50, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void WaveshareEPaper4P2In::initialize() {
  // https://www.waveshare.com/w/upload/7/7f/4.2inch-e-paper-b-specification.pdf - page 8

  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x03);  // VDS_EN, VDG_EN
  this->data(0x00);  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
  this->data(0x2B);  // VDH
  this->data(0x2B);  // VDL
  this->data(0xFF);  // VDHR

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0x17);  // PHA
  this->data(0x17);  // PHB
  this->data(0x17);  // PHC

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0xBF);  // KW-BF   KWR-AF  BWROTP 0f
  this->data(0x0B);
  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3C);  // 3A 100HZ   29 150Hz 39 200HZ  31 171HZ

  delay(2);
  // COMMAND LUT FOR VCOM
  this->command(0x20);
  for (uint8_t i : LUT_VCOM_DC_4_2)
    this->data(i);
  // COMMAND LUT WHITE TO WHITE
  this->command(0x21);
  for (uint8_t i : LUT_WHITE_TO_WHITE_4_2)
    this->data(i);
  // COMMAND LUT BLACK TO WHITE
  this->command(0x22);
  for (uint8_t i : LUT_BLACK_TO_WHITE_4_2)
    this->data(i);
  // COMMAND LUT WHITE TO BLACK
  this->command(0x23);
  for (uint8_t i : LUT_WHITE_TO_BLACK_4_2)
    this->data(i);
  // COMMAND LUT BLACK TO BLACK
  this->command(0x24);
  for (uint8_t i : LUT_BLACK_TO_BLACK_4_2)
    this->data(i);
}
void HOT WaveshareEPaper4P2In::display() {
  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x01);
  this->data(0x90);
  this->data(0x01);
  this->data(0x2C);

  // COMMAND VCM DC SETTING REGISTER
  this->command(0x82);
  this->data(0x12);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x97);

  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);
  // COMMAND DATA START TRANSMISSION 2
  this->command(0x13);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper4P2In::get_width_internal() { return 400; }
int WaveshareEPaper4P2In::get_height_internal() { return 300; }
void WaveshareEPaper4P2In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 4.2in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//               4.20in Type B (LUT from OTP)
// Datasheet:
//  - https://www.waveshare.com/w/upload/2/20/4.2inch-e-paper-module-user-manual-en.pdf
//  - https://github.com/waveshare/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_4in2b_V2.c
// ========================================================
void WaveshareEPaper4P2InBV2::initialize() {
  // these exact timings are required for a proper reset/init
  this->reset_pin_->digital_write(false);
  delay(2);
  this->reset_pin_->digital_write(true);
  delay(200);  // NOLINT

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x0f);  // LUT from OTP
}

void HOT WaveshareEPaper4P2InBV2::display() {
  // COMMAND DATA START TRANSMISSION 1 (B/W data)
  this->command(0x10);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // COMMAND DATA START TRANSMISSION 2 (RED data)
  this->command(0x13);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0xFF);
  this->end_data_();
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  this->wait_until_idle_();

  // COMMAND POWER OFF
  // NOTE: power off < deep sleep
  this->command(0x02);
}
int WaveshareEPaper4P2InBV2::get_width_internal() { return 400; }
int WaveshareEPaper4P2InBV2::get_height_internal() { return 300; }
void WaveshareEPaper4P2InBV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 4.2in (B V2)");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper5P8In::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x37);
  this->data(0x00);

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0xCF);
  this->data(0x0B);

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0xC7);
  this->data(0xCC);
  this->data(0x28);

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);

  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3C);

  // COMMAND TEMPERATURE SENSOR CALIBRATION
  this->command(0x41);
  this->data(0x00);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x77);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x02);
  this->data(0x58);
  this->data(0x01);
  this->data(0xC0);

  // COMMAND VCM DC SETTING REGISTER
  this->command(0x82);
  this->data(0x1E);

  this->command(0xE5);
  this->data(0x03);
}
void HOT WaveshareEPaper5P8In::display() {
  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);

  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    uint8_t temp1 = this->buffer_[i];
    for (uint8_t j = 0; j < 8; j++) {
      uint8_t temp2;
      if (temp1 & 0x80) {
        temp2 = 0x03;
      } else {
        temp2 = 0x00;
      }

      temp2 <<= 4;
      temp1 <<= 1;
      j++;
      if (temp1 & 0x80) {
        temp2 |= 0x03;
      } else {
        temp2 |= 0x00;
      }
      temp1 <<= 1;
      this->write_byte(temp2);
    }

    App.feed_wdt();
  }
  this->end_data_();

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper5P8In::get_width_internal() { return 600; }
int WaveshareEPaper5P8In::get_height_internal() { return 448; }
void WaveshareEPaper5P8In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 5.83in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//               5.83in V2
// Datasheet/Specification/Reference:
//  - https://www.waveshare.com/w/upload/3/37/5.83inch_e-Paper_V2_Specification.pdf
//  - https://github.com/waveshare/e-Paper/blob/master/Arduino/epd5in83_V2/epd5in83_V2.cpp
// ========================================================
void WaveshareEPaper5P8InV2::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x07);
  this->data(0x07);
  this->data(0x3f);
  this->data(0x3f);

  // COMMAND POWER ON
  this->command(0x04);
  delay(10);
  this->wait_until_idle_();

  // PANNEL SETTING
  this->command(0x00);
  this->data(0x1F);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x02);
  this->data(0x88);
  this->data(0x01);
  this->data(0xE0);

  this->command(0x15);
  this->data(0x00);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // Do we need this?
  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3C);  // 3A 100HZ   29 150Hz 39 200HZ  31 171HZ
}
void HOT WaveshareEPaper5P8InV2::display() {
  // Reuse the code from WaveshareEPaper4P2In::display()
  // COMMAND VCM DC SETTING REGISTER
  this->command(0x82);
  this->data(0x12);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x97);

  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);

  // COMMAND DATA START TRANSMISSION 2
  this->command(0x13);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper5P8InV2::get_width_internal() { return 648; }
int WaveshareEPaper5P8InV2::get_height_internal() { return 480; }
void WaveshareEPaper5P8InV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 5.83inv2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5InBV2::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x07);
  this->data(0x07);  // VGH=20V,VGL=-20V
  this->data(0x3f);  // VDH=15V
  this->data(0x3f);  // VDL=-15V
  // COMMAND POWER ON
  this->command(0x04);
  delay(100);  // NOLINT
  this->wait_until_idle_();
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x0F);     // KW3f, KWR-2F, BWROTP 0f, BWOTP 1f
  this->command(0x61);  // tres
  this->data(0x03);     // 800px
  this->data(0x20);
  this->data(0x01);  // 400px
  this->data(0xE0);
  this->command(0x15);
  this->data(0x00);
  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x11);
  this->data(0x07);
  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  this->command(0x82);
  this->data(0x08);
  this->command(0x30);
  this->data(0x06);

  // COMMAND RESOLUTION SETTING
  this->command(0x65);
  this->data(0x00);
  this->data(0x00);  // 800*480
  this->data(0x00);
  this->data(0x00);
}
void HOT WaveshareEPaper7P5InBV2::display() {
  // COMMAND DATA START TRANSMISSION 1 (B/W data)
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);

  // COMMAND DATA START TRANSMISSION 2 (RED data)
  this->command(0x13);
  delay(2);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0x00);
  this->end_data_();
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(100);  // NOLINT
  this->wait_until_idle_();
  this->deep_sleep();
}
int WaveshareEPaper7P5InBV2::get_width_internal() { return 800; }
int WaveshareEPaper7P5InBV2::get_height_internal() { return 480; }
void WaveshareEPaper7P5InBV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in-bv2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5InBV3::initialize() { this->init_display_(); }
bool WaveshareEPaper7P5InBV3::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    this->command(0x71);
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGI(TAG, "Timeout while displaying image!");
      return false;
    }
    App.feed_wdt();
    delay(10);
  }
  delay(200);  // NOLINT
  return true;
};
void WaveshareEPaper7P5InBV3::init_display_() {
  this->reset_();

  // COMMAND POWER SETTING
  this->command(0x01);

  // 1-0=11: internal power
  this->data(0x07);
  this->data(0x17);  // VGH&VGL
  this->data(0x3F);  // VSH
  this->data(0x26);  // VSL
  this->data(0x11);  // VSHR

  // VCOM DC Setting
  this->command(0x82);
  this->data(0x24);  // VCOM

  // Booster Setting
  this->command(0x06);
  this->data(0x27);
  this->data(0x27);
  this->data(0x2F);
  this->data(0x17);

  // POWER ON
  this->command(0x04);

  delay(100);  // NOLINT
  this->wait_until_idle_();
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x3F);  // KW-3f   KWR-2F BWROTP 0f BWOTP 1f

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x03);  // source 800
  this->data(0x20);
  this->data(0x01);  // gate 480
  this->data(0xE0);
  // COMMAND ...?
  this->command(0x15);
  this->data(0x00);
  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x10);
  this->data(0x00);
  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);
  // Resolution setting
  this->command(0x65);
  this->data(0x00);
  this->data(0x00);  // 800*480
  this->data(0x00);
  this->data(0x00);

  uint8_t lut_vcom_7_i_n5_v2[] = {
      0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0xF, 0x1, 0xF, 0x1, 0x2, 0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_ww_7_i_n5_v2[] = {
      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_bw_7_i_n5_v2[] = {
      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_wb_7_i_n5_v2[] = {
      0x80, 0xF, 0xF, 0x0, 0x0, 0x3, 0x84, 0xF, 0x1, 0xF, 0x1, 0x4, 0x40, 0xF, 0xF, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_bb_7_i_n5_v2[] = {
      0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t count;
  this->command(0x20);  // VCOM
  for (count = 0; count < 42; count++)
    this->data(lut_vcom_7_i_n5_v2[count]);

  this->command(0x21);  // LUTBW
  for (count = 0; count < 42; count++)
    this->data(lut_ww_7_i_n5_v2[count]);

  this->command(0x22);  // LUTBW
  for (count = 0; count < 42; count++)
    this->data(lut_bw_7_i_n5_v2[count]);

  this->command(0x23);  // LUTWB
  for (count = 0; count < 42; count++)
    this->data(lut_wb_7_i_n5_v2[count]);

  this->command(0x24);  // LUTBB
  for (count = 0; count < 42; count++)
    this->data(lut_bb_7_i_n5_v2[count]);
};
void HOT WaveshareEPaper7P5InBV3::display() {
  this->init_display_();
  uint32_t buf_len = this->get_buffer_length_();

  this->command(0x10);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(0xFF);
  }

  this->command(0x13);  // Start Transmission
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(~this->buffer_[i]);
  }

  this->command(0x12);  // Display Refresh
  delay(100);           // NOLINT
  this->wait_until_idle_();
  this->deep_sleep();
}
int WaveshareEPaper7P5InBV3::get_width_internal() { return 800; }
int WaveshareEPaper7P5InBV3::get_height_internal() { return 480; }
void WaveshareEPaper7P5InBV3::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in-bv3");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5In::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x37);
  this->data(0x00);
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0xCF);
  this->data(0x0B);
  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0xC7);
  this->data(0xCC);
  this->data(0x28);
  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);
  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3C);
  // COMMAND TEMPERATURE SENSOR CALIBRATION
  this->command(0x41);
  this->data(0x00);
  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x77);
  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);
  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x02);
  this->data(0x80);
  this->data(0x01);
  this->data(0x80);
  // COMMAND VCM DC SETTING REGISTER
  this->command(0x82);
  this->data(0x1E);
  this->command(0xE5);
  this->data(0x03);
}
void HOT WaveshareEPaper7P5In::display() {
  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    uint8_t temp1 = this->buffer_[i];
    for (uint8_t j = 0; j < 8; j++) {
      uint8_t temp2;
      if (temp1 & 0x80) {
        temp2 = 0x03;
      } else {
        temp2 = 0x00;
      }
      temp2 <<= 4;
      temp1 <<= 1;
      j++;
      if (temp1 & 0x80) {
        temp2 |= 0x03;
      } else {
        temp2 |= 0x00;
      }
      temp1 <<= 1;
      this->write_byte(temp2);
    }
    App.feed_wdt();
  }
  this->end_data_();
  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper7P5In::get_width_internal() { return 640; }
int WaveshareEPaper7P5In::get_height_internal() { return 384; }
void WaveshareEPaper7P5In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}
bool WaveshareEPaper7P5InV2::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    this->command(0x71);
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    App.feed_wdt();
    delay(10);
  }
  return true;
}
void WaveshareEPaper7P5InV2::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x07);
  this->data(0x07);
  this->data(0x3f);
  this->data(0x3f);

  // We don't want the display to be powered at this point

  delay(100);  // NOLINT
  this->wait_until_idle_();

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x10);
  this->data(0x07);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x1F);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x03);
  this->data(0x20);
  this->data(0x01);
  this->data(0xE0);

  // COMMAND DUAL SPI MM_EN, DUSPI_EN
  this->command(0x15);
  this->data(0x00);

  // COMMAND POWER DRIVER HAT DOWN
  // This command will turn off booster, controller, source driver, gate driver, VCOM, and
  // temperature sensor, but register data will be kept until VDD turned OFF or Deep Sleep Mode.
  // Source/Gate/Border/VCOM will be released to floating.
  this->command(0x02);
}
void HOT WaveshareEPaper7P5InV2::display() {
  uint32_t buf_len = this->get_buffer_length_();

  // COMMAND POWER ON
  ESP_LOGI(TAG, "Power on the display and hat");

  // This command will turn on booster, controller, regulators, and temperature sensor will be
  // activated for one-time sensing before enabling booster. When all voltages are ready, the
  // BUSY_N signal will return to high.
  this->command(0x04);
  delay(200);  // NOLINT
  this->wait_until_idle_();

  // COMMAND DATA START TRANSMISSION NEW DATA
  this->command(0x13);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(~(this->buffer_[i]));
  }

  delay(100);  // NOLINT
  this->wait_until_idle_();

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(100);  // NOLINT
  this->wait_until_idle_();

  ESP_LOGV(TAG, "Before command(0x02) (>> power off)");
  this->command(0x02);
  this->wait_until_idle_();
  ESP_LOGV(TAG, "After command(0x02) (>> power off)");
}

int WaveshareEPaper7P5InV2::get_width_internal() { return 800; }
int WaveshareEPaper7P5InV2::get_height_internal() { return 480; }
uint32_t WaveshareEPaper7P5InV2::idle_timeout_() { return 10000; }
void WaveshareEPaper7P5InV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5inV2rev2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

/* 7.50inV2alt */
bool WaveshareEPaper7P5InV2alt::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    this->command(0x71);
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGI(TAG, "Timeout while displaying image!");
      return false;
    }
    delay(10);
  }
  return true;
}

void WaveshareEPaper7P5InV2alt::initialize() {
  this->reset_();

  // COMMAND POWER SETTING
  this->command(0x01);

  // 1-0=11: internal power
  this->data(0x07);
  this->data(0x17);  // VGH&VGL
  this->data(0x3F);  // VSH
  this->data(0x26);  // VSL
  this->data(0x11);  // VSHR

  // VCOM DC Setting
  this->command(0x82);
  this->data(0x24);  // VCOM

  // Booster Setting
  this->command(0x06);
  this->data(0x27);
  this->data(0x27);
  this->data(0x2F);
  this->data(0x17);

  // POWER ON
  this->command(0x04);

  delay(100);  // NOLINT
  this->wait_until_idle_();
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x3F);  // KW-3f   KWR-2F BWROTP 0f BWOTP 1f

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x03);  // source 800
  this->data(0x20);
  this->data(0x01);  // gate 480
  this->data(0xE0);
  // COMMAND ...?
  this->command(0x15);
  this->data(0x00);
  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x10);
  this->data(0x00);
  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);
  // Resolution setting
  this->command(0x65);
  this->data(0x00);
  this->data(0x00);  // 800*480
  this->data(0x00);
  this->data(0x00);

  this->wait_until_idle_();

  uint8_t lut_vcom_7_i_n5_v2[] = {
      0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0xF, 0x1, 0xF, 0x1, 0x2, 0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_ww_7_i_n5_v2[] = {
      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_bw_7_i_n5_v2[] = {
      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_wb_7_i_n5_v2[] = {
      0x80, 0xF, 0xF, 0x0, 0x0, 0x3, 0x84, 0xF, 0x1, 0xF, 0x1, 0x4, 0x40, 0xF, 0xF, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_bb_7_i_n5_v2[] = {
      0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t count;
  this->command(0x20);  // VCOM
  for (count = 0; count < 42; count++)
    this->data(lut_vcom_7_i_n5_v2[count]);

  this->command(0x21);  // LUTBW
  for (count = 0; count < 42; count++)
    this->data(lut_ww_7_i_n5_v2[count]);

  this->command(0x22);  // LUTBW
  for (count = 0; count < 42; count++)
    this->data(lut_bw_7_i_n5_v2[count]);

  this->command(0x23);  // LUTWB
  for (count = 0; count < 42; count++)
    this->data(lut_wb_7_i_n5_v2[count]);

  this->command(0x24);  // LUTBB
  for (count = 0; count < 42; count++)
    this->data(lut_bb_7_i_n5_v2[count]);
}

void WaveshareEPaper7P5InV2alt::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5inV2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

/* 7.50in-bc */
void WaveshareEPaper7P5InBC::initialize() {
  /* The command sequence is similar to the 7P5In display but differs in subtle ways
  to allow for faster updates. */
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x37);
  this->data(0x00);

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0xCF);
  this->data(0x08);

  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3A);

  // COMMAND VCM_DC_SETTING: all temperature range
  this->command(0x82);
  this->data(0x28);

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0xC7);
  this->data(0xCC);
  this->data(0x15);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x77);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // COMMAND FLASH CONTROL
  this->command(0x65);
  this->data(0x00);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x02);  // 640 >> 8
  this->data(0x80);
  this->data(0x01);  // 384 >> 8
  this->data(0x80);

  // COMMAND FLASH MODE
  this->command(0xE5);
  this->data(0x03);
}

void HOT WaveshareEPaper7P5InBC::display() {
  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  this->start_data_();

  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    // A line of eight source pixels (each a bit in this byte)
    uint8_t eight_pixels = this->buffer_[i];

    for (uint8_t j = 0; j < 8; j += 2) {
      /* For bichromatic displays, each byte represents two pixels. Each nibble encodes a pixel: 0=white, 3=black,
      4=color. Therefore, e.g. 0x44 = two adjacent color pixels, 0x33 is two adjacent black pixels, etc. If you want
      to draw using the color pixels, change '0x30' with '0x40' and '0x03' with '0x04' below. */
      uint8_t left_nibble = (eight_pixels & 0x80) ? 0x30 : 0x00;
      eight_pixels <<= 1;
      uint8_t right_nibble = (eight_pixels & 0x80) ? 0x03 : 0x00;
      eight_pixels <<= 1;
      this->write_byte(left_nibble | right_nibble);
    }
    App.feed_wdt();
  }
  this->end_data_();

  // Unlike the 7P5In display, we send the "power on" command here rather than during initialization
  // COMMAND POWER ON
  this->command(0x04);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}

int WaveshareEPaper7P5InBC::get_width_internal() { return 640; }

int WaveshareEPaper7P5InBC::get_height_internal() { return 384; }

void WaveshareEPaper7P5InBC::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in-bc");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5InHDB::initialize() {
  this->command(0x12);  // SWRESET

  this->wait_until_idle_();  // waiting for the electronic paper IC to release the idle signal

  this->command(0x46);  // Auto Write RAM
  this->data(0xF7);

  this->wait_until_idle_();  // waiting for the electronic paper IC to release the idle signal

  this->command(0x47);  // Auto Write RAM
  this->data(0xF7);

  this->wait_until_idle_();  // waiting for the electronic paper IC to release the idle signal

  this->command(0x0C);  // Soft start setting
  this->data(0xAE);
  this->data(0xC7);
  this->data(0xC3);
  this->data(0xC0);
  this->data(0x40);

  this->command(0x01);  // Set MUX as 527
  this->data(0xAF);
  this->data(0x02);
  this->data(0x01);

  this->command(0x11);  // Data entry mode
  this->data(0x01);

  this->command(0x44);
  this->data(0x00);  // RAM x address start at 0
  this->data(0x00);
  this->data(0x6F);  // RAM x address end at 36Fh -> 879
  this->data(0x03);

  this->command(0x45);
  this->data(0xAF);  // RAM y address start at 20Fh;
  this->data(0x02);
  this->data(0x00);  // RAM y address end at 00h;
  this->data(0x00);

  this->command(0x3C);  // VBD
  this->data(0x01);     // LUT1, for white

  this->command(0x18);
  this->data(0X80);

  this->command(0x22);
  this->data(0XB1);  // Load Temperature and waveform setting.

  this->command(0x20);

  this->wait_until_idle_();  // waiting for the electronic paper IC to release the idle signal

  this->command(0x4E);
  this->data(0x00);
  this->data(0x00);

  this->command(0x4F);
  this->data(0xAF);
  this->data(0x02);
}

void HOT WaveshareEPaper7P5InHDB::display() {
  this->command(0x4F);
  this->data(0xAf);
  this->data(0x02);

  // BLACK
  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // RED
  this->command(0x26);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0x00);
  this->end_data_();

  this->command(0x22);
  this->data(0xC7);
  this->command(0x20);
  delay(100);  // NOLINT
}

int WaveshareEPaper7P5InHDB::get_width_internal() { return 880; }

int WaveshareEPaper7P5InHDB::get_height_internal() { return 528; }

void WaveshareEPaper7P5InHDB::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in-HD-b");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

static const uint8_t LUT_SIZE_TTGO_DKE_PART = 153;

static const uint8_t PART_UPDATE_LUT_TTGO_DKE[LUT_SIZE_TTGO_DKE_PART] = {
    0x0, 0x40, 0x0, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x80, 0x80, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x40, 0x40, 0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x80, 0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0xF, 0x0,  0x0, 0x0, 0x0,  0x0,  0x0,  0x4,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x1, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x1,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0,
    // 0x22,   0x17,   0x41,   0x0,    0x32,   0x32
};

void WaveshareEPaper2P13InDKE::initialize() {}
void HOT WaveshareEPaper2P13InDKE::display() {
  bool partial = this->at_update_ != 0;
  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;

  if (partial) {
    ESP_LOGI(TAG, "Performing partial e-paper update.");
  } else {
    ESP_LOGI(TAG, "Performing full e-paper update.");
  }

  // start and set up data format
  this->command(0x12);
  this->wait_until_idle_();

  this->command(0x11);
  this->data(0x03);
  this->command(0x44);
  this->data(1);
  this->data(this->get_width_internal() / 8);
  this->command(0x45);
  this->data(0);
  this->data(0);
  this->data(this->get_height_internal());
  this->data(0);
  this->command(0x4e);
  this->data(1);
  this->command(0x4f);
  this->data(0);
  this->data(0);

  if (!partial) {
    // send data
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // commit
    this->command(0x20);
    this->wait_until_idle_();
  } else {
    // set up partial update
    this->command(0x32);
    this->start_data_();
    this->write_array(PART_UPDATE_LUT_TTGO_DKE, sizeof(PART_UPDATE_LUT_TTGO_DKE));
    this->end_data_();
    this->command(0x3F);
    this->data(0x22);

    this->command(0x03);
    this->data(0x17);
    this->command(0x04);
    this->data(0x41);
    this->data(0x00);
    this->data(0x32);
    this->command(0x2C);
    this->data(0x32);

    this->command(0x37);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x40);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);

    this->command(0x3C);
    this->data(0x80);
    this->command(0x22);
    this->data(0xC0);
    this->command(0x20);
    this->wait_until_idle_();

    // send data
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // commit as partial
    this->command(0x22);
    this->data(0xCF);
    this->command(0x20);
    this->wait_until_idle_();

    // data must be sent again on partial update
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();
  }

  ESP_LOGI(TAG, "Completed e-paper update.");
}

int WaveshareEPaper2P13InDKE::get_width_internal() { return 128; }
int WaveshareEPaper2P13InDKE::get_height_internal() { return 250; }
uint32_t WaveshareEPaper2P13InDKE::idle_timeout_() { return 5000; }
void WaveshareEPaper2P13InDKE::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.13inDKE");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper2P13InDKE::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

// ========================================================
//               13.3in (K version)
// Datasheet/Specification/Reference:
//  - https://files.waveshare.com/wiki/13.3inch-e-Paper-HAT-(K)/13.3-inch-e-Paper-(K)-user-manual.pdf
//  - https://github.com/waveshareteam/e-Paper/tree/master/Arduino/epd13in3k
// ========================================================

// using default wait_until_idle_() function
void WaveshareEPaper13P3InK::initialize() {
  this->wait_until_idle_();
  this->command(0x12);  // SWRESET
  this->wait_until_idle_();

  this->command(0x0c);  // set soft start
  this->data(0xae);
  this->data(0xc7);
  this->data(0xc3);
  this->data(0xc0);
  this->data(0x80);

  this->command(0x01);                            // driver output control
  this->data((get_height_internal() - 1) % 256);  // Y
  this->data((get_height_internal() - 1) / 256);  // Y
  this->data(0x00);

  this->command(0x11);  // data entry mode
  this->data(0x03);

  // SET WINDOWS
  // XRAM_START_AND_END_POSITION
  this->command(0x44);
  this->data(0 & 0xFF);
  this->data((0 >> 8) & 0x03);
  this->data((get_width_internal() - 1) & 0xFF);
  this->data(((get_width_internal() - 1) >> 8) & 0x03);
  // YRAM_START_AND_END_POSITION
  this->command(0x45);
  this->data(0 & 0xFF);
  this->data((0 >> 8) & 0x03);
  this->data((get_height_internal() - 1) & 0xFF);
  this->data(((get_height_internal() - 1) >> 8) & 0x03);

  this->command(0x3C);  // Border setting
  this->data(0x01);

  this->command(0x18);  // use the internal temperature sensor
  this->data(0x80);

  // SET CURSOR
  // XRAM_ADDRESS
  this->command(0x4E);
  this->data(0 & 0xFF);
  this->data((0 >> 8) & 0x03);
  // YRAM_ADDRESS
  this->command(0x4F);
  this->data(0 & 0xFF);
  this->data((0 >> 8) & 0x03);
}
void HOT WaveshareEPaper13P3InK::display() {
  // do single full update
  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // COMMAND DISPLAY REFRESH
  this->command(0x22);
  this->data(0xF7);
  this->command(0x20);
}

int WaveshareEPaper13P3InK::get_width_internal() { return 960; }
int WaveshareEPaper13P3InK::get_height_internal() { return 680; }
uint32_t WaveshareEPaper13P3InK::idle_timeout_() { return 10000; }
void WaveshareEPaper13P3InK::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 13.3inK");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace waveshare_epaper
}  // namespace esphome
