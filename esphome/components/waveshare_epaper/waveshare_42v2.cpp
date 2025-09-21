#include "waveshare_42v2.h"
#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_4.2v2";

// Taken from https://files.waveshare.com/upload/9/97/4.2-inch-e-Paper-V2-user-manual.pdf

static const uint8_t SET_GATE_DRIVING_VOLTAGE = 0x03;
static const uint8_t SET_SOURCE_DRIVING_VOLTAGE = 0x04;
static const uint8_t BOOSTER_SOFT_START_CONTROL = 0x0c;
static const uint8_t DEEP_SLEEP = 0x10;
static const uint8_t DATA_ENTRY_MODE = 0x11;
// Software / Soft Reset
static const uint8_t SW_RESET = 0x12;
static const uint8_t WRITE_TO_TEMPERATURE_REGISTER = 0x1a;
static const uint8_t MASTER_ACTIVATION = 0x20;
static const uint8_t DISPLAY_UPDATE_CONTROL_1 = 0x21;
static const uint8_t DISPLAY_UPDATE_CONTROL_2 = 0x22;
static const uint8_t BORDER_WAVEFORM_CONTROL = 0x3c;
// Write the black/white values to RAM
static const uint8_t WRITE_RAM_BW = 0x24;
// Write the secondary colour values to RAM (errata in data sheet, likely from a BWR model)
static const uint8_t WRITE_RAM_RED = 0x26;
static const uint8_t WRITE_VCOM_REGISTER = 0x2c;
// Write LUT register (227 bytes)
static const uint8_t WRITE_LUT_REGISTER = 0x32;
// Option for LUT end
static const uint8_t END_OPTION = 0x3f;
// Sets the X windowing positions
static const uint8_t SET_RAM_X_START_END_POSITION = 0x44;
// Sets the Y windowing postiions
static const uint8_t SET_RAM_Y_START_END_POSITION = 0x45;
// Set X address counter
static const uint8_t SET_RAM_X_ADDRESS = 0x4e;
// Set the Y address counter
static const uint8_t SET_RAM_Y_ADDRESS = 0x4f;

static const uint8_t LUT_ALL[233] = {
    0x01, 0x0A, 0x1B, 0x0F, 0x03, 0x01, 0x01, 0x05, 0x0A, 0x01, 0x0A, 0x01, 0x01, 0x01, 0x05, 0x08, 0x03, 0x02,
    0x04, 0x01, 0x01, 0x01, 0x04, 0x04, 0x02, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x0A, 0x1B, 0x0F, 0x03, 0x01, 0x01, 0x05, 0x4A, 0x01, 0x8A, 0x01,
    0x01, 0x01, 0x05, 0x48, 0x03, 0x82, 0x84, 0x01, 0x01, 0x01, 0x84, 0x84, 0x82, 0x00, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x0A, 0x1B, 0x8F, 0x03, 0x01,
    0x01, 0x05, 0x4A, 0x01, 0x8A, 0x01, 0x01, 0x01, 0x05, 0x48, 0x83, 0x82, 0x04, 0x01, 0x01, 0x01, 0x04, 0x04,
    0x02, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x8A, 0x1B, 0x8F, 0x03, 0x01, 0x01, 0x05, 0x4A, 0x01, 0x8A, 0x01, 0x01, 0x01, 0x05, 0x48, 0x83, 0x02,
    0x04, 0x01, 0x01, 0x01, 0x04, 0x04, 0x02, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x8A, 0x9B, 0x8F, 0x03, 0x01, 0x01, 0x05, 0x4A, 0x01, 0x8A, 0x01,
    0x01, 0x01, 0x05, 0x48, 0x03, 0x42, 0x04, 0x01, 0x01, 0x01, 0x04, 0x04, 0x42, 0x00, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x07, 0x17, 0x41, 0xA8, 0x32, 0x30,
};

uint32_t WaveshareEPaper4P2InV2::get_buffer_length_() {
  if (this->display_mode_ == MODE_GRAYSCALE4) {
    // black and gray buffer
    return this->get_width_controller() * this->get_height_internal() / 4u;
  } else {
    // just a black buffer
    return this->get_width_controller() * this->get_height_internal() / 8u;
  }
}

void WaveshareEPaper4P2InV2::display() {
  if (this->is_busy_ || (this->busy_pin_ != nullptr && this->busy_pin_->digital_read()))
    return;
  this->is_busy_ = true;
  if (this->display_mode_ == MODE_GRAYSCALE4) {
    ESP_LOGD(TAG, "Performing grayscale4 update");
    this->update_(MODE_GRAYSCALE4);
  } else if (this->display_mode_ == MODE_FAST) {
    ESP_LOGD(TAG, "Performing fast update");
    this->update_(MODE_FAST);
  } else {
    const bool partial = this->at_update_ != 0;
    this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
    if (partial) {
      ESP_LOGD(TAG, "Performing partial update");
      this->update_(MODE_PARTIAL);
    } else {
      ESP_LOGD(TAG, "Performing full update");
      // this->init_();
      this->update_(MODE_FULL);
    }
  }
}

