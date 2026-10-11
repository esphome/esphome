// SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0
// Protocol adapted from esp_lcd_touch_spd2010 2.0.1 for ESPHome.
// https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch_spd2010

#include "spd2010_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::spd2010 {

ESPHOME_LOG_TAG(TAG, "spd2010.touchscreen");

static constexpr uint16_t REG_CLEAR_INTERRUPT = 0x0002;
static constexpr uint16_t REG_CPU_START = 0x0004;
static constexpr uint16_t REG_STATUS = 0x0020;
static constexpr uint16_t REG_VERSION = 0x0026;
static constexpr uint16_t REG_TOUCH_START = 0x0046;
static constexpr uint16_t REG_POINT_MODE = 0x0050;
static constexpr uint16_t REG_HDP_STATUS = 0x02FC;
static constexpr uint16_t REG_HDP = 0x0300;

// Low status byte
static constexpr uint8_t STATUS_POINT = 0x01;
static constexpr uint8_t STATUS_GESTURE = 0x02;
static constexpr uint8_t STATUS_AUX = 0x08;
// High status byte
static constexpr uint8_t STATUS_BIOS = 0x40;
static constexpr uint8_t STATUS_CPU = 0x20;
static constexpr uint8_t STATUS_RUNNING = 0x08;

static constexpr uint8_t HDP_STATUS_DONE = 0x82;
static constexpr uint8_t HDP_STATUS_MORE = 0x00;

static constexpr uint8_t MAX_CONTACT_ID = 0x0A;
static constexpr uint8_t GESTURE_REPORT_ID = 0xF6;
static constexpr size_t REPORT_HEADER_LENGTH = 4;
static constexpr size_t CONTACT_LENGTH = 6;
static constexpr size_t MAX_REPORT_LENGTH = REPORT_HEADER_LENGTH + 10 * CONTACT_LENGTH;
static constexpr uint8_t MAX_DRAIN_READS = 8;

static constexpr uint32_t RESET_PULSE_WIDTH =
    20;  // longer than the datasheet says, but the controller seems to need it
static constexpr uint32_t RESET_DELAY = 20;

bool SPD2010Touchscreen::read_register_(uint16_t reg, uint8_t *data, size_t length) const {
  const uint8_t command[] = {static_cast<uint8_t>(reg), static_cast<uint8_t>(reg >> 8u)};
  if (this->write(command, sizeof(command)) != i2c::ERROR_OK)
    return false;
  // The controller needs a STOP and a delay before a commandless receive.
  delay_microseconds_safe(200);
  return this->read(data, length) == i2c::ERROR_OK;
}

bool SPD2010Touchscreen::write_command_(uint16_t reg, uint16_t value) const {
  const uint8_t command[]{
      static_cast<uint8_t>(reg),
      static_cast<uint8_t>(reg >> 8u),
      static_cast<uint8_t>(value),
      static_cast<uint8_t>(value >> 8u),
  };
  return this->write(command, sizeof(command)) == i2c::ERROR_OK;
}

bool SPD2010Touchscreen::clear_interrupt_() const { return this->write_command_(REG_CLEAR_INTERRUPT, 1); }

void SPD2010Touchscreen::setup() {
  if (this->x_raw_max_ == this->x_raw_min_)
    this->x_raw_max_ = this->display_->get_native_width() - 1;
  if (this->y_raw_max_ == this->y_raw_min_)
    this->y_raw_max_ = this->display_->get_native_height() - 1;
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(false);
    this->set_timeout(RESET_PULSE_WIDTH, [this] {
      this->reset_pin_->digital_write(true);
      this->set_timeout(RESET_DELAY, [this] { this->initialize_(); });
    });
  } else {
    this->initialize_();
  }
}

void SPD2010Touchscreen::initialize_() {
  uint8_t version[18];
  if (!this->read_register_(REG_VERSION, version, sizeof(version))) {
    this->mark_failed(LOG_STR("Failed to read firmware version"));
    return;
  }
  ESP_LOGD(TAG, "Firmware version: 0x%04X", encode_uint16(version[5], version[4]));
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }
  this->ready_ = true;
}

