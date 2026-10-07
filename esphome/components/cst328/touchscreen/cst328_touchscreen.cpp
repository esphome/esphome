#include "cst328_touchscreen.h"
#include "esphome/core/log.h"

namespace esphome::cst328 {

static const char *const TAG = "cst328.touchscreen";

static const uint32_t CST328_BEFORE_RESET_TIMEOUT = 50;  // 50 ms from datasheet
static const uint32_t CST328_TRANSITION_TIMEOUT = 300;   // 200 ms from datasheet, but typically much less
static const uint16_t CST328_FW_CRC = 0xCACA;            // Expected firmware CRC value
static const uint8_t CST328_SYNC_BYTE = 0xAB;            // Sync byte used in communication

static const uint8_t ZERO_BYTE = 0;

#define I2C_WARN_ON_ERROR(x, log_tag, format, ...) \
  do { \
    i2c::ErrorCode err_rc_ = (x); \
    if (err_rc_ != i2c::ERROR_OK) { \
      ESP_LOGW(log_tag, "%s(%d): [error %d] " format, __FUNCTION__, __LINE__, err_rc_, ##__VA_ARGS__); \
      this->status_set_warning(format); \
    } \
  } while (0)

#define I2C_FAIL_ON_ERROR(x, log_tag, format, ...) \
  do { \
    i2c::ErrorCode err_rc_ = (x); \
    if (err_rc_ != i2c::ERROR_OK) { \
      ESP_LOGE(log_tag, "%s(%d): [error %d] " format, __FUNCTION__, __LINE__, err_rc_, ##__VA_ARGS__); \
      this->mark_failed(); \
      return; \
    } \
  } while (0)

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
  this->reset_pin_->digital_write(false);
  delay(5);
  this->reset_pin_->digital_write(true);
  this->set_timeout(CST328_TRANSITION_TIMEOUT, [this] { this->continue_setup_(); });
}

void CST328Touchscreen::continue_setup_() {
  ESP_LOGV(TAG, "Continuing CST328 setup...");

  uint8_t data_byte{0};
  uint8_t buf[24]{};

  I2C_FAIL_ON_ERROR(this->write_register16(CST_WM_DEBUG_INFO, buf, 0), TAG, "Failed to enter debug/info mode");
  I2C_FAIL_ON_ERROR(this->read_register16(CST_REG_FW_CRC_AND_BOOT_TIME, buf, 4), TAG,
                    "Failed to read FW CRC and boot time");

  uint16_t fw_crc = buf[2] + (buf[3] << 8);
  if (fw_crc != CST328_FW_CRC) {
    ESP_LOGE(TAG, "Error: Firmware CRC mismatch, expected 0x%04X but got 0x%04X", CST328_FW_CRC, fw_crc);
    this->mark_failed();
    return;
  }

  I2C_FAIL_ON_ERROR(this->read_register16(CST_REG_CHIP_TYPE_AND_PROJECT_ID, buf, 4), TAG,
                    "Failed to read chip and project ID");

  this->chip_id_ = buf[2] + (buf[3] << 8);
  this->project_id_ = buf[0] + (buf[1] << 8);
  ESP_LOGD(TAG, "Chip ID %X, project ID %X", this->chip_id_, this->project_id_);
  I2C_FAIL_ON_ERROR(this->read_register16(CST_REG_FW_REVISION, buf, 4), TAG, "Failed to read FW version");

  this->fw_ver_major_ = buf[3];
  this->fw_ver_minor_ = buf[2];
  this->fw_build_ = buf[0] + (buf[1] << 8);
  ESP_LOGV(TAG, "FW version %d.%d.%d", this->fw_ver_major_, this->fw_ver_minor_, this->fw_build_);

  if (i2c::ERROR_OK == this->read_register16(CST_REG_X_Y_RESOLUTION, buf, 4)) {
    this->x_raw_max_ = buf[0] + (buf[1] << 8);
    this->y_raw_max_ = buf[2] + (buf[3] << 8);
  } else {
    this->x_raw_max_ = this->display_->get_native_width();
    this->y_raw_max_ = this->display_->get_native_height();
  }

  I2C_WARN_ON_ERROR(this->write_register16(CST_WM_NORMAL, buf, 0), TAG, "Failed to enter normal mode");
  I2C_WARN_ON_ERROR(this->read_register16(CST_REG_TOUCH_INFORMATION, &data_byte, 1), TAG, "Failed to read sync");
  I2C_WARN_ON_ERROR(this->write_register16(CST_REG_TOUCH_INFORMATION, &CST328_SYNC_BYTE, 1), TAG,
                    "Failed to write sync");

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

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

    uint8_t data_idx = 0;
    for (uint8_t i = 0; i < touch_cnt; i++) {
      uint8_t id = touch_data[data_idx] >> 4;
      int16_t x = (touch_data[data_idx + 1] << 4) | ((touch_data[data_idx + 3] >> 4) & 0x0F);
      int16_t y = (touch_data[data_idx + 2] << 4) | (touch_data[data_idx + 3] & 0x0F);
      int16_t z = touch_data[data_idx + 4];

      this->add_raw_touch_position_(id, x, y, z);
      data_idx += (i == 0) ? 7 : 5;
    }
  }

  bool cleanup_error = false;
  cleanup_error |= (i2c::ERROR_OK != this->write_register16(CST_REG_TOUCH_FINGER_NUMBER, &ZERO_BYTE, 1));
  cleanup_error |= (i2c::ERROR_OK != this->write_register16(CST_REG_TOUCH_INFORMATION, &CST328_SYNC_BYTE, 1));

  if (cleanup_error) {
    ESP_LOGW(TAG, "Failed to clean up touch registers");
  }
}

}  // namespace esphome::cst328
