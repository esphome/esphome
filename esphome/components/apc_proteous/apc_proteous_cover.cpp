#include "apc_proteous_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::apc_proteous {

static const char *const TAG = "apc_proteous.cover";
static const char *const START_CMD = "*1\r";
static const char *const OPEN_CMD = "*6\r";
static const char *const CLOSE_CMD = "*7\r";
static const char *const QUERY_S = "?s\r";
static const char *const QUERY_X = "?x\r";

// Upper bound on a buffered response line; the longest valid frame is "?s=XX".
static const size_t MAX_RESPONSE_LEN = 16;

// Depending on the controller's calibration the reported position may stop a
// little short of the physical limits (e.g. 99% at the fully-open reed switch)
// rather than reaching a clean 0 or 100. Treat any reading within this margin
// (in percent) of an end as fully open/closed so the endpoint states track the
// limit switches regardless of calibration.
static const uint8_t ENDPOINT_MARGIN = 2;

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
          if (this->trace_active_) {
            ESP_LOGD(TAG, "UART trace rx: '%s'", this->rx_buffer_.c_str());
          }
          this->parse_response_();
          this->rx_buffer_.clear();
        }
      } else if (data != '\n') {
        // Ignore line feeds; bound the buffer so missed delimiters or line noise
        // cannot grow it without limit (the longest valid frame is "?s=XX").
        if (this->rx_buffer_.length() >= MAX_RESPONSE_LEN) {
          if (this->trace_active_) {
            ESP_LOGD(TAG, "UART trace rx (no CR, discarding): '%s'", this->rx_buffer_.c_str());
          }
          this->rx_buffer_.clear();
        }
        this->rx_buffer_ += (char) data;
      }
    }
  }
}

void APCProteousCover::parse_response_() {
  // Expected format: "?s=XX" or "?x=XX" where XX is a hex value
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
  int32_t value = static_cast<int32_t>(strtol(hex_str, &end_ptr, 16));

  if (end_ptr == hex_str) {
    ESP_LOGW(TAG, "Failed to parse hex value: %s", this->rx_buffer_.c_str());
    return;
  }

  bool state_changed = false;

  if (type == 's') {
    // s-status: bit 0 = operating, bit 1 = direction (0=opening, 1=closing)
    this->s_status_ = (uint8_t) value;

    bool is_operating = (value & 0x01) != 0;
    bool is_opening = (value & 0x02) == 0;

    CoverOperation new_operation = COVER_OPERATION_IDLE;
    if (is_operating) {
      if (is_opening) {
        new_operation = COVER_OPERATION_OPENING;
      } else {
        new_operation = COVER_OPERATION_CLOSING;
      }
    }

    if (this->current_operation != new_operation) {
      this->current_operation = new_operation;
      state_changed = true;
    }

    // A movement command is acknowledged once the controller reports motion in that direction;
    // clear it so retry_pending_command_() stops resending.
    if ((this->pending_command_ == OPEN_CMD && new_operation == COVER_OPERATION_OPENING) ||
        (this->pending_command_ == CLOSE_CMD && new_operation == COVER_OPERATION_CLOSING)) {
      this->pending_command_ = nullptr;
      this->command_retries_ = 0;
    }

    // Keep the UART trace running until the gate has moved and then returned to idle.
    if (this->trace_active_) {
      if (new_operation != COVER_OPERATION_IDLE) {
        this->trace_saw_motion_ = true;
      } else if (this->trace_saw_motion_) {
        ESP_LOGD(TAG, "UART trace stopped: gate idle");
        this->trace_active_ = false;
      }
    }

    ESP_LOGV(TAG, "s-status: 0x%02X (operating=%d, opening=%d)", this->s_status_, is_operating, is_opening);
  } else if (type == 'x') {
    // x-status: position percentage (0-100 decimal, but sent as hex)
    this->x_status_ = (uint8_t) value;

    // Convert 0-100 to 0.0-1.0 range, snapping the extremes to the endpoints so
    // a calibration offset does not stop is_open/is_closed from ever being true.
    float new_position;
    if (this->x_status_ + ENDPOINT_MARGIN >= 100) {
      new_position = COVER_OPEN;
    } else if (this->x_status_ <= ENDPOINT_MARGIN) {
      new_position = COVER_CLOSED;
    } else {
      new_position = clamp(this->x_status_ / 100.0f, 0.0f, 1.0f);
    }

    if (this->position != new_position) {
      this->position = new_position;
      state_changed = true;
      if (this->current_operation != COVER_OPERATION_IDLE && this->target_position_ != COVER_OPEN &&
          this->target_position_ != COVER_CLOSED) {
        // Check if we've reached target position
        if ((this->current_operation == COVER_OPERATION_OPENING && this->position >= this->target_position_) ||
            (this->current_operation == COVER_OPERATION_CLOSING && this->position <= this->target_position_)) {
          // Send stop command
          ESP_LOGD(TAG, "Target position %.2f reached, sending stop command", this->target_position_);
          this->stop_cmd_();
        }
      }
    }

    ESP_LOGV(TAG, "x-status: 0x%02X (%d%%, position=%.2f)", this->x_status_, this->x_status_, this->position);
  }

  if (state_changed) {
    this->publish_state();
  }
}

