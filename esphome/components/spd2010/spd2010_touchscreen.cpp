// SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0
// Protocol adapted from esp_lcd_touch_spd2010 2.0.1 for ESPHome.
// https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch_spd2010

#include "spd2010_touchscreen.h"

#include "esphome/core/log.h"

namespace esphome::spd2010 {

static const char *const TAG = "spd2010.touchscreen";
static constexpr uint16_t REG_CLEAR_INTERRUPT = 0x0002;
static constexpr uint16_t REG_CPU_START = 0x0004;
static constexpr uint16_t REG_STATUS = 0x0020;
static constexpr uint16_t REG_VERSION = 0x0026;
static constexpr uint16_t REG_TOUCH_START = 0x0046;
static constexpr uint16_t REG_POINT_MODE = 0x0050;
static constexpr uint16_t REG_HDP_STATUS = 0x02FC;
static constexpr uint16_t REG_HDP = 0x0300;
static constexpr uint8_t STATUS_BIOS = 0x40;
static constexpr uint8_t STATUS_CPU = 0x20;
static constexpr uint8_t STATUS_RUNNING = 0x08;
static constexpr size_t MAX_REPORT_LENGTH = 4 + 10 * 6;

bool SPD2010Touchscreen::read_register_(uint16_t reg, uint8_t *data, size_t length) {
  const uint8_t command[] = {static_cast<uint8_t>(reg), static_cast<uint8_t>(reg >> 8)};
  if (this->write(command, sizeof(command)) != i2c::ERROR_OK)
    return false;
  // The controller needs a STOP and a delay before a commandless receive.
  delay_microseconds_safe(200);
  if (this->read(data, length) != i2c::ERROR_OK)
    return false;
  delay_microseconds_safe(200);
  return true;
}

bool SPD2010Touchscreen::write_command_(uint16_t reg, uint16_t value) {
  const uint8_t command[] = {static_cast<uint8_t>(reg), static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(value),
                             static_cast<uint8_t>(value >> 8)};
  if (this->write(command, sizeof(command)) != i2c::ERROR_OK)
    return false;
  delay_microseconds_safe(200);
  return true;
}

void SPD2010Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(false);
    this->set_timeout(2, [this]() {
      this->reset_pin_->digital_write(true);
      this->set_timeout(22, [this]() { this->initialize_(); });
    });
  } else {
    this->initialize_();
  }
}

bool SPD2010Touchscreen::start_controller_(uint8_t status) {
  if (status & STATUS_BIOS) {
    return this->write_command_(REG_CLEAR_INTERRUPT, 1) && this->write_command_(REG_CPU_START, 1);
  }
  if (status & STATUS_CPU) {
    return this->write_command_(REG_POINT_MODE, 0) && this->write_command_(REG_TOUCH_START, 0) &&
           this->write_command_(REG_CLEAR_INTERRUPT, 1);
  }
  return true;
}

void SPD2010Touchscreen::initialize_() {
  uint8_t status[4];
  if (this->read_register_(REG_STATUS, status, sizeof(status)) && this->start_controller_(status[1]) &&
      (status[1] & STATUS_RUNNING) && !(status[1] & (STATUS_BIOS | STATUS_CPU))) {
    uint8_t version[18];
    if (!this->read_register_(REG_VERSION, version, sizeof(version))) {
      ESP_LOGE(TAG, "Failed to read firmware version");
      this->mark_failed();
      return;
    }
    ESP_LOGD(TAG, "Firmware version: 0x%04X", encode_uint16(version[5], version[4]));
    this->ready_ = true;
    if (this->interrupt_pin_ != nullptr) {
      this->interrupt_pin_->setup();
      this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
    }
    // Service a report that may already have pulled the interrupt line low.
    this->store_.touched = true;
    return;
  }
  if (++this->setup_attempts_ >= 20) {
    ESP_LOGE(TAG, "Controller did not enter point mode");
    this->mark_failed();
    return;
  }
  this->set_timeout(50, [this]() { this->initialize_(); });
}

bool SPD2010Touchscreen::finish_report_() {
  // A corrupt status must not cause an unbounded drain loop or buffer overflow.
  for (uint8_t attempt = 0; attempt < 8; attempt++) {
    uint8_t status[8];
    if (!this->read_register_(REG_HDP_STATUS, status, sizeof(status)))
      return false;
    if (status[5] == 0x82)
      return this->write_command_(REG_CLEAR_INTERRUPT, 1);
    const uint16_t length = encode_uint16(status[3], status[2]);
    if (status[5] != 0 || length == 0 || length > MAX_REPORT_LENGTH)
      break;
    uint8_t remaining[MAX_REPORT_LENGTH];
    if (!this->read_register_(REG_HDP, remaining, length))
      return false;
  }
  ESP_LOGW(TAG, "Invalid or unfinished report");
  this->write_command_(REG_CLEAR_INTERRUPT, 1);
  return false;
}

bool SPD2010Touchscreen::read_data_() {
  uint8_t status[4];
  if (!this->read_register_(REG_STATUS, status, sizeof(status)))
    return false;
  if (status[1] & (STATUS_BIOS | STATUS_CPU)) {
    this->skip_update_ = true;
    // Recover if the controller restarts without producing another edge.
    this->set_timeout(50, [this]() { this->store_.touched = true; });
    return this->start_controller_(status[1]);
  }
  const uint16_t length = encode_uint16(status[3], status[2]);
  if ((status[1] & STATUS_RUNNING) && length == 0)
    return this->write_command_(REG_CLEAR_INTERRUPT, 1);
  if (!(status[0] & 0x03)) {
    this->skip_update_ = true;
    if ((status[1] & STATUS_RUNNING) && (status[0] & 0x08))
      return this->write_command_(REG_CLEAR_INTERRUPT, 1);
    return true;
  }
  if (length < 4 || length > MAX_REPORT_LENGTH) {
    ESP_LOGW(TAG, "Invalid report length: %u", length);
    this->write_command_(REG_CLEAR_INTERRUPT, 1);
    return false;
  }
  uint8_t report[MAX_REPORT_LENGTH];
  if (!this->read_register_(REG_HDP, report, length) || !this->finish_report_())
    return false;
  if (!(status[0] & 0x01)) {
    this->skip_update_ = true;
    return true;
  }
  if ((length - 4) % 6 != 0)
    return false;
  // Validate the whole frame before publishing any points.
  for (size_t offset = 4; offset < length; offset += 6) {
    if (report[offset] > 0x0A)
      return false;
  }
  for (size_t offset = 4; offset < length; offset += 6) {
    const uint8_t strength = report[offset + 4];
    if (strength == 0)
      continue;
    const uint16_t x = ((report[offset + 3] & 0xF0) << 4) | report[offset + 1];
    const uint16_t y = ((report[offset + 3] & 0x0F) << 8) | report[offset + 2];
    this->add_raw_touch_position_(report[offset], x, y, strength);
  }
  return true;
}

void SPD2010Touchscreen::update_touches() {
  if (!this->ready_) {
    this->skip_update_ = true;
    return;
  }
  if (!this->read_data_()) {
    this->skip_update_ = true;
    this->status_set_warning();
    this->set_timeout(50, [this]() { this->store_.touched = true; });
  } else {
    this->status_clear_warning();
  }
}

void SPD2010Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "SPD2010 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace esphome::spd2010
