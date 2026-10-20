#include "ssd1306_i2c.h"

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
#include <cstring>
#include "esphome/core/application.h"
#endif

#include "esphome/core/log.h"

namespace esphome::ssd1306_i2c {

ESPHOME_LOG_TAG(TAG, "ssd1306_i2c");

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
static constexpr uint32_t RECOVERY_RETRY_INTERVAL_MS = 1000;
#endif

void I2CSSD1306::setup() {
  this->init_reset_();

  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Reserve the required framebuffer before optional state, but track faults before sending init commands.
  if (!this->init_framebuffer_()) {
    return;
  }
#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  if (this->partial_updates_enabled_) {
    this->partial_update_state_ =
        RAMAllocator<PartialUpdateState>(RAMAllocator<PartialUpdateState>::PREFER_INTERNAL).make_unique();
    if (this->partial_update_state_ != nullptr) {
      this->partial_update_state_->previous_buffer = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::PREFER_INTERNAL)
                                                         .make_unique_array_for_overwrite(this->get_buffer_length_());
    }
    if (this->partial_update_state_ == nullptr || this->partial_update_state_->previous_buffer == nullptr) {
      ESP_LOGW(TAG, "Failed to allocate partial-update state; falling back to full-frame updates");
      this->partial_update_state_.reset();
      this->partial_updates_enabled_ = false;
    }
  }
  if (this->partial_update_state_ != nullptr) {
    this->partial_update_state_->initializing_controller = true;
  }
#endif
  SSD1306::setup();
#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  if (this->partial_update_state_ != nullptr) {
    this->partial_update_state_->initializing_controller = false;
  }
  this->disable_loop();
#endif
}

void I2CSSD1306::dump_config() {
  LOG_DISPLAY("", "I2C SSD1306", this);
  ESP_LOGCONFIG(TAG,
                "  Model: %s\n"
                "  External VCC: %s\n"
                "  Flip X: %s\n"
                "  Flip Y: %s\n"
                "  Offset X: %d\n"
                "  Offset Y: %d\n"
                "  Inverted Color: %s",
                LOG_STR_ARG(this->model_str_()), YESNO(this->external_vcc_), YESNO(this->flip_x_), YESNO(this->flip_y_),
                this->offset_x_, this->offset_y_, YESNO(this->invert_));
#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  ESP_LOGCONFIG(TAG, "  Partial Updates: %s", YESNO(this->partial_updates_enabled_));
#endif
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void I2CSSD1306::command(uint8_t value) {
  if (this->is_failed()) {
    return;
  }
#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  if (this->partial_updates_enabled_ && this->partial_update_state_ != nullptr) {
    auto &state = *this->partial_update_state_;
    if (state.reset_phase != ResetPhase::RESET_PHASE_IDLE || (state.initializing_controller && state.command_failed)) {
      return;
    }
    if (!this->write_byte(0x00, value)) {
      state.command_failed = true;
      this->mark_partial_transport_failure_(true);
    }
    return;
  }
#endif
  this->write_byte(0x00, value);
}

void HOT I2CSSD1306::write_display_data() {
  if (this->is_failed() || this->buffer_ == nullptr) {
    return;
  }
#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  if (this->partial_updates_enabled_ && this->partial_update_state_ != nullptr) {
    auto &state = *this->partial_update_state_;
    if (state.reset_phase != ResetPhase::RESET_PHASE_IDLE || (state.initializing_controller && state.command_failed)) {
      return;
    }
    this->write_checked_full_frame_();
    return;
  }
