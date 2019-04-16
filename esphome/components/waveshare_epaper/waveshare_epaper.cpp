#include "waveshare_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace waveshare_epaper {

static const char *TAG = "waveshare_epaper";

static const uint8_t WAVESHARE_EPAPER_COMMAND_DRIVER_OUTPUT_CONTROL = 0x01;
static const uint8_t WAVESHARE_EPAPER_COMMAND_BOOSTER_SOFT_START_CONTROL = 0x0C;
// static const uint8_t WAVESHARE_EPAPER_COMMAND_GATE_SCAN_START_POSITION = 0x0F;
// static const uint8_t WAVESHARE_EPAPER_COMMAND_DEEP_SLEEP_MODE = 0x10;
static const uint8_t WAVESHARE_EPAPER_COMMAND_DATA_ENTRY_MODE_SETTING = 0x11;
// static const uint8_t WAVESHARE_EPAPER_COMMAND_SW_RESET = 0x12;
// static const uint8_t WAVESHARE_EPAPER_COMMAND_TEMPERATURE_SENSOR_CONTROL = 0x1A;
static const uint8_t WAVESHARE_EPAPER_COMMAND_MASTER_ACTIVATION = 0x20;
// static const uint8_t WAVESHARE_EPAPER_COMMAND_DISPLAY_UPDATE_CONTROL_1 = 0x21;
static const uint8_t WAVESHARE_EPAPER_COMMAND_DISPLAY_UPDATE_CONTROL_2 = 0x22;
static const uint8_t WAVESHARE_EPAPER_COMMAND_WRITE_RAM = 0x24;
static const uint8_t WAVESHARE_EPAPER_COMMAND_WRITE_VCOM_REGISTER = 0x2C;
static const uint8_t WAVESHARE_EPAPER_COMMAND_WRITE_LUT_REGISTER = 0x32;
static const uint8_t WAVESHARE_EPAPER_COMMAND_SET_DUMMY_LINE_PERIOD = 0x3A;
static const uint8_t WAVESHARE_EPAPER_COMMAND_SET_GATE_TIME = 0x3B;
static const uint8_t WAVESHARE_EPAPER_COMMAND_BORDER_WAVEFORM_CONTROL = 0x3C;
static const uint8_t WAVESHARE_EPAPER_COMMAND_SET_RAM_X_ADDRESS_START_END_POSITION = 0x44;
static const uint8_t WAVESHARE_EPAPER_COMMAND_SET_RAM_Y_ADDRESS_START_END_POSITION = 0x45;
static const uint8_t WAVESHARE_EPAPER_COMMAND_SET_RAM_X_ADDRESS_COUNTER = 0x4E;
static const uint8_t WAVESHARE_EPAPER_COMMAND_SET_RAM_Y_ADDRESS_COUNTER = 0x4F;
static const uint8_t WAVESHARE_EPAPER_COMMAND_TERMINATE_FRAME_READ_WRITE = 0xFF;

// not in .text section since only 30 bytes
static const uint8_t FULL_UPDATE_LUT[30] = {0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 0x66, 0x69,
                                            0x69, 0x59, 0x58, 0x99, 0x99, 0x88, 0x00, 0x00, 0x00, 0x00,
                                            0xF8, 0xB4, 0x13, 0x51, 0x35, 0x51, 0x51, 0x19, 0x01, 0x00};

