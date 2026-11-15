#include "cst328_touchscreen.h"
#include "esphome/core/log.h"

namespace esphome::cst328 {

ESPHOME_LOG_TAG(TAG, "cst328.touchscreen");

static constexpr uint32_t CST328_BEFORE_RESET_TIMEOUT = 50;  // 50 ms from datasheet
static constexpr uint32_t CST328_TRANSITION_TIMEOUT = 300;   // 200 ms from datasheet, but typically much less

void CST328Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CST328 Touchscreen...");
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    this->set_timeout(CST328_BEFORE_RESET_TIMEOUT, [this] { this->reset_device_(); });
  } else {
    this->continue_setup_();
  }
}

void CST328Touchscreen::reset_device_() {
  pulse_reset(this->reset_pin_, {.low_ms = 5});
  this->set_timeout(CST328_TRANSITION_TIMEOUT, [this] { this->continue_setup_(); });
}

void CST328Touchscreen::continue_setup_() {
  ESP_LOGV(TAG, "Continuing CST328 setup...");

  if (!this->enter_command_mode_() || !this->check_firmware_())
    return;
  if (!this->setup_check(this->read_words_(cstxxx::REG_CHIP_INFO, this->project_id_, this->chip_id_),
                         LOG_STR("Failed to read chip and project ID")))
    return;
  ESP_LOGD(TAG, "Chip ID %X, project ID %X", this->chip_id_, this->project_id_);
  uint16_t fw_version;
  if (!this->setup_check(this->read_words_(cstxxx::REG_FW_VERSION, this->fw_build_, fw_version),
                         LOG_STR("Failed to read FW version")))
    return;
  this->fw_ver_major_ = fw_version >> 8;
  this->fw_ver_minor_ = fw_version & 0xFF;
  ESP_LOGV(TAG, "FW version %d.%d.%d", this->fw_ver_major_, this->fw_ver_minor_, this->fw_build_);

  uint16_t x_max, y_max;
  if (this->read_words_(cstxxx::REG_RESOLUTION, x_max, y_max)) {
    this->x_raw_max_ = x_max;
    this->y_raw_max_ = y_max;
  } else {
    this->x_raw_max_ = this->display_->get_native_width();
    this->y_raw_max_ = this->display_->get_native_height();
  }

  uint8_t sync;
  bool ok = this->write_register16(cstxxx::REG_NORMAL_MODE, nullptr, 0) == i2c::ERROR_OK;
  ok &= this->read_byte_reg16(cstxxx::REG_TOUCH_DATA, &sync);
  ok &= this->write_byte_reg16(cstxxx::REG_TOUCH_DATA, cstxxx::SYNC_BYTE);
  this->status_comm_check(ok);

  this->setup_interrupt_pin_(gpio::INTERRUPT_FALLING_EDGE);

  this->setup_complete_ = true;
  ESP_LOGV(TAG, "CST328 setup complete");
}

void CST328Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CST328 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Chip ID: 0x%04X, Project ID: 0x%04X", this->chip_id_, this->project_id_);
  ESP_LOGCONFIG(TAG, "  FW version: %d.%d.%d", this->fw_ver_major_, this->fw_ver_minor_, this->fw_build_);
  ESP_LOGCONFIG(TAG, "  X/Y resolution: %d/%d", this->x_raw_max_, this->y_raw_max_);
}

void CST328Touchscreen::update_button_state_(bool state) {
  if (this->button_touched_ == state) {
    return;
  }
  this->button_touched_ = state;
  for (auto *listener : this->button_listeners_) {
    listener->update_button(state);
  }
}

void CST328Touchscreen::update_touches() {
  if (!this->setup_complete_) {
    this->skip_update_ = true;
    return;
  }

  uint8_t touch_data[CST328_TOUCH_DATA_SIZE];

  this->status_clear_warning();

  if (i2c::ERROR_OK != this->read_register16(CST_REG_TOUCH_INFORMATION, touch_data, CST328_TOUCH_DATA_SIZE)) {
    ESP_LOGW(TAG, "Failed to read touch data");
    this->status_set_warning();
    this->skip_update_ = true;
    return;
  }

  uint8_t touch_cnt = touch_data[CST_REG_FINGER_COUNT_IDX] & 0x0F;
  if (touch_cnt == 0 || touch_cnt > CST328_TOUCH_MAX_POINTS) {
    this->update_button_state_(false);
  } else {
    this->update_button_state_(true);

    for (uint8_t i = 0; i < touch_cnt; i++) {
      auto touch = cstxxx::decode_touch(touch_data, i);
      this->add_raw_touch_position_(touch.id, touch.x, touch.y, touch.z);
    }
  }

  bool cleanup_error = false;
  cleanup_error |= !this->write_byte_reg16(cstxxx::REG_FINGER_COUNT, 0);
  cleanup_error |= !this->write_byte_reg16(cstxxx::REG_TOUCH_DATA, cstxxx::SYNC_BYTE);

  if (cleanup_error) {
    ESP_LOGW(TAG, "Failed to clean up touch registers");
  }
}

}  // namespace esphome::cst328
