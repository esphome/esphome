#include "cst226_touchscreen.h"

namespace esphome::cst226 {

ESPHOME_LOG_TAG(TAG, "cst226.touchscreen");

void CST226Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    pulse_reset(this->reset_pin_, {.high_before_ms = 5, .low_ms = 5});
    this->set_timeout(30, [this] { this->continue_setup_(); });
  } else {
    this->continue_setup_();
  }
}

void CST226Touchscreen::update_touches() {
  if (!this->setup_complete_) {
    this->skip_update_ = true;
    return;
  }
  uint8_t data[28];
  if (!this->status_comm_result(this->read_bytes(CST226_REG_STATUS, data, sizeof data))) {
    this->skip_update_ = true;
    return;
  }
  if (data[0] == 0x83 && data[1] == 0x17 && data[5] == 0x80) {
    this->update_button_state_(true);
    return;
  }
  this->update_button_state_(false);
  if (data[cstxxx::SYNC_IDX] != cstxxx::SYNC_BYTE || data[0] == cstxxx::SYNC_BYTE ||
      data[cstxxx::FINGER_COUNT_IDX] == 0x80) {
    this->skip_update_ = true;
    return;
  }
  uint8_t num_of_touches = data[cstxxx::FINGER_COUNT_IDX] & 0x7F;
  if (num_of_touches == 0 || num_of_touches > 5) {
    this->write_byte(CST226_REG_STATUS, cstxxx::SYNC_BYTE);
    return;
  }

  for (uint8_t i = 0; i != num_of_touches; i++) {
    auto touch = cstxxx::decode_touch(data, i);
    this->add_raw_touch_position_(touch.id, touch.x, touch.y, touch.z);
    ESP_LOGV(TAG, "Read touch %d: %d/%d", touch.id, touch.x, touch.y);
  }
}

void CST226Touchscreen::continue_setup_() {
  this->setup_interrupt_pin_(gpio::INTERRUPT_FALLING_EDGE);
  if (!this->enter_command_mode_())
    return;
  delay(10);
  if (!this->check_firmware_())
    return;
  if (!this->setup_check(this->read_words_(cstxxx::REG_CHIP_INFO, this->project_id_, this->chip_id_),
                         LOG_STR("Failed to read chip ID")))
    return;
  if (this->x_raw_max_ == 0 || this->y_raw_max_ == 0) {
    uint16_t x_max, y_max;
    if (this->read_words_(cstxxx::REG_RESOLUTION, x_max, y_max)) {
      this->x_raw_max_ = x_max;
      this->y_raw_max_ = y_max;
      if (this->swap_x_y_)
        std::swap(this->x_raw_max_, this->y_raw_max_);
    } else {
      this->x_raw_max_ = this->display_->get_native_width();
      this->y_raw_max_ = this->display_->get_native_height();
    }
  }
  this->status_comm_check(this->write_register16(cstxxx::REG_NORMAL_MODE, nullptr, 0) == i2c::ERROR_OK);
  this->setup_complete_ = true;
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
  ESP_LOGCONFIG(TAG,
                "  Chip ID: 0x%04X, Project ID: 0x%04X\n"
                "  X/Y raw max: %d/%d",
                this->chip_id_, this->project_id_, this->x_raw_max_, this->y_raw_max_);
}

}  // namespace esphome::cst226