static const uint8_t PARTIAL_UPDATE_LUT[30] = {0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x13, 0x14, 0x44, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void WaveshareEPaper::setup_pins_() {
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

  // Reset
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(200);
    this->reset_pin_->digital_write(true);
    delay(200);
  }
}
float WaveshareEPaper::get_setup_priority() const { return setup_priority::PROCESSOR; }
void WaveshareEPaper::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}
void WaveshareEPaper::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}
bool WaveshareEPaper::is_device_msb_first() { return true; }
bool WaveshareEPaper::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > 1000) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    delay(10);
  }
  return true;
}
void WaveshareEPaper::update() {
  this->do_update_();
  this->display();
}
void WaveshareEPaper::fill(int color) {
  // flip logic
  const uint8_t fill = color ? 0x00 : 0xFF;
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void HOT WaveshareEPaper::draw_absolute_pixel_internal(int x, int y, int color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  const uint32_t pos = (x + y * this->get_width_internal()) / 8u;
  const uint8_t subpos = x & 0x07;
  // flip logic
  if (!color)
    this->buffer_[pos] |= 0x80 >> subpos;
  else
    this->buffer_[pos] &= ~(0x80 >> subpos);
}
uint32_t WaveshareEPaper::get_buffer_length_() { return this->get_width_internal() * this->get_height_internal() / 8u; }
WaveshareEPaper::WaveshareEPaper(GPIOPin *dc_pin) : PollingComponent(0), dc_pin_(dc_pin) {}
bool WaveshareEPaper::is_device_high_speed() { return true; }
void WaveshareEPaper::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}
void WaveshareEPaper::end_command_() { this->disable(); }
void WaveshareEPaper::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void WaveshareEPaper::end_data_() { this->disable(); }

// ========================================================
//                          Type A
// ========================================================

void WaveshareEPaperTypeA::setup() {
  this->setup_pins_();

  this->command(WAVESHARE_EPAPER_COMMAND_DRIVER_OUTPUT_CONTROL);
  this->data(this->get_height_internal() - 1);
  this->data((this->get_height_internal() - 1) >> 8);
  this->data(0x00);  // ? GD = 0, SM = 0, TB = 0

  this->command(WAVESHARE_EPAPER_COMMAND_BOOSTER_SOFT_START_CONTROL);  // ?
  this->data(0xD7);
  this->data(0xD6);
  this->data(0x9D);

  this->command(WAVESHARE_EPAPER_COMMAND_WRITE_VCOM_REGISTER);  // ?
  this->data(0xA8);

  this->command(WAVESHARE_EPAPER_COMMAND_SET_DUMMY_LINE_PERIOD);  // ?
  this->data(0x1A);

  this->command(WAVESHARE_EPAPER_COMMAND_SET_GATE_TIME);  // 2µs per row
  this->data(0x08);

  this->command(WAVESHARE_EPAPER_COMMAND_DATA_ENTRY_MODE_SETTING);
  this->data(0x03);  // from top left to bottom right
}
void WaveshareEPaperTypeA::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
      ESP_LOGCONFIG(TAG, "  Model: 1.54in");
      break;
    case WAVESHARE_EPAPER_2_13_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in");
      break;
    case WAVESHARE_EPAPER_2_9_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.9in");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Full Update Every: %u", this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void HOT WaveshareEPaperTypeA::display() {
  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    return;
  }

  if (this->full_update_every_ >= 2) {
    bool prev_full_update = this->at_update_ == 1;
    bool full_update = this->at_update_ == 0;
    if (full_update != prev_full_update) {
      this->write_lut_(full_update ? FULL_UPDATE_LUT : PARTIAL_UPDATE_LUT);
    }
    this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
  }

  // Set x & y regions we want to write to (full)
  this->command(WAVESHARE_EPAPER_COMMAND_SET_RAM_X_ADDRESS_START_END_POSITION);
  this->data(0x00);
  this->data((this->get_width_internal() - 1) >> 3);
  this->command(WAVESHARE_EPAPER_COMMAND_SET_RAM_Y_ADDRESS_START_END_POSITION);
  this->data(0x00);
  this->data(0x00);
  this->data(this->get_height_internal() - 1);
  this->data((this->get_height_internal() - 1) >> 8);

  this->command(WAVESHARE_EPAPER_COMMAND_SET_RAM_X_ADDRESS_COUNTER);
  this->data(0x00);
  this->command(WAVESHARE_EPAPER_COMMAND_SET_RAM_Y_ADDRESS_COUNTER);
  this->data(0x00);
  this->data(0x00);

  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    return;
  }

  this->command(WAVESHARE_EPAPER_COMMAND_WRITE_RAM);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  this->command(WAVESHARE_EPAPER_COMMAND_DISPLAY_UPDATE_CONTROL_2);
  this->data(0xC4);
  this->command(WAVESHARE_EPAPER_COMMAND_MASTER_ACTIVATION);
  this->command(WAVESHARE_EPAPER_COMMAND_TERMINATE_FRAME_READ_WRITE);

  this->status_clear_warning();
}
int WaveshareEPaperTypeA::get_width_internal() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
      return 200;
    case WAVESHARE_EPAPER_2_13_IN:
      return 128;
    case WAVESHARE_EPAPER_2_9_IN:
      return 128;
  }
  return 0;
}
int WaveshareEPaperTypeA::get_height_internal() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
      return 200;
    case WAVESHARE_EPAPER_2_13_IN:
      return 250;
    case WAVESHARE_EPAPER_2_9_IN:
      return 296;
  }
  return 0;
}
void WaveshareEPaperTypeA::write_lut_(const uint8_t *lut) {
  this->command(WAVESHARE_EPAPER_COMMAND_WRITE_LUT_REGISTER);
  for (uint8_t i = 0; i < 30; i++)
    this->data(lut[i]);
}
WaveshareEPaperTypeA::WaveshareEPaperTypeA(GPIOPin *dc_pin, WaveshareEPaperTypeAModel model)
    : WaveshareEPaper(dc_pin), model_(model) {}
