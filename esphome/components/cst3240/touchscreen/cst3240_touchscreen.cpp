#include "cst3240_touchscreen.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <utility>

namespace esphome::cst3240 {

ESPHOME_LOG_TAG(TAG, "cst3240.touchscreen");

void CST3240Touchscreen::setup() {
  if (this->reset_pin_ == nullptr) {
    this->continue_setup_();
    return;
  }
  pulse_reset(this->reset_pin_, {.high_before_ms = 5, .low_ms = 5});
  this->set_timeout(30, [this] { this->continue_setup_(); });
}

void CST3240Touchscreen::continue_setup_() {
  if (!this->enter_command_mode_())
    return;
  if (!this->setup_check(this->read_words_(cstxxx::REG_CHIP_INFO, this->project_id_, this->chip_id_),
                         LOG_STR("Failed to read chip ID")))
    return;
  // Only warn: the chip ID has not been confirmed on enough boards to fail setup on it
  if (this->chip_id_ != CST3240_CHIP_ID) {
    ESP_LOGW(TAG, "Unexpected chip ID 0x%04X", this->chip_id_);
  }
  this->read_words_(cstxxx::REG_FW_VERSION, this->fw_build_, this->fw_version_);

  if (this->x_raw_max_ == this->x_raw_min_ || this->y_raw_max_ == this->y_raw_min_) {
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
  this->setup_interrupt_pin_(gpio::INTERRUPT_RISING_EDGE);
  this->setup_complete_ = true;
}

void CST3240Touchscreen::update_touches() {
  if (!this->setup_complete_) {
    this->skip_update_ = true;
    return;
  }
  uint8_t data[CST3240_REPORT_SIZE];
  if (!this->status_comm_result(this->read_register16(cstxxx::REG_TOUCH_DATA, data, sizeof(data)) == i2c::ERROR_OK)) {
    this->skip_update_ = true;
    return;
  }

  const uint8_t sync = data[cstxxx::SYNC_IDX];
  if (sync != cstxxx::SYNC_BYTE) {
    this->skip_update_ = true;
    if (sync != 0) {
      ESP_LOGV(TAG, "Skipping report, sync byte 0x%02X", sync);
      this->clear_report_();
    }
    return;
  }

  const uint8_t count = data[cstxxx::FINGER_COUNT_IDX] & 0x0F;
  if (count <= CST3240_MAX_TOUCHES) {
    for (uint8_t i = 0; i < count; i++) {
      auto touch = cstxxx::decode_touch(data, i);
      // A released finger is still listed once, with both press bits clear
      if ((touch.event & TOUCH_EVENT_PRESSED) == 0)
        continue;
      this->add_raw_touch_position_(touch.id, touch.x, touch.y, touch.z);
    }
  }

  if (this->interrupt_pin_ == nullptr) {
    this->clear_report_();
  } else if (!this->write_byte_reg16(cstxxx::REG_FINGER_COUNT, 0) ||
             !this->write_byte_reg16(cstxxx::REG_TOUCH_DATA, cstxxx::SYNC_BYTE)) {
    ESP_LOGW(TAG, "Failed to acknowledge the report");
  }
}

void CST3240Touchscreen::clear_report_() {
  // Built on the stack: write_register16() heap allocates a frame this long
  uint8_t frame[2 + CST3240_REPORT_SIZE]{};
  put_be16(frame, cstxxx::REG_TOUCH_DATA);
  if (this->write(frame, sizeof(frame)) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to clear the report");
  }
}

void CST3240Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG,
                "CST3240 Touchscreen:\n"
                "  Chip ID: 0x%04X, Project ID: 0x%04X\n"
                "  FW version: %d.%d.%d\n"
                "  X/Y resolution: %d/%d",
                this->chip_id_, this->project_id_, this->fw_version_ >> 8, this->fw_version_ & 0xFF, this->fw_build_,
                this->x_raw_max_, this->y_raw_max_);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace esphome::cst3240
