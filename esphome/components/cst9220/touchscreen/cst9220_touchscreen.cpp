#include "cst9220_touchscreen.h"

namespace esphome::cst9220 {

// Id 0 is the base Touchscreen class touch timeout.
static constexpr uint32_t REREAD_TIMEOUT_ID = 1;
static constexpr uint32_t REREAD_DELAY_MS = 20;

void CST9220Touchscreen::setup() {
  pulse_reset(this->reset_pin_, {.high_before_ms = 5, .low_ms = 10});
  // Wait for the controller to leave its bootloader before talking to it.
  this->set_timeout(30, [this] { this->continue_setup_(); });
}

void CST9220Touchscreen::continue_setup_() {
  this->setup_interrupt_pin_(gpio::INTERRUPT_FALLING_EDGE);

  if (!this->enter_command_mode_())
    return;
  delay(10);
  if (!this->check_firmware_())
    return;

  // Read the panel resolution unless the user supplied calibration values.
  uint16_t x_max, y_max;
  if (this->read_words_(cstxxx::REG_RESOLUTION, x_max, y_max)) {
    if (this->x_raw_max_ == this->x_raw_min_)
      this->x_raw_max_ = x_max;
    if (this->y_raw_max_ == this->y_raw_min_)
      this->y_raw_max_ = y_max;
  }

  if (!this->setup_check(this->read_words_(cstxxx::REG_CHIP_INFO, this->project_id_, this->chip_id_),
                         LOG_STR("Failed to read chip ID")))
    return;
  if (this->chip_id_ != CST9220_CHIP_ID && this->chip_id_ != CST9217_CHIP_ID) {
    ESP_LOGE(TAG, "Unknown chip ID: 0x%04X", this->chip_id_);
    this->mark_failed(LOG_STR("Unknown chip ID"));
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
  if (!this->status_comm_result(this->read_register16(cstxxx::REG_TOUCH_DATA, data, sizeof(data)) == i2c::ERROR_OK)) {
    this->skip_update_ = true;
    return;
  }

  // A valid report carries the marker at offset 6. Writing an acknowledge back makes the controller return
  // invalid frames during a long press, so none is written. An invalid frame keeps the previous points; while
  // a finger is down, read again shortly so a lost release frame does not leave the touch held.
  if (data[cstxxx::SYNC_IDX] != cstxxx::SYNC_BYTE) {
    this->skip_update_ = true;
    if (!this->touches_.empty())
      this->set_timeout(REREAD_TIMEOUT_ID, REREAD_DELAY_MS, [this] { this->store_.touched = true; });
    return;
  }

  uint8_t num_touches = data[cstxxx::FINGER_COUNT_IDX] & 0x7F;
  if (num_touches > CST9220_MAX_TOUCHES)
    num_touches = CST9220_MAX_TOUCHES;

  for (uint8_t i = 0; i < num_touches; i++) {
    auto touch = cstxxx::decode_touch(data, i);
    if (touch.event != TOUCH_EVENT_DOWN)
      continue;
    ESP_LOGV(TAG, "Read touch %d: %d/%d", touch.id, touch.x, touch.y);
    this->add_raw_touch_position_(touch.id, touch.x, touch.y);
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