void WaveshareEPaperTypeA::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

// ========================================================
//                          Type B
// ========================================================

static const uint8_t WAVESHARE_EPAPER_B_COMMAND_PANEL_SETTING = 0x00;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_POWER_SETTING = 0x01;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_POWER_OFF = 0x02;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_POWER_OFF_SEQUENCE_SETTING = 0x03;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_POWER_ON = 0x04;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_POWER_MEASURE = 0x05;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_BOOSTER_SOFT_START = 0x06;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_DEEP_SLEEP = 0x07;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_DATA_START_TRANSMISSION_1 = 0x10;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_DATA_STOP = 0x11;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_DISPLAY_REFRESH = 0x12;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_DATA_START_TRANSMISSION_2 = 0x13;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_PARTIAL_DATA_START_TRANSMISSION_1 = 0x14;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_PARTIAL_DATA_START_TRANSMISSION_2 = 0x15;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_PARTIAL_DISPLAY_REFRESH = 0x16;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_LUT_FOR_VCOM = 0x20;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_LUT_WHITE_TO_WHITE = 0x21;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_LUT_BLACK_TO_WHITE = 0x22;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_LUT_WHITE_TO_BLACK = 0x23;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_LUT_BLACK_TO_BLACK = 0x24;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_PLL_CONTROL = 0x30;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_TEMPERATURE_SENSOR_COMMAND = 0x40;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_TEMPERATURE_SENSOR_CALIBRATION = 0x41;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_TEMPERATURE_SENSOR_WRITE = 0x42;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_TEMPERATURE_SENSOR_READ = 0x43;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_VCOM_AND_DATA_INTERVAL_SETTING = 0x50;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_LOW_POWER_DETECTION = 0x51;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_TCON_SETTING = 0x60;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_RESOLUTION_SETTING = 0x61;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_GET_STATUS = 0x71;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_AUTO_MEASURE_VCOM = 0x80;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_VCOM_VALUE = 0x81;
static const uint8_t WAVESHARE_EPAPER_B_COMMAND_VCM_DC_SETTING_REGISTER = 0x82;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_PARTIAL_WINDOW = 0x90;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_PARTIAL_IN = 0x91;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_PARTIAL_OUT = 0x92;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_PROGRAM_MODE = 0xA0;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_ACTIVE_PROGRAM = 0xA1;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_READ_OTP_DATA = 0xA2;
// static const uint8_t WAVESHARE_EPAPER_B_COMMAND_POWER_SAVING = 0xE3;

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