void APCProteousCover::retry_pending_command_() {
  if (this->pending_command_ == nullptr) {
    return;
  }
  // The controller acknowledges a movement command by reporting motion in that direction, which
  // clears pending_command_ (see parse_response_). Give it COMMAND_ACK_MS to do so before retrying.
  if ((millis() - this->pending_command_time_) < COMMAND_ACK_MS) {
    return;
  }

  // If the gate has already reached the commanded end there is nothing to resend.
  bool is_open_cmd = this->pending_command_ == OPEN_CMD;
  if ((is_open_cmd && this->position >= COVER_OPEN) || (!is_open_cmd && this->position <= COVER_CLOSED)) {
    this->pending_command_ = nullptr;
    this->command_retries_ = 0;
    return;
  }

  if (this->command_retries_ >= MAX_COMMAND_RETRIES) {
    ESP_LOGW(TAG, "Gate did not acknowledge %s command after %u retries", is_open_cmd ? "open" : "close",
             this->command_retries_);
    this->pending_command_ = nullptr;
    this->command_retries_ = 0;
    return;
  }

  this->command_retries_++;
  ESP_LOGD(TAG, "Gate did not respond, resending %s command (retry %u)", is_open_cmd ? "open" : "close",
           this->command_retries_);
  this->pending_command_time_ = millis();
  this->write_command_(this->pending_command_);
}

void APCProteousCover::start_trace_(const char *what) {
  this->trace_active_ = true;
  this->trace_saw_motion_ = false;
  this->trace_start_time_ = millis();
  ESP_LOGD(TAG, "UART trace started (%s command)", what);
}

void APCProteousCover::update() {
  // Stop the debug trace if it has been running too long without the gate moving and stopping.
  if (this->trace_active_ && (millis() - this->trace_start_time_) > TRACE_MAX_MS) {
    ESP_LOGD(TAG, "UART trace stopped: timeout");
    this->trace_active_ = false;
  }

  // Resend a movement command the controller has not acknowledged; the serial link can drop one,
  // leaving the gate stopped until something re-issues the command.
  this->retry_pending_command_();

  // Hold off polling right after a command so the command and its echo do not collide with a
  // query. The next update() (500ms later) will poll once the quiet period has passed.
  if ((millis() - this->last_command_tx_) < COMMAND_QUIET_MS) {
    return;
  }

  // Discard any unterminated echo the controller left in the buffer so it cannot merge with the
  // response to this query. By now the line is idle, so anything here is a stale command echo.
  if (!this->rx_buffer_.empty()) {
    if (this->trace_active_) {
      ESP_LOGD(TAG, "UART trace: discarding stale rx '%s' before query", this->rx_buffer_.c_str());
    }
    this->rx_buffer_.clear();
  }

  // Alternate between querying s-status and x-status
  const char *query = this->query_s_next_ ? QUERY_S : QUERY_X;
  if (this->trace_active_) {
    ESP_LOGD(TAG, "UART trace tx: %s", this->query_s_next_ ? "?s" : "?x");
  }
  this->write_str(query);
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
  traits.set_supports_toggle(true);
  return traits;
}

