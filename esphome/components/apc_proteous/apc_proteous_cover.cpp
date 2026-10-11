#include "apc_proteous_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::apc_proteous {

ESPHOME_LOG_TAG(TAG, "apc_proteous.cover");
static constexpr const char *START_CMD = "*1\r";  // start/stop toggle
static constexpr const char *OPEN_CMD = "*6\r";
static constexpr const char *CLOSE_CMD = "*7\r";
static constexpr const char *QUERY_S = "?s\r";
static constexpr const char *QUERY_X = "?x\r";

// Depending on calibration the reported position may stop a little short of the physical limits
// (e.g. 99% at the fully open reed switch), so readings this close to an end count as that end.
static constexpr uint8_t ENDPOINT_MARGIN = 2;

using namespace esphome::cover;

void APCProteousCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->position = 0.5f;
  }
}

void APCProteousCover::loop() {
  uint8_t data;
  while (this->available() > 0) {
    if (!this->read_byte(&data)) {
      continue;
    }
    if (data == '\r') {
      if (this->rx_len_ != 0) {
        this->rx_buffer_[this->rx_len_] = '\0';
        ESP_LOGVV(TAG, "rx: '%s'", this->rx_buffer_);
        this->parse_response_();
        this->rx_len_ = 0;
      }
    } else if (data != '\n') {
      if (this->rx_len_ >= MAX_RESPONSE_LEN) {
        ESP_LOGVV(TAG, "rx overflow, discarding");
        this->rx_len_ = 0;
      }
      this->rx_buffer_[this->rx_len_++] = static_cast<char>(data);
    }
  }
}

void APCProteousCover::parse_response_() {
  // Expected format: "?s=XX" or "?x=XX" where XX is a hex value
  if (this->rx_len_ < 4 || this->rx_buffer_[0] != '?' || this->rx_buffer_[2] != '=') {
    ESP_LOGV(TAG, "Invalid response: %s", this->rx_buffer_);
    return;
  }

  const char *hex_str = this->rx_buffer_ + 3;
  char *end_ptr;
  uint8_t value = static_cast<uint8_t>(strtol(hex_str, &end_ptr, 16));
  if (end_ptr == hex_str) {
    ESP_LOGW(TAG, "Failed to parse hex value: %s", this->rx_buffer_);
    return;
  }

  bool state_changed = false;
  char type = this->rx_buffer_[1];
  if (type == 's') {
    // bit 0 = operating, bit 1 = direction (0 = opening, 1 = closing)
    CoverOperation new_operation = COVER_OPERATION_IDLE;
    if (value & 0x01) {
      new_operation = (value & 0x02) ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING;
    }
    if (this->current_operation != new_operation) {
      this->current_operation = new_operation;
      state_changed = true;
    }
    // Motion in the commanded direction acknowledges the pending command
    if ((this->pending_command_ == OPEN_CMD && new_operation == COVER_OPERATION_OPENING) ||
        (this->pending_command_ == CLOSE_CMD && new_operation == COVER_OPERATION_CLOSING)) {
      this->clear_pending_();
    }
    ESP_LOGV(TAG, "s-status: 0x%02X (operation=%d)", value, new_operation);
  } else if (type == 'x') {
    // Position percentage, snapped to the ends so is_open/is_closed can latch despite calibration offsets
    float new_position;
    if (value + ENDPOINT_MARGIN >= 100) {
      new_position = COVER_OPEN;
    } else if (value <= ENDPOINT_MARGIN) {
      new_position = COVER_CLOSED;
    } else {
      new_position = value / 100.0f;
    }
    if (this->position != new_position) {
      this->position = new_position;
      state_changed = true;
      if (this->target_position_.has_value() && this->current_operation != COVER_OPERATION_IDLE &&
          ((this->current_operation == COVER_OPERATION_OPENING && this->position >= *this->target_position_) ||
           (this->current_operation == COVER_OPERATION_CLOSING && this->position <= *this->target_position_))) {
        ESP_LOGD(TAG, "Target position %.2f reached", *this->target_position_);
        this->stop_cmd_(millis());
      }
    }
    ESP_LOGV(TAG, "x-status: %d%% (position=%.2f)", value, this->position);
  }

  if (state_changed) {
    this->publish_state();
  }
}

void APCProteousCover::clear_pending_() {
  this->pending_command_ = nullptr;
  this->command_retries_ = 0;
}