void WaveshareEPaper2P7In::setup() {
  this->setup_pins_();
  // this->buffer_.init(this->get_width_(), this->get_height_());

  this->command(WAVESHARE_EPAPER_B_COMMAND_POWER_SETTING);
  this->data(0x03);  // VDS_EN, VDG_EN
  this->data(0x00);  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
  this->data(0x2B);  // VDH
  this->data(0x2B);  // VDL
  this->data(0x09);  // VDHR

  this->command(WAVESHARE_EPAPER_B_COMMAND_BOOSTER_SOFT_START);
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

  this->command(WAVESHARE_EPAPER_B_COMMAND_PARTIAL_DISPLAY_REFRESH);
  this->data(0x00);

  this->command(WAVESHARE_EPAPER_B_COMMAND_POWER_ON);
  this->wait_until_idle_();
  delay(10);

  this->command(WAVESHARE_EPAPER_B_COMMAND_PANEL_SETTING);
  this->data(0xAF);  // KW-BF   KWR-AF    BWROTP 0f
  this->command(WAVESHARE_EPAPER_B_COMMAND_PLL_CONTROL);
  this->data(0x3A);  // 3A 100HZ   29 150Hz 39 200HZ    31 171HZ
  this->command(WAVESHARE_EPAPER_B_COMMAND_VCM_DC_SETTING_REGISTER);
  this->data(0x12);

  delay(2);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_FOR_VCOM);
  for (uint8_t i : LUT_VCOM_DC_2_7)
    this->data(i);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_WHITE_TO_WHITE);
  for (uint8_t i : LUT_WHITE_TO_WHITE_2_7)
    this->data(i);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_BLACK_TO_WHITE);
  for (uint8_t i : LUT_BLACK_TO_WHITE_2_7)
    this->data(i);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_WHITE_TO_BLACK);
  for (uint8_t i : LUT_WHITE_TO_BLACK_2_7)
    this->data(i);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_BLACK_TO_BLACK);
  for (uint8_t i : LUT_BLACK_TO_BLACK_2_7)
    this->data(i);
}
void HOT WaveshareEPaper2P7In::display() {
  // TODO check active frame buffer to only transmit once / use partial transmits
  this->command(WAVESHARE_EPAPER_B_COMMAND_DATA_START_TRANSMISSION_1);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);
  this->command(WAVESHARE_EPAPER_B_COMMAND_DATA_START_TRANSMISSION_2);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  this->command(WAVESHARE_EPAPER_B_COMMAND_DISPLAY_REFRESH);
}
int WaveshareEPaper2P7In::get_width_internal() { return 176; }
int WaveshareEPaper2P7In::get_height_internal() { return 264; }
WaveshareEPaper2P7In::WaveshareEPaper2P7In(GPIOPin *dc_pin) : WaveshareEPaper(dc_pin) {}
void WaveshareEPaper2P7In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.7in");
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