void APCProteousCover::write_command_(const char *cmd) {
  // All commands go through here so update() can hold off polling for COMMAND_QUIET_MS
  // afterwards, keeping the command (and the controller's echo of it) clear of the queries.
  this->last_command_tx_ = millis();
  this->write_str(cmd);
}

void APCProteousCover::send_command_(const char *cmd) {
  // Don't publish state here: current_operation and position are updated only from
  // status reads, so the published state always reflects what the controller reports.
  // Record the command so stop_cmd_() can cancel it before the controller has reported
  // the resulting motion.
  this->pending_command_ = cmd;
  this->pending_command_time_ = millis();
  this->command_retries_ = 0;
  this->start_trace_(cmd == OPEN_CMD ? "open" : "close");
  this->write_command_(cmd);
}

void APCProteousCover::open_cmd_() {
  // OPEN_CMD is directional and idempotent, so only skip it when the controller already
  // reports the gate opening. current_operation is authoritative and self-clears to idle
  // when the gate stops, so a part-open idle gate is correctly re-commanded.
  if (this->current_operation == COVER_OPERATION_OPENING) {
    return;
  }
  ESP_LOGD(TAG, "Sending open command");
  this->send_command_(OPEN_CMD);
}

void APCProteousCover::close_cmd_() {
  if (this->current_operation == COVER_OPERATION_CLOSING) {
    return;
  }
  ESP_LOGD(TAG, "Sending close command");
  this->send_command_(CLOSE_CMD);
}

void APCProteousCover::stop_cmd_() {
  // START_CMD (*1) is a start/stop toggle, so only send it when the gate is actually
  // moving -- either the controller reports motion, or we have an in-flight movement
  // command that the controller has not reported yet. Otherwise it would start the gate.
  bool command_pending =
      this->pending_command_ != nullptr && (millis() - this->pending_command_time_) < PENDING_COMMAND_MS;
  if (this->current_operation == COVER_OPERATION_IDLE && !command_pending) {
    ESP_LOGD(TAG, "Cover already idle, ignoring stop command");
    return;
  }
  ESP_LOGD(TAG, "Sending stop command");
  this->start_trace_("stop");
  this->write_command_(START_CMD);
  this->pending_command_ = nullptr;
  // current_operation will update from the next status poll.
}

void APCProteousCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    // Send stop/start command
    this->stop_cmd_();
  } else if (call.get_position().has_value()) {
    float target_position = *call.get_position();
    this->target_position_ = target_position;

    ESP_LOGD(TAG, "Target position: %.2f, current: %.2f", target_position, this->position);

    if (target_position == COVER_OPEN) {
      // Fully open
      this->open_cmd_();
    } else if (target_position == COVER_CLOSED) {
      // Fully close
      this->close_cmd_();
    } else {
      // Partial position - open or close to approximate position
      // Since we don't have direct position control, we'll need to
      // open/close and monitor the position, then send stop when reached
      if (target_position > this->position) {
        ESP_LOGD(TAG, "Sending open command (target partial position)");
        this->open_cmd_();
      } else if (target_position < this->position) {
        ESP_LOGD(TAG, "Sending close command (target partial position)");
        this->close_cmd_();
      }
    }
  } else if (call.get_toggle()) {
    this->start_trace_("toggle");
    this->write_command_(START_CMD);
  }
}

}  // namespace esphome::apc_proteous
