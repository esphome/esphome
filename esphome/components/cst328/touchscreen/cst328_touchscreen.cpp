#include "cst328_touchscreen.h"

namespace esphome {
namespace cst328 {

static const char *const TAG = "cst328.touchscreen";

static const uint32_t CST328_TRANSITION_TIMEOUT = 300;  // 200 ms from datasheet, but typically much less
static const uint16_t CST328_FW_CRC = 0xCACA;           // Expected firmware CRC value
static const uint8_t CST328_SYNC_BYTE = 0xAB;           // Sync byte used in communication

void CST328Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CST328 Touchscreen...");
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_device_();
    this->set_timeout(2000 /*CST328_TRANSITION_TIMEOUT*/, [this] { this->continue_setup_(); });
  } else {
    this->continue_setup_();
  }
}

void CST328Touchscreen::reset_device_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(50);
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
  }
}

void CST328Touchscreen::continue_setup_() {
  ESP_LOGI(TAG, "Continuing CST328 setup...");
  uint8_t buf[24];

  ESP_LOGI(TAG, "write_touch_register_ CST_WM_DEBUG_INFO");
  // Enter debug/info mode
  if (i2c::ERROR_OK != this->write_touch_register_(CST_WM_DEBUG_INFO, buf, 0)) {
    ESP_LOGE(TAG, "Failed to enter debug/info mode");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "read16_ CST_REG_FW_CRC_AND_BOOT_TIME");
  if (i2c::ERROR_OK == this->read_touch_register_(CST_REG_FW_CRC_AND_BOOT_TIME, buf, 4)) {
    ESP_LOGI(TAG, "TouchPad_ID:0x%02x,0x%02x,0x%02x,0x%02x", buf[0], buf[1], buf[2], buf[3]);
  } else {
    ESP_LOGE(TAG, "Failed to read");
    return;
  }

  ESP_LOGI(TAG, "read16_ CST_REG_CHIP_TYPE_AND_PROJECT_ID");
  // Read chip and project ID
  if (i2c::ERROR_OK == this->read_touch_register_(CST_REG_CHIP_TYPE_AND_PROJECT_ID, buf, 4)) {
    this->chip_id_ = buf[2] + (buf[3] << 8);
    this->project_id_ = buf[0] + (buf[1] << 8);
    ESP_LOGV(TAG, "Chip ID %X, project ID %X", this->chip_id_, this->project_id_);
  } else {
    ESP_LOGE(TAG, "Failed to read");
    return;
  }
  ESP_LOGI(TAG, "read16_ CST_REG_FW_REVISION");

  // Read FW version
  if (i2c::ERROR_OK == this->read_touch_register_(CST_REG_FW_REVISION, buf, 4)) {
    this->fw_ver_major_ = buf[3];
    this->fw_ver_minor_ = buf[2];
    this->fw_build_ = buf[0] + (buf[1] << 8);
    ESP_LOGV(TAG, "FW version %d.%d.%d", this->fw_ver_major_, this->fw_ver_minor_, this->fw_build_);
  } else {
    ESP_LOGE(TAG, "Failed to read");
    return;
  }

  // Read X/Y resolution
  if (i2c::ERROR_OK == this->read_touch_register_(CST_REG_X_Y_RESOLUTION, buf, 4)) {
    this->x_raw_max_ = buf[0] + (buf[1] << 8);
    this->y_raw_max_ = buf[2] + (buf[3] << 8);
  } else {
    this->x_raw_max_ = this->display_->get_native_width();
    this->y_raw_max_ = this->display_->get_native_height();
  }

  // Enter normal mode
  this->write_touch_register_(CST_WM_NORMAL, buf, 0);

  // read once and sync?
  uint8_t sync_byte;
  this->read_touch_register_(CST_REG_TOUCH_INFORMATION, &sync_byte, 1);
  sync_byte = CST328_SYNC_BYTE;
  this->write_touch_register_(CST_REG_TOUCH_INFORMATION, &sync_byte, 1);

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
  if (this->button_touched_ == state)
    return;
  this->button_touched_ = state;
  for (auto *listener : this->button_listeners_)
    listener->update_button(state);
}

void CST328Touchscreen::update_touches() {
  if (!this->setup_complete_) {
    return;
  }
  const uint8_t clear_byte{0};
  const uint8_t sync_byte{CST328_SYNC_BYTE};
  uint8_t data[CST328_TOUCH_DATA_SIZE];
  uint8_t touch_cnt{0};

  this->status_clear_warning();
  this->skip_update_ = false;
  ESP_LOGI(TAG, "update_touches - read fingers");
  if (i2c::ERROR_OK != this->read_touch_register_(CST_REG_TOUCH_FINGER_NUMBER, data, 1)) {
    this->skip_update_ = true;
  } else {
    touch_cnt = data[0] & 0x0F;

    if (touch_cnt == 0 || touch_cnt > CST328_TOUCH_MAX_POINTS) {
      this->update_button_state_(false);
    } else {
      ESP_LOGVV(TAG, "%d touch(es)", touch_cnt);

      // Read Touch Points
      ESP_LOGI(TAG, "update_touches - read points");
      if (i2c::ERROR_OK == this->read_touch_register_(CST_REG_TOUCH_INFORMATION, data, sizeof(data)), true) {
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

  this->write_touch_register_(CST_REG_TOUCH_FINGER_NUMBER, &clear_byte, 1);
  this->write_touch_register_(CST_REG_TOUCH_INFORMATION, &sync_byte, 1);
}

i2c::ErrorCode CST328Touchscreen::write_touch_register_(uint16_t reg, const uint8_t *data, size_t len) {
  auto err = this->write_register16(reg, data, len);
  ESP_LOGI(TAG, "read_touch_register_1 reg: 0x%04X, len: %d, err: %d", reg, len, err);
  // reg = convert_big_endian(reg);
  // uint8_t reg_h = (uint8_t) (reg >> 8);
  // uint8_t reg_l = (uint8_t) (reg & 0xFF);
  // auto ret = i2c::ERROR_OK == this->write(&reg_h, 1);
  // if (ret) {
  //   ret |= i2c::ERROR_OK == this->write(&reg_l, 1);
  // }
  // if (ret) {
  //   ret |= i2c::ERROR_OK == this->write(data, len);
  // }
  return err;
}

i2c::ErrorCode CST328Touchscreen::read_touch_register_(uint16_t reg, uint8_t *data, size_t len) {
  auto err = this->read_register16(reg, data, len);
  ESP_LOGI(TAG, "read_touch_register_1 reg: 0x%04X, len: %d, err: %d", reg, len, err);

  // reg = convert_big_endian(reg);
  // i2c::ErrorCode err = this->write((uint8_t *) &reg, 2);
  // ESP_LOGI(TAG, "read_touch_register_1 reg: 0x%04X, len: %d, err: %d", reg, len, err);
  // if (err != i2c::ERROR_OK) {
  //   return err;
  // }
  // err = this->read(data, len);
  // ESP_LOGI(TAG, "read_touch_register_2 err: %d", err);

  // uint8_t reg_h = (uint8_t) (reg >> 8);
  // uint8_t reg_l = (uint8_t) (reg & 0xFF);

  // this->bus_->write_readv(this->address_, &reg_h, 1, nullptr, 0);
  // this->bus_->write_readv(this->address_, &reg_l, 1, nullptr, 0);

  return err;
}

}  // namespace cst328
}  // namespace esphome