void WaveshareEPaper4P2In::setup() {
  this->setup_pins_();

  this->command(WAVESHARE_EPAPER_B_COMMAND_POWER_SETTING);
  this->data(0x03);  // VDS_EN, VDG_EN
  this->data(0x00);  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
  this->data(0x2B);  // VDH
  this->data(0x2B);  // VDL
  this->data(0xFF);  // VDHR

  this->command(WAVESHARE_EPAPER_B_COMMAND_BOOSTER_SOFT_START);
  this->data(0x17);
  this->data(0x17);
  this->data(0x17);

  this->command(WAVESHARE_EPAPER_B_COMMAND_POWER_ON);
  this->wait_until_idle_();
  delay(10);
  this->command(WAVESHARE_EPAPER_B_COMMAND_PANEL_SETTING);
  this->data(0xBF);  // KW-BF   KWR-AF  BWROTP 0f
  this->data(0x0B);
  this->command(WAVESHARE_EPAPER_B_COMMAND_PLL_CONTROL);
  this->data(0x3C);  // 3A 100HZ   29 150Hz 39 200HZ  31 171HZ

  delay(2);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_FOR_VCOM);
  for (uint8_t i : LUT_VCOM_DC_4_2)
    this->data(i);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_WHITE_TO_WHITE);
  for (uint8_t i : LUT_WHITE_TO_WHITE_4_2)
    this->data(i);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_BLACK_TO_WHITE);
  for (uint8_t i : LUT_BLACK_TO_WHITE_4_2)
    this->data(i);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_WHITE_TO_BLACK);
  for (uint8_t i : LUT_WHITE_TO_BLACK_4_2)
    this->data(i);
  this->command(WAVESHARE_EPAPER_B_COMMAND_LUT_BLACK_TO_BLACK);
  for (uint8_t i : LUT_BLACK_TO_BLACK_4_2)
    this->data(i);
}
void HOT WaveshareEPaper4P2In::display() {
  this->command(WAVESHARE_EPAPER_B_COMMAND_RESOLUTION_SETTING);
  this->data(0x01);
  this->data(0x90);
  this->data(0x01);
  this->data(0x2C);

  this->command(WAVESHARE_EPAPER_B_COMMAND_VCM_DC_SETTING_REGISTER);
  this->data(0x12);

  this->command(WAVESHARE_EPAPER_B_COMMAND_VCOM_AND_DATA_INTERVAL_SETTING);
  this->data(0x97);

  // TODO check active frame buffer to only transmit once / use partial transmits
  this->command(WAVESHARE_EPAPER_B_COMMAND_DATA_START_TRANSMISSION_1);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);
  this->command(WAVESHARE_EPAPER_B_COMMAND_DATA_START_TRANSMISSION_2);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  this->command(WAVESHARE_EPAPER_B_COMMAND_DISPLAY_REFRESH);
}
int WaveshareEPaper4P2In::get_width_internal() { return 400; }
int WaveshareEPaper4P2In::get_height_internal() { return 300; }
bool WaveshareEPaper4P2In::is_device_high_speed() { return false; }
WaveshareEPaper4P2In::WaveshareEPaper4P2In(GPIOPin *dc_pin) : WaveshareEPaper(dc_pin) {}
void WaveshareEPaper4P2In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 4.2in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5In::setup() {
  this->setup_pins_();

  this->command(WAVESHARE_EPAPER_B_COMMAND_POWER_SETTING);
  this->data(0x37);
  this->data(0x00);

  this->command(WAVESHARE_EPAPER_B_COMMAND_PANEL_SETTING);
  this->data(0xCF);
  this->data(0x0B);

  this->command(WAVESHARE_EPAPER_B_COMMAND_BOOSTER_SOFT_START);
  this->data(0xC7);
  this->data(0xCC);
  this->data(0x28);

  this->command(WAVESHARE_EPAPER_B_COMMAND_POWER_ON);
  this->wait_until_idle_();
  delay(10);

  this->command(WAVESHARE_EPAPER_B_COMMAND_PLL_CONTROL);
  this->data(0x3C);

  this->command(WAVESHARE_EPAPER_B_COMMAND_TEMPERATURE_SENSOR_CALIBRATION);
  this->data(0x00);

  this->command(WAVESHARE_EPAPER_B_COMMAND_VCOM_AND_DATA_INTERVAL_SETTING);
  this->data(0x77);

  this->command(WAVESHARE_EPAPER_B_COMMAND_TCON_SETTING);
  this->data(0x22);

  this->command(WAVESHARE_EPAPER_B_COMMAND_RESOLUTION_SETTING);
  this->data(0x02);
  this->data(0x80);
  this->data(0x01);
  this->data(0x80);

  this->command(WAVESHARE_EPAPER_B_COMMAND_VCM_DC_SETTING_REGISTER);
  this->data(0x1E);

  this->command(0xE5);
  this->data(0x03);
}
void HOT WaveshareEPaper7P5In::display() {
  this->command(WAVESHARE_EPAPER_B_COMMAND_DATA_START_TRANSMISSION_1);

  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    uint8_t temp1 = this->buffer_[i];
    for (uint8_t j = 0; j < 8; j++) {
      uint8_t temp2;
      if (temp1 & 0x80)
        temp2 = 0x03;
      else
        temp2 = 0x00;

      temp2 <<= 4;
      temp1 <<= 1;
      j++;
      if (temp1 & 0x80)
        temp2 |= 0x03;
      else
        temp2 |= 0x00;
      temp1 <<= 1;
      this->write_byte(temp2);
    }

    App.feed_wdt();
  }
  this->end_data_();

  this->command(WAVESHARE_EPAPER_B_COMMAND_DISPLAY_REFRESH);
}
int WaveshareEPaper7P5In::get_width_internal() { return 640; }
int WaveshareEPaper7P5In::get_height_internal() { return 384; }
WaveshareEPaper7P5In::WaveshareEPaper7P5In(GPIOPin *dc_pin) : WaveshareEPaper(dc_pin) {}
void WaveshareEPaper7P5In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace waveshare_epaper
}  // namespace esphome
