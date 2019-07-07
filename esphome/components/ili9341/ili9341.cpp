#include "ili9341.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ili9341 {

static const char *TAG = "waveshare_epaper";

static const uint8_t FULL_UPDATE_LUT[30] = {0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 0x66, 0x69,
                                            0x69, 0x59, 0x58, 0x99, 0x99, 0x88, 0x00, 0x00, 0x00, 0x00,
                                            0xF8, 0xB4, 0x13, 0x51, 0x35, 0x51, 0x51, 0x19, 0x01, 0x00};

static const uint8_t PARTIAL_UPDATE_LUT[30] = {0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x13, 0x14, 0x44, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void ili9341::setup_pins_() {
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
float ili9341::get_setup_priority() const { return setup_priority::PROCESSOR; }
void ili9341::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}
void ili9341::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}
bool ili9341::wait_until_idle_() {
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
void ili9341::update() {
  this->do_update_();
  this->display();
}
void ili9341::fill(int color) {
  // flip logic
  const uint8_t fill = color ? 0x00 : 0xFF;
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void HOT ili9341::draw_absolute_pixel_internal(int x, int y, int color) {
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
uint32_t ili9341::get_buffer_length_() { return this->get_width_internal() * this->get_height_internal() / 8u; }
void ili9341::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}
void ili9341::end_command_() { this->disable(); }
void ili9341::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void ili9341::end_data_() { this->disable(); }
void ili9341::on_safe_shutdown() { this->deep_sleep(); }

// ========================================================
//                          Type A
// ========================================================

void ili9341_M5Stack::initialize() {
//NEED to implement

}
void ili9341_M5Stack::dump_config() {
  LOG_DISPLAY("", "ili9341", this);
  ESP_LOGCONFIG(TAG, "  Full Update Every: %u", this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void HOT ili9341_M5Stack::display() {
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
  // COMMAND SET RAM X ADDRESS START END POSITION
  //this->command(0x44);
  //this->data(0x00);
  this->data((this->get_width_internal() - 1) >> 3);
  // COMMAND SET RAM Y ADDRESS START END POSITION
  // this->command(0x45);
  // this->data(0x00);
  // this->data(0x00);
  this->data(this->get_height_internal() - 1);
  this->data((this->get_height_internal() - 1) >> 8);

  // COMMAND SET RAM X ADDRESS COUNTER
  // this->command(0x4E);
  // this->data(0x00);
  // COMMAND SET RAM Y ADDRESS COUNTER
  // this->command(0x4F);
  // this->data(0x00);
  // this->data(0x00);

  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    return;
  }

  // COMMAND WRITE RAM
  // this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // COMMAND DISPLAY UPDATE CONTROL 2
  // this->command(0x22);
  // this->data(0xC4);
  // COMMAND MASTER ACTIVATION
  // this->command(0x20);
  // COMMAND TERMINATE FRAME READ WRITE
  // this->command(0xFF);

  this->status_clear_warning();
}
int ili9341_M5Stack::get_width_internal() {
  return 320;
}
int ili9341_M5Stack::get_height_internal() {
  return 240;
}
void ili9341_M5Stack::write_lut_(const uint8_t *lut) {
  // COMMAND WRITE LUT REGISTER
  //this->command(0x32);
  for (uint8_t i = 0; i < 30; i++)
    this->data(lut[i]);
}

void ili9341_M5Stack::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

}  // namespace ili9341
}  // namespace esphome