#endif
  if (this->is_sh1106_() || this->is_sh1107_()) {
    uint32_t i = 0;
    // Some panels wire their visible columns to a window of the controller RAM
    // that does not start at column 0 (e.g. SH1107 M5Stack Unit OLED needs offset_x: 32).
    // SH1106 keeps its historical 0x02 base column on top of any offset.
    uint8_t start_column = this->offset_x_;
    if (this->is_sh1106_()) {
      start_column += 0x02;
    }
    for (uint8_t page = 0; page < (uint8_t) this->get_height_internal() / 8; page++) {
      this->command(0xB0 + page);                 // row
      this->command(start_column & 0x0F);         // lower column
      this->command(0x10 | (start_column >> 4));  // higher column
      for (uint8_t x = 0; x < (uint8_t) this->get_width_internal() / 16; x++) {
        uint8_t data[16];
        for (uint8_t &j : data)
          j = this->buffer_[i++];
        this->write_bytes(0x40, data, sizeof(data));
      }
    }
  } else {
    size_t block_size = 16;
    if ((this->get_buffer_length_() % 24) == 0) {
      // use 24 byte block size for e.g. 72x40 displays where buffer size is multiple of 24, not 16
      block_size = 24;
    }

    for (uint32_t i = 0; i < this->get_buffer_length_();) {
      uint8_t data[block_size];
      for (uint8_t &j : data)
        j = this->buffer_[i++];
      this->write_bytes(0x40, data, sizeof(data));
    }
  }
}

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES

void I2CSSD1306::mark_partial_transport_failure_(bool reinitialize) {
  this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  if (this->partial_update_state_ == nullptr) {
    return;
  }
  auto &state = *this->partial_update_state_;
  state.have_previous_buffer = false;
  state.transport_failed = true;
  state.controller_reinit_required |= reinitialize;
}

void I2CSSD1306::commit_shadow_buffer_() {
  std::memcpy(this->partial_update_state_->previous_buffer.get(), this->buffer_, this->get_buffer_length_());
  this->partial_update_state_->have_previous_buffer = true;
}

bool I2CSSD1306::reinitialize_controller_(bool display_off) {
  auto &state = *this->partial_update_state_;
  state.command_failed = false;
  state.initializing_controller = true;
  this->init_controller_(display_off);
  state.initializing_controller = false;
  return !state.command_failed;
}

bool I2CSSD1306::write_checked_region_(const Region &region) {
  const uint16_t width = static_cast<uint16_t>(this->get_width_internal());
  const uint16_t column_base = this->get_column_start_();
  const uint8_t commands[6] = {0x21,
                               static_cast<uint8_t>(column_base + region.min_x),
                               static_cast<uint8_t>(column_base + region.max_x),
                               0x22,
                               region.min_page,
                               region.max_page};

  // Address window
  if (this->write_register(0x00, commands, sizeof(commands)) != i2c::ERROR_OK) {
    this->mark_partial_transport_failure_(true);
    return false;
  }

  // Horizontal addressing advances continuously across the selected pages.
  uint8_t data[PartialUpdatePlanner::DATA_CHUNK_SIZE];
  uint8_t chunk_size = 0;
  for (uint8_t page = region.min_page; page <= region.max_page; page++) {
    const size_t page_offset = static_cast<size_t>(page) * width;
    for (uint16_t x = region.min_x; x <= region.max_x; x++) {
      data[chunk_size++] = this->buffer_[page_offset + x];
      if (chunk_size == PartialUpdatePlanner::DATA_CHUNK_SIZE) {
        if (this->write_register(0x40, data, chunk_size) != i2c::ERROR_OK) {
          this->mark_partial_transport_failure_();
          return false;
        }
        chunk_size = 0;
      }
    }
  }
  if (chunk_size > 0) {
    if (this->write_register(0x40, data, chunk_size) != i2c::ERROR_OK) {
      this->mark_partial_transport_failure_();
      return false;
    }
  }
  return true;
}

bool I2CSSD1306::write_checked_full_frame_() {
  const Region full{0, static_cast<uint8_t>(this->get_width_internal() - 1), 0,
                    static_cast<uint8_t>((this->get_height_internal() / 8) - 1)};
  return this->write_checked_region_(full);
}