void APCProteousCover::retry_pending_command_(uint32_t now) {
  if (this->pending_command_ == nullptr || (now - this->pending_command_time_) < COMMAND_ACK_MS) {
    return;
  }
  bool is_open_cmd = this->pending_command_ == OPEN_CMD;
  // Nothing to resend once the gate is at the commanded end
  if ((is_open_cmd && this->position >= COVER_OPEN) || (!is_open_cmd && this->position <= COVER_CLOSED)) {
    this->clear_pending_();
    return;
  }
  const LogString *label = is_open_cmd ? LOG_STR("open") : LOG_STR("close");
  if (this->command_retries_ >= MAX_COMMAND_RETRIES) {
    ESP_LOGW(TAG, "Gate did not acknowledge %s command after %u retries", LOG_STR_ARG(label), this->command_retries_);
    this->clear_pending_();
    return;
  }
  this->command_retries_++;
  ESP_LOGD(TAG, "Gate did not respond, resending %s command (retry %u)", LOG_STR_ARG(label), this->command_retries_);
  this->pending_command_time_ = now;
  this->write_command_(this->pending_command_, now);
}

void APCProteousCover::update() {
  const uint32_t now = millis();
  this->retry_pending_command_(now);

  // Let the command and its echo clear the line before the next query
  if ((now - this->last_command_tx_) < COMMAND_QUIET_MS) {
    return;
  }
  // The line is idle, so anything left in the buffer is an unterminated echo
  if (this->rx_len_ != 0) {
    this->rx_buffer_[this->rx_len_] = '\0';
    ESP_LOGVV(TAG, "discarding stale rx '%s' before query", this->rx_buffer_);
    this->rx_len_ = 0;
  }

  const char *query = this->query_s_next_ ? QUERY_S : QUERY_X;
  ESP_LOGVV(TAG, "tx: %s", query);
  this->write_str(query);
  this->query_s_next_ = !this->query_s_next_;
}

void APCProteousCover::dump_config() {
  LOG_COVER("", "APC Proteous Cover", this);
  LOG_UPDATE_INTERVAL(this);
  this->check_uart_settings(9600, 1, uart::UART_CONFIG_PARITY_NONE, 8);
}

CoverTraits APCProteousCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_stop(true);
  traits.set_supports_toggle(true);
  return traits;
}

void APCProteousCover::write_command_(const char *cmd, uint32_t now) {
  ESP_LOGVV(TAG, "tx: %s", cmd);
  this->last_command_tx_ = now;
  this->write_str(cmd);
}

void APCProteousCover::send_command_(const char *cmd) {
  // The published state comes only from status reads; this just records the command so a stop
  // can cancel it before the controller reports motion, and so it can be resent if dropped.
  // The commands are idempotent, so only skip one when the gate is already moving that way.
  CoverOperation operation = cmd == OPEN_CMD ? COVER_OPERATION_OPENING : COVER_OPERATION_CLOSING;
  if (this->current_operation == operation) {
    return;
  }
  ESP_LOGD(TAG, "Sending %s command",
           operation == COVER_OPERATION_OPENING ? LOG_STR_LITERAL("open") : LOG_STR_LITERAL("close"));
  const uint32_t now = millis();
  this->pending_command_ = cmd;
  this->pending_command_time_ = now;
  this->command_retries_ = 0;
  this->write_command_(cmd, now);
}

void APCProteousCover::stop_cmd_(uint32_t now) {
  // START_CMD is a start/stop toggle, so it is only sent while the gate is moving, or while a
  // movement command is in flight that the controller has not reported yet; otherwise it would start the gate.
  if (this->current_operation == COVER_OPERATION_IDLE && this->pending_command_ == nullptr) {
    ESP_LOGD(TAG, "Cover already idle, ignoring stop command");
    return;
  }
  ESP_LOGD(TAG, "Sending stop command");
  this->write_command_(START_CMD, now);
  this->clear_pending_();
  this->target_position_.reset();
}

void APCProteousCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    this->stop_cmd_(millis());
  } else if (call.get_position().has_value()) {
    float target = *call.get_position();
    ESP_LOGD(TAG, "Target position: %.2f, current: %.2f", target, this->position);
    // There is no position command: a partial target is reached by moving towards it and stopping
    // once the reported position gets there
    this->target_position_.reset();
    if (target == COVER_OPEN) {
      this->send_command_(OPEN_CMD);
    } else if (target == COVER_CLOSED) {
      this->send_command_(CLOSE_CMD);
    } else if (target != this->position) {
      this->target_position_ = target;
      this->send_command_(target > this->position ? OPEN_CMD : CLOSE_CMD);
    }
  } else if (call.get_toggle()) {
    this->clear_pending_();
    this->target_position_.reset();
    this->write_command_(START_CMD, millis());
  }
}

}  // namespace esphome::apc_proteous