bool SPD2010Touchscreen::finish_report_() {
  // A corrupt status must not cause an unbounded drain loop or buffer overflow.
  for (uint8_t attempt = 0; attempt != MAX_DRAIN_READS; attempt++) {
    uint8_t status[8];
    if (!this->read_register_(REG_HDP_STATUS, status, sizeof(status)))
      return false;
    if (status[5] == HDP_STATUS_DONE)
      return this->clear_interrupt_();
    const uint16_t length = encode_uint16(status[3], status[2]);
    if (status[5] != HDP_STATUS_MORE || length == 0 || length > MAX_REPORT_LENGTH)
      break;
    uint8_t remaining[MAX_REPORT_LENGTH];
    if (!this->read_register_(REG_HDP, remaining, length))
      return false;
  }
  ESP_LOGW(TAG, "Invalid or unfinished report");
  (void) this->clear_interrupt_();
  return false;
}

// Returns true only when touch data was read correctly and should be published.
bool SPD2010Touchscreen::read_data_() {
  uint8_t status[4];
  if (!this->read_register_(REG_STATUS, status, sizeof(status))) {
    this->status_set_warning(LOG_STR("Failed to read status"));
    return false;
  }
  const uint8_t status_low = status[0];
  const uint8_t status_high = status[1];
  const uint16_t length = encode_uint16(status[3], status[2]);

  if (status_high & STATUS_BIOS) {
    this->status_set_warning(LOG_STR("BIOS status; start CPU"));
    (void) (this->clear_interrupt_() && this->write_command_(REG_CPU_START, 1));
    return false;
  }
  if (status_high & STATUS_CPU) {
    this->status_set_warning(LOG_STR("CPU status; stop point mode, start touch"));
    (void) (this->write_command_(REG_POINT_MODE, 0) && this->write_command_(REG_TOUCH_START, 0) &&
            this->clear_interrupt_());
    return false;
  }
  if ((status_high & STATUS_RUNNING) && length == 0) {
    if (!this->clear_interrupt_()) {
      this->status_set_warning(LOG_STR("Failed to clear interrupt"));
      return false;
    }
    this->status_clear_warning();
    return true;
  }
  if (!(status_low & (STATUS_POINT | STATUS_GESTURE))) {
    if ((status_high & STATUS_RUNNING) && (status_low & STATUS_AUX) && !this->clear_interrupt_()) {
      this->status_set_warning(LOG_STR("Failed to clear interrupt"));
      return false;
    }
    this->status_clear_warning();
    return false;
  }
  if (length < REPORT_HEADER_LENGTH || length > MAX_REPORT_LENGTH ||
      (length - REPORT_HEADER_LENGTH) % CONTACT_LENGTH != 0) {
    ESP_LOGW(TAG, "Invalid report length: %u", length);
    this->status_set_warning(LOG_STR("Invalid report length"));
    (void) this->clear_interrupt_();
    return false;
  }
  uint8_t report[MAX_REPORT_LENGTH];
  if (!this->read_register_(REG_HDP, report, length) || !this->finish_report_()) {
    this->status_set_warning(LOG_STR("Failed to read report"));
    return false;
  }
  // Gesture reports are not used.
  if (!(status_low & STATUS_POINT) ||
      (length > REPORT_HEADER_LENGTH && report[REPORT_HEADER_LENGTH] == GESTURE_REPORT_ID)) {
    this->status_clear_warning();
    return false;
  }
  // Check every ID before adding any point, so a bad report leaves the stored touches unchanged.
  for (size_t offset = REPORT_HEADER_LENGTH; offset < length; offset += CONTACT_LENGTH) {
    if (report[offset] > MAX_CONTACT_ID) {
      this->status_set_warning(LOG_STR("Invalid contact ID"));
      return false;
    }
  }
  for (size_t offset = REPORT_HEADER_LENGTH; offset < length; offset += CONTACT_LENGTH) {
    const uint8_t *contact = report + offset;
    // Zero strength marks a released contact.
    if (contact[4] == 0)
      continue;
    const uint16_t x = ((contact[3] & 0xF0u) << 4u) | contact[1];
    const uint16_t y = ((contact[3] & 0x0Fu) << 8u) | contact[2];
    this->add_raw_touch_position_(contact[0], x, y, contact[4]);
  }
  this->status_clear_warning();
  return true;
}

void SPD2010Touchscreen::update_touches() {
  if (!this->ready_ || !this->read_data_())
    this->skip_update_ = true;
}

void SPD2010Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SPD2010 Touchscreen:\n"
                "  Raw X range: %d-%d\n"
                "  Raw Y range: %d-%d",
                this->x_raw_min_, this->x_raw_max_, this->y_raw_min_, this->y_raw_max_);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace esphome::spd2010
