#include "cst226_touchscreen.h"

namespace esphome {
namespace cst226 {

static const char *const TAG = "cst226.touchscreen";

void CST226Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CST226 Touchscreen...");
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
    this->set_timeout(30, [this] { this->continue_setup_(); });
  } else {
    this->continue_setup_();
  }
}

void CST226Touchscreen::update_touches() {
  uint8_t data[28];
  if (!this->read_bytes(CST226_REG_STATUS, data, sizeof data)) {
    this->status_set_warning();
    this->skip_update_ = true;
    return;
  }
  this->status_clear_warning();
  if (data[0] == 0x83 && data[1] == 0x17 && data[5] == 0x80) {
    this->update_button_state_(true);
    return;
  }
  this->update_button_state_(false);
  if (data[6] != 0xAB || data[0] == 0xAB || data[5] == 0x80) {
    this->skip_update_ = true;
    return;
  }
  uint8_t num_of_touches = data[5] & 0x7F;
  if (num_of_touches == 0 || num_of_touches > 5) {
    this->write_byte(0, 0xAB);
    return;
  }

  size_t index = 0;
  for (uint8_t i = 0; i != num_of_touches; i++) {
    uint8_t id = data[index] >> 4;
    int16_t x = (data[index + 1] << 4) | ((data[index + 3] >> 4) & 0x0F);
    int16_t y = (data[index + 2] << 4) | (data[index + 3] & 0x0F);
    int16_t z = data[index + 4];
    this->add_raw_touch_position_(id, x, y, z);
    ESP_LOGV(TAG, "Read touch %d: %d/%d", id, x, y);
    index += 5;
    if (i == 0)
      index += 2;
  }
}

bool CST226Touchscreen::read16_(uint16_t addr, uint8_t *data, size_t len) {
  if (this->read_register16(addr, data, len) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Read data from 0x%04X failed", addr);
    this->mark_failed();
    return false;
  }
  return true;
}
void CST226Touchscreen::continue_setup_() {
  uint8_t buffer[8];
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }
  buffer[0] = 0xD1;
  if (this->write_register16(0xD1, buffer, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Write byte to 0xD1 failed");
    this->mark_failed();
    return;
  }
  delay(10);
  if (this->read16_(0xD204, buffer, 4)) {
    uint16_t chip_id = buffer[2] + (buffer[3] << 8);
    uint16_t project_id = buffer[0] + (buffer[1] << 8);
    ESP_LOGCONFIG(TAG, "Chip ID %X, project ID %x", chip_id, project_id);
  }
  if (this->x_raw_max_ == 0 || this->y_raw_max_ == 0) {
    if (this->read16_(0xD1F8, buffer, 4)) {
      this->x_raw_max_ = buffer[0] + (buffer[1] << 8);
      this->y_raw_max_ = buffer[2] + (buffer[3] << 8);
      if (this->swap_x_y_)
        std::swap(this->x_raw_max_, this->y_raw_max_);
    } else {
      this->x_raw_max_ = this->display_->get_native_width();
      this->y_raw_max_ = this->display_->get_native_height();
    }
  }
  this->setup_complete_ = true;
  ESP_LOGCONFIG(TAG, "CST226 Touchscreen setup complete");
}
void CST226Touchscreen::update_button_state_(bool state) {
  if (this->button_touched_ == state)
    return;
  this->button_touched_ = state;
  for (auto *listener : this->button_listeners_)
    listener->update_button(state);
}

void CST226Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CST226 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace cst226
}  // namespace esphome
