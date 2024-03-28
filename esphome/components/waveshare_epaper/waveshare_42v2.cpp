#include "waveshare_42v2.h"
#include <cstdint>
#include "esphome/core/log.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_4.2v2";

void WaveshareEPaper4P2InV2::display() {
  ESP_LOGD(TAG, "Performing full update");
  this->full_update_();
}

void WaveshareEPaper4P2InV2::full_update_() {
  this->reset_();
  this->wait_until_idle_();

  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  this->command(0x26);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  this->turn_on_display_full_();

  this->deep_sleep();
}

void WaveshareEPaper4P2InV2::turn_on_display_full_() {
  this->command(0x22);
  this->data(0xc7);
  this->command(0x20);

  this->wait_until_idle_();
}

void WaveshareEPaper4P2InV2::set_window_(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2) {
  this->command(0x44);  // SET_RAM_X_ADDRESS_START_END_POSITION
  this->data((x >> 3) & 0xFF);
  this->data((x2 >> 3) & 0xFF);

  this->command(0x45);  // SET_RAM_Y_ADDRESS_START_END_POSITION
  this->data(y & 0xFF);
  this->data((y >> 8) & 0xFF);
  this->data(62 & 0xFF);
  this->data((y2 >> 8) & 0xFF);
}

void WaveshareEPaper4P2InV2::clear_() {
  uint8_t *buffer = (uint8_t *) calloc(this->get_buffer_length_(), sizeof(uint8_t));
  memset(buffer, 0xff, this->get_buffer_length_());

  this->command(0x24);
  this->start_data_();
  this->write_array(buffer, this->get_buffer_length_());
  this->end_data_();

  this->command(0x26);
  this->start_data_();
  this->write_array(buffer, this->get_buffer_length_());
  this->end_data_();

  free(buffer);
}

void WaveshareEPaper4P2InV2::set_cursor_(uint16_t x, uint16_t y) {
  this->command(0x4E);  // SET_RAM_X_ADDRESS_COUNTER
  this->data(x & 0xFF);

  this->command(0x4F);  // SET_RAM_Y_ADDRESS_COUNTER
  this->data(y & 0xFF);
  this->data((y >> 8) & 0xFF);
}

void WaveshareEPaper4P2InV2::fast_initialize_() {
#define MODE_1_SECOND 1
#define MODE_1_5_SECOND 0

  this->reset_();
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }
  this->command(0x12);  // soft  reset
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }
  this->command(0x21);
  this->data(0x40);
  this->data(0x00);

  this->command(0x3C);
  this->data(0x05);

#if MODE_1_5_SECOND
  // 1.5s
  this->command(0x1A);  // Write to temperature register
  this->data(0x6E);
#endif
#if MODE_1_SECOND
  // 1s
  this->command(0x1A);  // Write to temperature register
  this->data(0x5A);
#endif

  this->command(0x22);  // Load temperature value
  this->data(0x91);
  this->command(0x20);
  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }
  this->command(0x11);  // data  entry  mode
  this->data(0x03);     // X-mode

  this->set_window_(0, 0, this->get_width_internal() - 1, this->get_height_internal() - 1);
  this->set_cursor_(0, 0);

  if (!this->wait_until_idle_()) {
    ESP_LOGW(TAG, "wait_until_idle_ returned FALSE. Is your busy pin set?");
  }

  this->clear_();
  this->turn_on_display_full_();
}

void WaveshareEPaper4P2InV2::initialize() {
  this->fast_initialize_();
}

void WaveshareEPaper4P2InV2::deep_sleep() {
  this->command(0x10);
  this->data(0x01);
  delay(200);
}

void WaveshareEPaper4P2InV2::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

int WaveshareEPaper4P2InV2::get_width_internal() { return 400; }

int WaveshareEPaper4P2InV2::get_height_internal() { return 300; }

void WaveshareEPaper4P2InV2::reset_() {
  this->reset_pin_->digital_write(true);  // Note: Should be inverted logic
  delay(100);
  this->reset_pin_->digital_write(false);  // Note: Should be inverted logic according to the docs
  delay(this->reset_duration_);
  this->reset_pin_->digital_write(true);  // Note: Should be inverted logic
  delay(100);
}

}  // namespace waveshare_epaper
}  // namespace esphome