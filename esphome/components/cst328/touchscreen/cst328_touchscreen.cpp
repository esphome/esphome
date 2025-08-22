#include "cst328_touchscreen.h"

namespace esphome {
namespace cst328 {

static const char *const TAG = "cst328.touchscreen";

static const uint32_t CST328_TRANSITION_TIMEOUT = 50;  // 200 ms from datasheet, but typically much less
static const uint16_t CST328_FW_CRC = 0xCACA;          // Expected firmware CRC value
static const uint8_t CST328_SYNC_BYTE = 0xAB;          // Sync byte used in communication

void CST328Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CST328 Touchscreen...");
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(50);
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
    this->set_timeout(CST328_TRANSITION_TIMEOUT, [this] { this->continue_setup_(); });
  } else {
    this->continue_setup_();
  }
}

bool CST328Touchscreen::read16_(uint16_t addr, uint8_t *data, size_t len, bool ok_to_fail) {
  if (i2c::ERROR_OK != this->read_register16(addr, data, len)) {
    if (!ok_to_fail) {
      ESP_LOGE(TAG, "Read data from 0x%04X failed", addr);
      this->mark_failed();
    }
    return false;
  }
  return true;
}

void CST328Touchscreen::continue_setup_() {
  uint8_t buffer[4];

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  // Enter debug/info mode
  if (i2c::ERROR_OK != this->write_register16(CST_WM_DEBUG_INFO, buffer, 0)) {
    ESP_LOGE(TAG, "Failed to enter debug/info mode");
    this->mark_failed();
    return;
  }

  // Read FW checksum
  if (this->read16_(CST_REG_FW_CRC_AND_BOOT_TIME, buffer, 4)) {
    uint16_t fw_crc = buffer[2] + (buffer[3] << 8);
    if (CST328_FW_CRC != fw_crc) {
      ESP_LOGE(TAG, "Invalid FW checksum: %X", fw_crc);
      this->mark_failed();
      return;
    }
  } else {
    return;
  }

  // Read chip and project ID
  if (this->read16_(CST_REG_CHIP_TYPE_AND_PROJECT_ID, buffer, 4)) {
    this->chip_id_ = buffer[2] + (buffer[3] << 8);
    this->project_id_ = buffer[0] + (buffer[1] << 8);
    ESP_LOGV(TAG, "Chip ID %X, project ID %X", this->chip_id_, this->project_id_);
  } else {
    return;
  }

  // Read FW version
  if (this->read16_(CST_REG_FW_REVISION, buffer, 4)) {
    this->fw_ver_major_ = buffer[3];
    this->fw_ver_minor_ = buffer[2];
    this->fw_build_ = buffer[0] + (buffer[1] << 8);
    ESP_LOGV(TAG, "FW version %d.%d.%d", this->fw_ver_major_, this->fw_ver_minor_, this->fw_build_);
  } else {
    return;
  }

  // Read X/Y resolution
  if (this->read16_(CST_REG_X_Y_RESOLUTION, buffer, 4)) {
    this->x_raw_max_ = buffer[0] + (buffer[1] << 8);
    this->y_raw_max_ = buffer[2] + (buffer[3] << 8);
  } else {
    this->x_raw_max_ = this->display_->get_native_width();
    this->y_raw_max_ = this->display_->get_native_height();
  }

  // Enter normal mode
  this->write_register16(CST_WM_NORMAL, buffer, 0);

  // read once and sync?
  uint8_t sync_byte;
  this->read_register16(CST_REG_TOUCH_INFORMATION, &sync_byte, 1);
  sync_byte = CST328_SYNC_BYTE;
  this->write_register16(CST_REG_TOUCH_INFORMATION, &sync_byte, 1);

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
  if (this->button_touched_ == state)
    return;
  this->button_touched_ = state;
  for (auto *listener : this->button_listeners_)
    listener->update_button(state);
}

void CST328Touchscreen::update_touches() {
  const uint8_t clear_byte{0};
  const uint8_t sync_byte{CST328_SYNC_BYTE};
  uint8_t data[CST328_TOUCH_DATA_SIZE];
  uint8_t touch_cnt{0};

  this->status_clear_warning();
  this->skip_update_ = false;

  if (!this->read16_(CST_REG_TOUCH_FINGER_NUMBER, data, 1, true)) {
    this->skip_update_ = true;
  } else {
    touch_cnt = data[0] & 0x0F;

    if (touch_cnt == 0 || touch_cnt > CST328_TOUCH_MAX_POINTS) {
      this->update_button_state_(false);
    } else {
      ESP_LOGVV(TAG, "%d touch(es)", touch_cnt);

      // Read Touch Points
      if (this->read16_(CST_REG_TOUCH_INFORMATION, data, sizeof(data)), true) {
        size_t index = 0;
        for (uint8_t i = 0; i != touch_cnt; i++) {
          uint8_t id = data[index] >> 4;
          uint8_t status = (data[index] & 0x0F) >> 1;
          int16_t x = (data[index + 1] << 4) | ((data[index + 3] >> 4) & 0x0F);
          int16_t y = (data[index + 2] << 4) | (data[index + 3] & 0x0F);
          int16_t z = data[index + 4];

          this->add_raw_touch_position_(id, x, y, z);

          // first touch data block is 7 bytes, others are 5
          index += 5;
          if (i == 0) {
            index += 2;
          }
        }
      }
    }
  }

  this->write_register16(CST_REG_TOUCH_FINGER_NUMBER, &clear_byte, 1);
  this->write_register16(CST_REG_TOUCH_INFORMATION, &sync_byte, 1);
}
}  // namespace cst328
}  // namespace esphome
