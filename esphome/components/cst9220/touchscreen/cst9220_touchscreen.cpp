#include "cst9220_touchscreen.h"
#include "esphome/core/helpers.h"

#include <cinttypes>

namespace esphome::cst9220 {

void CST9220Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
  }
  // Wait for the controller to leave its bootloader before talking to it.
  this->set_timeout(30, [this] { this->continue_setup_(); });
}

void CST9220Touchscreen::continue_setup_() {
  uint8_t buffer[4];

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  // Enter command mode so the configuration registers can be read.
  if (this->write_register16(REG_CMD_MODE, buffer, 0) != i2c::ERROR_OK) {
    this->status_set_error(LOG_STR("Failed to enter command mode"));
    this->mark_failed();
    return;
  }
  delay(10);

  // The firmware check code confirms that valid firmware is loaded.
  if (this->read_register16(REG_CHECKCODE, buffer, 4) != i2c::ERROR_OK) {
    this->status_set_error(LOG_STR("Failed to read check code"));
    this->mark_failed();
    return;
  }
  uint32_t checkcode = encode_uint32(buffer[3], buffer[2], buffer[1], buffer[0]);
  if ((checkcode & 0xFFFF0000) != 0xCACA0000) {
    ESP_LOGE(TAG, "Invalid firmware check code: 0x%08" PRIX32, checkcode);
    this->status_set_error(LOG_STR("Invalid firmware check code"));
    this->mark_failed();
    return;
  }

  // Read the panel resolution unless the user supplied calibration values.
  if (this->read_register16(REG_RESOLUTION, buffer, 4) == i2c::ERROR_OK) {
    if (this->x_raw_max_ == this->x_raw_min_)
      this->x_raw_max_ = encode_uint16(buffer[1], buffer[0]);
    if (this->y_raw_max_ == this->y_raw_min_)
      this->y_raw_max_ = encode_uint16(buffer[3], buffer[2]);
  }

  // Read the chip type and project id and validate the controller.
  if (this->read_register16(REG_CHIP_INFO, buffer, 4) != i2c::ERROR_OK) {
    this->status_set_error(LOG_STR("Failed to read chip ID"));
    this->mark_failed();
    return;
  }
  this->chip_id_ = encode_uint16(buffer[3], buffer[2]);
  this->project_id_ = encode_uint16(buffer[1], buffer[0]);
  if (this->chip_id_ != CST9220_CHIP_ID && this->chip_id_ != CST9217_CHIP_ID) {
    ESP_LOGE(TAG, "Unknown chip ID: 0x%04X", this->chip_id_);
    this->status_set_error(LOG_STR("Unknown chip ID"));
    this->mark_failed();
    return;
  }

  // Fall back to the display dimensions if the resolution read failed.
  if (this->x_raw_max_ == this->x_raw_min_)
    this->x_raw_max_ = this->display_->get_native_width();
  if (this->y_raw_max_ == this->y_raw_min_)
    this->y_raw_max_ = this->display_->get_native_height();

  this->setup_complete_ = true;
}

void CST9220Touchscreen::update_touches() {
  if (!this->setup_complete_)
    return;
  uint8_t data[CST9220_DATA_LENGTH];
  // Only an actual I2C failure should skip the update; a successful read with no
  // touches is a real "all fingers lifted" state that must flow through so the
  // base class can generate the release event.
  if (this->read_register16(REG_TOUCH_DATA, data, sizeof(data)) != i2c::ERROR_OK) {
    this->status_set_warning();
    this->skip_update_ = true;
    return;
  }
  this->status_clear_warning();

  // Acknowledge the report so the controller can prepare the next one.
  uint8_t ack = TOUCH_ACK;
  this->write_register16(REG_TOUCH_DATA, &ack, 1);

  // A valid report carries the ACK marker at offset 6; offset 0 holds the first
  // point and must be neither the ACK marker nor empty. Anything else means no
  // valid touch data this cycle, which we report as zero touches (not a skip).
  if (data[0] == TOUCH_ACK || data[0] == 0x00 || data[6] != TOUCH_ACK)
    return;

  uint8_t num_touches = data[5] & 0x7F;
  if (num_touches > CST9220_MAX_TOUCHES)
    num_touches = CST9220_MAX_TOUCHES;

  for (uint8_t i = 0; i < num_touches; i++) {
    // The first point starts at offset 0; subsequent points are offset by the
    // two status bytes that follow it.
    const uint8_t *p = data + i * 5 + (i == 0 ? 0 : 2);
    uint8_t id = p[0] >> 4;
    uint8_t event = p[0] & 0x0F;
    if (event != TOUCH_EVENT_DOWN)
      continue;
    // p[3] is shared: high nibble holds the X LSBs, low nibble the Y LSBs.
    uint16_t x = (p[1] << 4) | (p[3] >> 4);
    uint16_t y = (p[2] << 4) | (p[3] & 0x0F);
    ESP_LOGV(TAG, "Read touch %d: %d/%d", id, x, y);
    this->add_raw_touch_position_(id, x, y);
  }
}

void CST9220Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG,
                "CST9220 Touchscreen:\n"
                "  Chip ID: 0x%04X\n"
                "  Project ID: 0x%04X\n"
                "  X Raw Min: %d, X Raw Max: %d\n"
                "  Y Raw Min: %d, Y Raw Max: %d",
                this->chip_id_, this->project_id_, this->x_raw_min_, this->x_raw_max_, this->y_raw_min_,
                this->y_raw_max_);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace esphome::cst9220