bool I2CSSD1306::finish_recovery_(bool reinitialize) {
  auto &state = *this->partial_update_state_;
  // Even a data NACK may have accompanied power loss between address probes.
  const bool restore_settings = reinitialize || state.transport_failed;
  if (restore_settings && !this->reinitialize_controller_(reinitialize)) {
    return false;
  }
  if (!this->write_checked_full_frame_()) {
    state.controller_reinit_required = true;
    return false;
  }
  // Restore the requested power state before trusting the shadow or clearing the warning.
  if (restore_settings && !this->write_byte(0x00, this->is_on_ ? 0xAF : 0xAE)) {
    this->mark_partial_transport_failure_(true);
    return false;
  }
  state.controller_reinit_required = false;
  state.command_failed = false;
  state.transport_failed = false;
  state.retry_wait = false;
  this->commit_shadow_buffer_();
  this->status_clear_warning();
  if (reinitialize) {
    ESP_LOGI(TAG, "SSD1306 controller reinitialized and framebuffer resynchronized");
  }
  return true;
}

void I2CSSD1306::recover_() {
  auto &state = *this->partial_update_state_;
  if (state.reset_phase != ResetPhase::RESET_PHASE_IDLE) {
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (state.retry_wait && now - state.last_recovery_attempt < RECOVERY_RETRY_INTERVAL_MS) {
    return;
  }
  state.last_recovery_attempt = now;
  state.retry_wait = true;
  // Probe before resetting: a disconnected panel should only cost one probe per retry interval.
  if (this->write(nullptr, 0) != i2c::ERROR_OK) {
    this->mark_partial_transport_failure_(true);
    return;
  }
  if (state.controller_reinit_required && this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    state.reset_phase = ResetPhase::RESET_PHASE_HIGH;
    state.reset_phase_started = now;
    this->enable_loop();
    return;
  }
  // Data-only failures get a soft settings/frame resync, without display-off or a reset pulse.
  this->finish_recovery_(state.controller_reinit_required);
}

void I2CSSD1306::loop() {
  if (this->is_failed() || !this->partial_updates_enabled_ || this->partial_update_state_ == nullptr) {
    this->disable_loop();
    return;
  }
  auto &state = *this->partial_update_state_;
  const uint32_t now = App.get_loop_component_start_time();
  if (state.reset_phase == ResetPhase::RESET_PHASE_HIGH) {
    if (now - state.reset_phase_started >= 1) {
      this->reset_pin_->digital_write(false);
      state.reset_phase = ResetPhase::RESET_PHASE_LOW;
      state.reset_phase_started = now;
    }
    return;
  }
  if (state.reset_phase == ResetPhase::RESET_PHASE_LOW) {
    if (now - state.reset_phase_started < 10) {
      return;
    }
    this->reset_pin_->digital_write(true);
    state.reset_phase = ResetPhase::RESET_PHASE_IDLE;
    this->disable_loop();
    this->finish_recovery_(true);
    return;
  }
  this->disable_loop();
}

void I2CSSD1306::update() {
  if (this->is_failed() || this->buffer_ == nullptr) {
    return;
  }
  if (!this->partial_updates_enabled_ || this->partial_update_state_ == nullptr) {
    SSD1306::update();
    return;
  }
  auto &state = *this->partial_update_state_;
  this->do_update_();
  if (!state.have_previous_buffer) {
    this->recover_();
    return;
  }
  if (!state.planner.build(this->buffer_, state.previous_buffer.get(), this->get_width_internal(),
                           this->get_height_internal() / 8)) {
    // Configuration validation bounds the dimensions, but keep invalid plans away from the transport.
    this->mark_partial_transport_failure_(true);
    return;
  }
  if (state.planner.get_region_count() == 0) {
    if (this->write(nullptr, 0) != i2c::ERROR_OK) {
      this->mark_partial_transport_failure_(true);
    }
    return;
  }
  for (size_t i = 0; i < state.planner.get_region_count(); i++) {
    if (!this->write_checked_region_(state.planner.get_region(i))) {
      return;
    }
  }
  this->commit_shadow_buffer_();
}
#endif

}  // namespace esphome::ssd1306_i2c
