#include "cst3240_touchscreen.hpp"

#include "esphome/core/log.h"

namespace esphome::cst3240 {

ESPHOME_LOG_TAG(TAG, "CST3240");

// public but not for users at runtime
void CST3240Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    this->inited_ = false;
    // async init time in next branch of esphome
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
    pulse_reset(this->reset_pin_, {.high_before_ms = 5, .low_ms = 5});
    this->set_timeout(30, [this] { this->continue_setup_(); });
#else
    this->reset_pin_->digital_write(false);
    delay(8);
    this->reset_pin_->digital_write(true);
    delay(22);
    this->continue_setup_();
#endif
  } else {
    this->continue_setup_();
  }
}

void CST3240Touchscreen::continue_setup_() {
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
    this->stop_poller();
    this->polling_ = false;
  }
  if (!this->check_init_())
    return;

  // Grab some infos from chip...
  if (!this->write_reg16_(0xD101))
    return;
  uint8_t buf[4] = {0};
  if (this->read_reg16_(0xD204, buf, sizeof(buf))) {
    this->chip_id_ = buf[2] + (buf[3] << 8);
    this->project_id_ = buf[0] + (buf[1] << 8);
    if (this->chip_id_ != 0x3240) {
      ESP_LOGW(TAG, "Wrong Chip ID %X!", this->chip_id_);
    }
    ESP_LOGD(TAG, "Chip ID %X, Project ID %X", this->chip_id_, this->project_id_);
  }
  if (this->read_reg16_(0xD208, buf, sizeof(buf))) {
    this->fw_ver_major_ = buf[3];
    this->fw_ver_minor_ = buf[2];
    this->fw_build_ = buf[0] + (buf[1] << 8);
    // wrote for ver. 1.0.1
    ESP_LOGI(TAG, "FW ver. %d.%d.%d", this->fw_ver_major_, this->fw_ver_minor_, this->fw_build_);
  }
  // Resolution may be set by ESPHome display, only fill if empty
  if ((this->x_raw_max_ == 0) && this->read_reg16_(0xD1F8, buf, sizeof(buf))) {
    this->x_raw_max_ = buf[0] + (buf[1] << 8);
    this->y_raw_max_ = buf[2] + (buf[3] << 8);
  }
  ESP_LOGD(TAG, "Resolution %d/%d", this->x_raw_max_, this->y_raw_max_);
  this->write_reg16_(0xD109);  // exit 0xD101 debug mode
}

void CST3240Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Touchscreen");
  LOG_I2C_DEVICE(this);
  if (this->reset_pin_ != nullptr)
    LOG_PIN("  TP_RST: ", this->reset_pin_);
  if (this->interrupt_pin_ != nullptr)
    LOG_PIN("  TP_INT: ", this->interrupt_pin_);
  ESP_LOGCONFIG(TAG, "  Chip ID: %04X, Project ID: %04X", this->chip_id_, this->project_id_);
  ESP_LOGCONFIG(TAG, "  FW version: %d.%d.%d", this->fw_ver_major_, this->fw_ver_minor_, this->fw_build_);
  ESP_LOGCONFIG(TAG, "  X/Y resolution: %d/%d", this->x_raw_max_, this->y_raw_max_);
}

bool CST3240Touchscreen::write_reg16_(uint16_t reg) {
  uint8_t data[2] = {uint8_t(reg >> 8), uint8_t(reg & 0xFF)};
  return (this->write(data, 2) == i2c::ERROR_OK);
}

bool CST3240Touchscreen::write_reg16_(uint16_t reg, uint8_t byte) {
  uint8_t data[3] = {uint8_t(reg >> 8), uint8_t(reg & 0xFF), byte};
  return (this->write(data, 3) == i2c::ERROR_OK);
}

bool CST3240Touchscreen::clear_reg16_(uint16_t reg) {
  uint8_t data[(MAX_TOUCH_POINTS * 7) + 2] = {0};
  data[0] = uint8_t(reg >> 8);
  data[1] = uint8_t(reg & 0xFF);
  return (this->write(data, sizeof(data)) == i2c::ERROR_OK);
}

bool CST3240Touchscreen::read_reg16_(uint16_t reg, uint8_t *buffer, size_t len) {
  uint8_t data[2] = {uint8_t(reg >> 8), uint8_t(reg & 0xFF)};
  if (this->write(data, 2) != i2c::ERROR_OK)
    return false;
  return this->read(buffer, len) == i2c::ERROR_OK;
}

bool CST3240Touchscreen::check_init_() {
  if (this->inited_)
    return true;
  this->inited_ = this->write_reg16_(0xD109);
  this->is_touched_ = false;
  return this->inited_;
}

void CST3240Touchscreen::update_touches() {
  if (!this->check_init_()) {
    this->skip_update_ = true;
    return;
  }

  uint8_t data[(MAX_TOUCH_POINTS * 7)] = {0};

  if (!this->read_reg16_(0xD000, data, sizeof(data))) {
    this->skip_update_ = true;
    ESP_LOGW(TAG, "I2C read error");
    return;
  }
  // this write could be skipped, but keep it (standby related?)
  this->write_reg16_(0xD000, 0xAB);

  for (uint8_t i = 0; i < MAX_TOUCH_POINTS; i++) {
    uint8_t offset = i * 7;
    touchscreen::TouchPoint tp;
    tp.id = data[offset + 5] & 0x0F;
    if (tp.id > MAX_TOUCH_POINTS || tp.id == 0)
      break;

    tp.x_raw = (data[offset + 1] << 4) | ((data[offset + 3] >> 4) & 0x0F);
    tp.y_raw = (data[offset + 2] << 4) | (data[offset + 3] & 0x0F);
    tp.z_raw = (data[offset + 4]);

    // bit 0x04 EVT_PRESS (else EVT_RELEASE)
    if (data[offset] & 0x04) {
      // will set is_touched_
      this->add_raw_touch_position_(tp.id, tp.x_raw, tp.y_raw, tp.z_raw);
      ESP_LOGVV(TAG, "%d: event at %d/%d z=%d", tp.id, tp.x_raw, tp.y_raw, tp.z_raw);
    } else {
      this->is_touched_ = false;
      ESP_LOGD(TAG, "%d: click at %d/%d z=%d", tp.id, tp.x_raw, tp.y_raw, tp.z_raw);
      // Clear CST3240 points buffer to avoid loops in Polling mode (without INT)
      if (this->polling_) {
        this->clear_reg16_(0xD000);
        break;
      }
    }
  }

}  // update_touches

}  // namespace esphome::cst3240