void WaveshareEPaper4P2InV2::update_(DisplayMode mode) {
  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;

  this->reset_();
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  if (mode == MODE_PARTIAL) {
    this->command(DISPLAY_UPDATE_CONTROL_1);
    this->data(0x00);
    this->data(0x00);

    this->command(BORDER_WAVEFORM_CONTROL);
    this->data(0x80);

    //    this->set_window_(0, 0, this->get_width_internal() - 1, this->get_height_internal() - 1);
    //    this->set_cursor_(0, 0);
  } else if (mode == MODE_FULL) {
    this->command(DISPLAY_UPDATE_CONTROL_1);
    this->data(0x40);
    this->data(0x00);

    this->command(BORDER_WAVEFORM_CONTROL);
    this->data(0x05);
  }

  this->command(WRITE_RAM_BW);
  this->start_data_();
  if (mode == MODE_GRAYSCALE4) {
    this->write_array(this->buffer_, buf_half_len);
  } else {
    this->write_array(this->buffer_, this->get_buffer_length_());
  }
  this->end_data_();

  // new data
  if ((mode == MODE_FULL) || (mode == MODE_FAST)) {
    this->command(WRITE_RAM_RED);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();
  } else if (mode == MODE_GRAYSCALE4) {
    this->command(WRITE_RAM_RED);
    this->start_data_();
    this->write_array(this->buffer_ + buf_half_len, buf_half_len);
    this->end_data_();
  }

  this->turn_on_display_(mode);

  this->deep_sleep();
  this->is_busy_ = false;
}

void WaveshareEPaper4P2InV2::turn_on_display_(DisplayMode mode) {
  this->command(DISPLAY_UPDATE_CONTROL_2);
  switch (mode) {
    case MODE_GRAYSCALE4:
      this->data(0xcf);
      break;
    case MODE_PARTIAL:
      this->data(0xff);
      break;
    case MODE_FAST:
      this->data(0xc7);
      break;
    case MODE_FULL:
    default:
      this->data(0xf7);
      break;
  }
  this->command(MASTER_ACTIVATION);

  // possible timeout.
  this->wait_until_idle_();
}

void WaveshareEPaper4P2InV2::set_window_(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2) {
  this->command(SET_RAM_X_START_END_POSITION);
  this->data((x >> 3) & 0xFF);
  this->data((x2 >> 3) & 0xFF);

  this->command(SET_RAM_Y_START_END_POSITION);
  this->data(y & 0xFF);
  this->data((y >> 8) & 0xFF);
  this->data(62 & 0xFF);
  this->data((y2 >> 8) & 0xFF);
}

void WaveshareEPaper4P2InV2::clear_() {
  uint32_t bufflen;
  if (this->display_mode_ == MODE_GRAYSCALE4) {
    bufflen = this->get_buffer_length_() / 2u;
  } else {
    bufflen = this->get_buffer_length_();
  }
  uint8_t *buffer = (uint8_t *) calloc(bufflen, sizeof(uint8_t));
  memset(buffer, 0xff, bufflen);

  this->command(WRITE_RAM_BW);
  this->start_data_();
  this->write_array(buffer, bufflen);
  this->end_data_();

  this->command(WRITE_RAM_RED);
  this->start_data_();
  this->write_array(buffer, bufflen);
  this->end_data_();

  free(buffer);
}

void WaveshareEPaper4P2InV2::set_cursor_(uint16_t x, uint16_t y) {
  this->command(SET_RAM_X_ADDRESS);
  this->data(x & 0xFF);

  this->command(SET_RAM_Y_ADDRESS);
  this->data(y & 0xFF);
  this->data((y >> 8) & 0xFF);
}

void WaveshareEPaper4P2InV2::write_lut_() {
  this->command(WRITE_LUT_REGISTER);
  this->start_data_();
  this->write_array(LUT_ALL, 227);
  this->end_data_();

  this->command(END_OPTION);
  this->data(LUT_ALL[227]);

  this->command(SET_GATE_DRIVING_VOLTAGE);
  this->data(LUT_ALL[228]);

  this->command(SET_SOURCE_DRIVING_VOLTAGE);
  this->data(LUT_ALL[229]);
  this->data(LUT_ALL[230]);
  this->data(LUT_ALL[231]);

  this->command(WRITE_VCOM_REGISTER);
  this->data(LUT_ALL[232]);
}

