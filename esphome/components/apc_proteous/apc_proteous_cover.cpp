#include "apc_proteous_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace apc_proteous {

static const char *const TAG = "apc_proteous.cover";

using namespace esphome::cover;

void APCProteousCover::setup() {
  // Try to restore previous state
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // Default to unknown state
    this->position = 0.5f;
  }
  this->current_operation = COVER_OPERATION_IDLE;
}

void APCProteousCover::loop() {
  // Read incoming UART data
  uint8_t data;
  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      // Carriage return marks end of message
      if (data == '\r') {
        if (!this->rx_buffer_.empty()) {
          this->parse_response_();
          this->rx_buffer_.clear();
        }
      } else {
        this->rx_buffer_ += (char) data;
      }
    }
  }
}

void APCProteousCover::parse_response_() {
  // Expected format: "?s-XX" or "?x-XX" where XX is hex value
  if (this->rx_buffer_.length() < 4 || this->rx_buffer_[0] != '?') {
    ESP_LOGV(TAG, "Invalid response: %s", this->rx_buffer_.c_str());
    return;
  }

  char type = this->rx_buffer_[1];
  if (this->rx_buffer_[2] != '=') {
    ESP_LOGV(TAG, "Invalid response format: %s", this->rx_buffer_.c_str());
    return;
  }

  // Parse hex value
  const char *hex_str = this->rx_buffer_.c_str() + 3;
  char *end_ptr;
  long value = strtol(hex_str, &end_ptr, 16);

  if (end_ptr == hex_str) {
    ESP_LOGW(TAG, "Failed to parse hex value: %s", this->rx_buffer_.c_str());
    return;
  }

  bool state_changed = false;

  if (type == 's') {
    // s-status: bit 0 = operating, bit 1 = open
    this->s_status_ = (uint8_t) value;

    bool is_operating = (value & 0x01) != 0;
    bool is_closed = (value & 0x02) == 0;

    CoverOperation new_operation = COVER_OPERATION_IDLE;
    if (is_operating) {
      // Determine direction based on whether we're closed or not
      // If closed and operating, we're opening. Otherwise, we're closing.
      if (is_closed || this->position < 0.5f) {
        new_operation = COVER_OPERATION_OPENING;
      } else {
        new_operation = COVER_OPERATION_CLOSING;
      }
    }

    if (this->current_operation != new_operation) {
      this->current_operation = new_operation;
      state_changed = true;
    }

    ESP_LOGV(TAG, "s-status: 0x%02X (operating=%d, closed=%d)", this->s_status_, is_operating, is_closed);
  } else if (type == 'x') {
    // x-status: position percentage (0-100 decimal, but sent as hex)
    this->x_status_ = (uint8_t) value;

    // Convert 0-100 to 0.0-1.0 range
    float new_position = this->x_status_ / 100.0f;
    new_position = clamp(new_position, 0.0f, 1.0f);

    if (fabs(this->position - new_position) > 0.01f) {
      this->position = new_position;
      state_changed = true;
      this->initial_state_received_ = true;
    }

    ESP_LOGV(TAG, "x-status: 0x%02X (%d%%, position=%.2f)", this->x_status_, this->x_status_, this->position);
  }

  if (state_changed) {
    this->publish_state();
  }
}

void APCProteousCover::update() {
  // Alternate between querying s-status and x-status
  if (this->query_s_next_) {
    this->write_str("?s\r");
  } else {
    this->write_str("?x\r");
  }
  this->query_s_next_ = !this->query_s_next_;
}

void APCProteousCover::dump_config() {
  LOG_COVER("", "APC Proteous Cover", this);
  this->check_uart_settings(9600, 1, uart::UART_CONFIG_PARITY_NONE, 8);
}

CoverTraits APCProteousCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_stop(true);
  traits.set_is_assumed_state(false);
  return traits;
}

void APCProteousCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    // Send stop/start command
    ESP_LOGD(TAG, "Sending stop command");
    this->write_str("*1\r");
    this->current_operation = COVER_OPERATION_IDLE;
    this->publish_state();
  } else if (call.get_position().has_value()) {
    float target_position = *call.get_position();

    ESP_LOGD(TAG, "Target position: %.2f, current: %.2f", target_position, this->position);

    if (target_position == COVER_OPEN) {
      // Fully open
      ESP_LOGD(TAG, "Sending open command");
      this->write_str("*6\r");
      this->current_operation = COVER_OPERATION_OPENING;
    } else if (target_position == COVER_CLOSED) {
      // Fully close
      ESP_LOGD(TAG, "Sending close command");
      this->write_str("*7\r");
      this->current_operation = COVER_OPERATION_CLOSING;
    } else {
      // Partial position - open or close to approximate position
      // Since we don't have direct position control, we'll need to
      // open/close and monitor the position, then send stop when reached
      if (target_position > this->position) {
        ESP_LOGD(TAG, "Sending open command (target partial position)");
        this->write_str("*6\r");
        this->current_operation = COVER_OPERATION_OPENING;
      } else if (target_position < this->position) {
        ESP_LOGD(TAG, "Sending close command (target partial position)");
        this->write_str("*7\r");
        this->current_operation = COVER_OPERATION_CLOSING;
      }
    }

    this->publish_state();
  }
}

}  // namespace apc_proteous
}  // namespace esphome