void WaveshareEPaper4P2InV2::initialize_internal_(DisplayMode mode) {
  static const uint8_t MODE_1_SECOND = 1;
  static const uint8_t MODE_1_5_SECOND = 0;

  this->reset_();
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }
  this->command(SW_RESET);
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  this->command(DISPLAY_UPDATE_CONTROL_1);
  if (mode == MODE_GRAYSCALE4) {
    this->data(0x00);
  } else {
    this->data(0x40);
  }
  this->data(0x00);

  this->command(BORDER_WAVEFORM_CONTROL);
  if (mode == MODE_GRAYSCALE4) {
    this->data(0x03);
  } else {
    this->data(0x05);
  }

  if (mode == MODE_FAST) {
#if MODE_1_5_SECOND
    // 1.5s
    this->command(WRITE_TO_TEMPERATURE_REGISTER);
    this->data(0x6E);
#endif
#if MODE_1_SECOND
    // 1s
    this->command(WRITE_TO_TEMPERATURE_REGISTER);
    this->data(0x5A);
#endif

    this->command(DISPLAY_UPDATE_CONTROL_2);
    this->data(0x91);  // Enable Clock Signal, Load LUT with Display Mode 1, Disable Clock Signal
    this->command(MASTER_ACTIVATION);
    if (!this->wait_until_idle_()) {
      ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
    }
  } else if (mode == MODE_GRAYSCALE4) {
    this->command(BOOSTER_SOFT_START_CONTROL);
    this->data(0x8B);
    this->data(0x9C);
    this->data(0xA4);
    this->data(0x0F);

    this->write_lut_();
  }

  this->command(DATA_ENTRY_MODE);
  this->data(0x03);  // X-mode

  this->set_window_(0, 0, this->get_width_internal() - 1, this->get_height_internal() - 1);
  this->set_cursor_(0, 0);

  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  this->clear_();
  this->turn_on_display_(mode);
}

void WaveshareEPaper4P2InV2::initialize() {
  if (this->display_mode_ == MODE_PARTIAL) {
    this->initialize_internal_(MODE_FULL);
  } else {
    this->initialize_internal_(this->display_mode_);
  }
}

uint32_t WaveshareEPaper4P2InV2::idle_timeout_() {
  if (this->display_mode_ == MODE_GRAYSCALE4) {
    return 1000;
  } else {
    return 100;
  }
}

void WaveshareEPaper4P2InV2::deep_sleep() {
  this->command(DEEP_SLEEP);
  this->data(0x01);  // Deep Sleep Mode 1
  // Instead of delay(200), we'll rely on the busy pin to indicate when the display is ready
  // The busy pin will be checked in wait_until_idle_() before any new operations
}

void WaveshareEPaper4P2InV2::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

int WaveshareEPaper4P2InV2::get_width_internal() { return 400; }

int WaveshareEPaper4P2InV2::get_height_internal() { return 300; }

void WaveshareEPaper4P2InV2::reset_() {
  // The reset sequence is now handled through the busy pin and wait_until_idle_()
  // Note: Documentation implies that the reset pin should be inverted compared to this
  this->reset_pin_->digital_write(true);
  this->wait_until_idle_();
  this->reset_pin_->digital_write(false);
  this->wait_until_idle_();
  this->reset_pin_->digital_write(true);
  this->wait_until_idle_();
}

void HOT WaveshareEPaper4P2InV2::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;
  const uint32_t pos = (x + y * this->get_width_controller()) / 8u;
  const uint8_t subpos = x & 0x07;
  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;

  if (this->display_mode_ == MODE_GRAYSCALE4) {
    uint8_t gray = (color.white != 0) ? color.white : ((color.red * 299 + color.green * 587 + color.blue * 114) / 1000);

    if (gray >= 192) {
      // White (00)
      this->buffer_[pos] &= ~(0x80 >> subpos);
      this->buffer_[pos + buf_half_len] &= ~(0x80 >> subpos);
    } else if (gray >= 128) {
      // Light Gray (10)
      this->buffer_[pos] |= (0x80 >> subpos);
      this->buffer_[pos + buf_half_len] &= ~(0x80 >> subpos);
    } else if (gray >= 64) {
      // Dark Gray (01)
      this->buffer_[pos] &= ~(0x80 >> subpos);
      this->buffer_[pos + buf_half_len] |= (0x80 >> subpos);
    } else {
      // Black (11)
      this->buffer_[pos] |= (0x80 >> subpos);
      this->buffer_[pos + buf_half_len] |= (0x80 >> subpos);
    }
  } else {
    // Binary mode logic
    if (!color.is_on()) {
      this->buffer_[pos] &= ~(0x80 >> subpos);
    } else {
      if ((color.r > 0) || (color.g > 0) || (color.b > 0)) {
        this->buffer_[pos] &= ~(0x80 >> subpos);
      } else {
        this->buffer_[pos] |= (0x80 >> subpos);
      }
    }
  }
}

void WaveshareEPaper4P2InV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 4.20inV2");
  if (this->display_mode_ == MODE_GRAYSCALE4) {
    ESP_LOGCONFIG(TAG, "  Display Mode: 4 Grayscale");
  } else if (this->display_mode_ == MODE_FAST) {
    ESP_LOGCONFIG(TAG, "  Display Mode: Fast");
  }
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace waveshare_epaper
}  // namespace esphome
