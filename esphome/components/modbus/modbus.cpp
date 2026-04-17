#include "modbus.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus {

static const char *const TAG = "modbus";

// Maximum bytes to log for Modbus frames (truncated if larger)
static constexpr size_t MODBUS_MAX_LOG_BYTES = 64;

// Approximate bits per character on the wire (depends on parity/stop bit config)
static constexpr uint32_t MODBUS_BITS_PER_CHAR = 11;
// Milliseconds per second
static constexpr uint32_t MS_PER_SEC = 1000;

void Modbus::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }

  this->frame_delay_ms_ =
      std::max(2,  // 1750us minimum per spec - rounded up to 2ms.
                   // 3.5 characters * 11 bits per character * 1000ms/sec / (bits/sec) (Standard modbus frame delay)
               (uint16_t) (3.5 * MODBUS_BITS_PER_CHAR * MS_PER_SEC / this->parent_->get_baud_rate()) + 1);

  // When rx_full_threshold is configured (non-zero), the UART has a hardware FIFO with a
  // meaningful threshold (e.g., ESP32 native UART), so we can calculate a precise delay.
  // Otherwise (e.g., USB UART), use 50ms to handle data arriving in chunks.
  //
  // If the user explicitly set long_rx_buffer_delay via YAML, keep their value and skip the
  // auto-calculation. A user-set value also unconditionally applies in loop() (see below),
  // bypassing the rx_full_threshold size gate — this is the escape hatch for slow devices
  // (e.g. some SDM-series power meters) whose responses arrive in multiple short bursts with
  // per-burst gaps longer than the auto-derived delay.
  static constexpr uint16_t DEFAULT_LONG_RX_BUFFER_DELAY_MS = 50;
  if (!this->long_rx_buffer_delay_user_set_) {
    size_t rx_threshold = this->parent_->get_rx_full_threshold();
    this->long_rx_buffer_delay_ms_ =
        rx_threshold != uart::UARTComponent::RX_FULL_THRESHOLD_UNSET
            ? (rx_threshold * MODBUS_BITS_PER_CHAR * MS_PER_SEC / this->parent_->get_baud_rate()) + 1
            : DEFAULT_LONG_RX_BUFFER_DELAY_MS;
  }
}

void Modbus::loop() {
  // First process all available incoming data.
  this->receive_and_parse_modbus_bytes_();

  // If the response frame is finished (including interframe delay) - we timeout.
  // The long_rx_buffer_delay accounts for long responses (larger than the UART rx_full_threshold) to avoid timeouts
  // when the buffer is filling the back half of the response.
  //
  // When the user explicitly set long_rx_buffer_delay via YAML, it applies unconditionally
  // (no rx_full_threshold gate). The gate is only a useful heuristic for the auto-derived
  // value — users who know their device sends in short bursts with long gaps want the long
  // delay to apply from the very first byte, not only once the buffer has already grown.
  const uint16_t long_delay =
      this->long_rx_buffer_delay_user_set_
          ? this->long_rx_buffer_delay_ms_
          : (this->rx_buffer_.size() >= this->parent_->get_rx_full_threshold() ? this->long_rx_buffer_delay_ms_ : 0);
  const uint16_t timeout = std::max((uint16_t) this->frame_delay_ms_, long_delay);
  // We use millis() here and elsewhere instead of App.get_loop_component_start_time() to avoid stale timestamps
  // It's critical in all timestamp comparisons that the left timestamp comes before the right one in time
  // If we use a cached value in place of millis() and last_modbus_byte_ is updated inside our loop
  // then the comparison is backwards (small negative which wraps to large positive) and will cause a false timeout
  // So in this component we don't use any cached timestamp values to avoid these annoying bugs
  if (millis() - this->last_modbus_byte_ > timeout) {
    this->clear_rx_buffer_(LOG_STR("timeout after partial response"), true);
  }

  // If we're past the send_wait_time timeout and response buffer doesn't have the start of the expected response
  if (this->waiting_for_response_ != 0 &&
      millis() - this->last_send_ > this->last_send_tx_offset_ + this->send_wait_time_ &&
      (this->rx_buffer_.empty() || this->rx_buffer_[0] != this->waiting_for_response_)) {
    ESP_LOGW(TAG, "Stop waiting for response from %" PRIu8 " %" PRIu32 "ms after last send",
             this->waiting_for_response_, millis() - this->last_send_);
    this->waiting_for_response_ = 0;
  }

  // If there's no response pending and there's commands in the buffer
  this->send_next_frame_();
}

bool Modbus::tx_blocked() {
  const uint32_t now = millis();

  // We block transmission in any of these case:
  // 1. There are bytes in the UART Rx buffer
  // 2. There are bytes in our Rx buffer
  // 3. We're waiting for a response
  // 4. The last sent byte isn't more than frame_delay ms ago (i.e. wait to tell receivers that our previous Tx is done)
  // 5. The last received byte isn't more than frame_delay ms ago (i.e. wait to be sure there isn't more Rx coming)
  // 6. If we're a client - also wait for the turnaround delay, to give the servers time to process the previous message
  return this->available() || !this->rx_buffer_.empty() || (this->waiting_for_response_ != 0) ||
         (now - this->last_send_ < this->last_send_tx_offset_ + this->frame_delay_ms_ +
                                       (this->role == ModbusRole::CLIENT ? this->turnaround_delay_ms_ : 0)) ||
         (now - this->last_modbus_byte_ <
          this->frame_delay_ms_ + (this->role == ModbusRole::CLIENT ? this->turnaround_delay_ms_ : 0));
}

bool Modbus::tx_buffer_empty() { return this->tx_buffer_.empty(); }

void Modbus::receive_and_parse_modbus_bytes_() {
  // Read all available bytes in batches to reduce UART call overhead.
  size_t avail = this->available();
  uint8_t buf[64];
  while (avail > 0) {
    size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read)) {
      break;
    }
    avail -= to_read;
    for (size_t i = 0; i < to_read; i++) {
      if (this->rx_buffer_.empty()) {
        ESP_LOGV(TAG, "Received first byte %" PRIu8 " (0X%x) %" PRIu32 "ms after last send", buf[i], buf[i],
                 millis() - this->last_send_);
      } else {
        ESP_LOGVV(TAG, "Received byte %" PRIu8 " (0X%x) %" PRIu32 "ms after last send", buf[i], buf[i],
                  millis() - this->last_send_);
      }

      // parse_modbus_byte_ is responsible for resynchronising the buffer on CRC failure, so it
      // always returns true and we never clear the buffer here.
      this->parse_modbus_byte_(buf[i]);
      this->last_modbus_byte_ = millis();
    }
  }
}

bool Modbus::parse_modbus_byte_(uint8_t byte) {
  // Append the new byte and try to parse a frame from the front of rx_buffer_.
  // We resynchronise past any leading junk so the parser can recover from e.g. stale bytes
  // from a late-arriving previous response, or direction-switch noise from some RS485
  // transceiver modules, without waiting for the receive timeout.
  //
  // Two resync mechanisms cooperate:
  //
  // 1. CLIENT fast-path: while a response is pending (waiting_for_response_ != 0), any byte at
  //    the front of rx_buffer_ that doesn't match the expected slave address is dropped
  //    immediately. This avoids the trap where junk byte 1 looks like a function code and
  //    junk byte 2 looks like a large payload length, causing the parser to swallow the real
  //    frame as "payload" without ever reaching a CRC check.
  //
  // 2. BAD_FRAME drop-one-byte retry: if try_parse_rx_buffer_ reaches a CRC check and it
  //    fails, we drop byte 0 and retry. This handles junk that happens to begin with the
  //    expected address by coincidence.
  this->rx_buffer_.push_back(byte);
  while (!this->rx_buffer_.empty()) {
    if (this->role == ModbusRole::CLIENT && this->waiting_for_response_ != 0 &&
        this->rx_buffer_[0] != this->waiting_for_response_) {
      ESP_LOGV(TAG, "Resync: dropping 0x%02X (waiting for address 0x%02X)", this->rx_buffer_[0],
               this->waiting_for_response_);
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }
    const ParseResult result = this->try_parse_rx_buffer_();
    if (result == ParseResult::NEEDS_MORE_BYTES || result == ParseResult::FRAME_PROCESSED) {
      return true;
    }
    // BAD_FRAME: drop the first byte and retry parsing from the new front.
    ESP_LOGV(TAG, "Resync: dropping byte 0x%02X after CRC fail, %zu bytes remain", this->rx_buffer_[0],
             this->rx_buffer_.size() - 1);
    this->rx_buffer_.erase(this->rx_buffer_.begin());
  }
  return true;
}

Modbus::ParseResult Modbus::try_parse_rx_buffer_() {
  const size_t size = this->rx_buffer_.size();
  // Byte 0: modbus address, byte 1: function code, byte 2: size (for read registers).
  if (size < 3) {
    return ParseResult::NEEDS_MORE_BYTES;
  }
  const uint8_t *raw = this->rx_buffer_.data();

  uint8_t address = raw[0];
  uint8_t function_code = raw[1];

  uint8_t data_len = raw[2];
  uint8_t data_offset = 3;

  // Per https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf Ch 5 User-Defined function codes
  if (((function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_1_INIT) &&
       (function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_1_END)) ||
      ((function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_2_INIT) &&
       (function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_2_END))) {
    // Handle user-defined function, since we don't know how big this ought to be,
    // ideally we should delegate the entire length detection to whatever handler is
    // installed, but wait, there is the CRC, and if we get a hit there is a good
    // chance that this is a complete message ... admittedly there is a small chance is
    // isn't but that is quite small given the purpose of the CRC in the first place
    data_offset = 1;
    data_len = size - data_offset - 2;

    uint16_t computed_crc = crc16(raw, data_offset + data_len);
    uint16_t remote_crc = uint16_t(raw[data_offset + data_len]) | (uint16_t(raw[data_offset + data_len + 1]) << 8);

    if (computed_crc != remote_crc) {
      // For user-defined functions the frame length is discovered by searching for a matching CRC;
      // keep accumulating bytes until either the CRC matches or the frame-delay timeout fires.
      return ParseResult::NEEDS_MORE_BYTES;
    }

    ESP_LOGD(TAG, "User-defined function %02X found", function_code);

  } else {
    // data starts at 2 and length is 4 for read registers commands
    if (this->role == ModbusRole::SERVER) {
      if (function_code == ModbusFunctionCode::READ_COILS ||
          function_code == ModbusFunctionCode::READ_DISCRETE_INPUTS ||
          function_code == ModbusFunctionCode::READ_HOLDING_REGISTERS ||
          function_code == ModbusFunctionCode::READ_INPUT_REGISTERS ||
          function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER) {
        data_offset = 2;
        data_len = 4;
      } else if (function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
        if (size < 7) {
          return ParseResult::NEEDS_MORE_BYTES;
        }
        data_offset = 2;
        // starting address (2 bytes) + quantity of registers (2 bytes) + byte count itself (1 byte) + actual byte count
        data_len = 2 + 2 + 1 + raw[6];
      }
    } else {
      // the response for write command mirrors the requests and data starts at offset 2 instead of 3 for read commands
      if (function_code == ModbusFunctionCode::WRITE_SINGLE_COIL ||
          function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER ||
          function_code == ModbusFunctionCode::WRITE_MULTIPLE_COILS ||
          function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
        data_offset = 2;
        data_len = 4;
      }
    }

    // Error ( msb indicates error )
    // response format:  Byte[0] = device address, Byte[1] function code | 0x80 , Byte[2] exception code, Byte[3-4] crc
    if ((function_code & FUNCTION_CODE_EXCEPTION_MASK) == FUNCTION_CODE_EXCEPTION_MASK) {
      data_offset = 2;
      data_len = 1;
    }

    // Need header + data + 2 CRC bytes before we can validate.
    if (size < static_cast<size_t>(data_offset) + data_len + 2) {
      return ParseResult::NEEDS_MORE_BYTES;
    }

    uint16_t computed_crc = crc16(raw, data_offset + data_len);
    uint16_t remote_crc = uint16_t(raw[data_offset + data_len]) | (uint16_t(raw[data_offset + data_len + 1]) << 8);
    if (computed_crc != remote_crc) {
      if (this->disable_crc_) {
        ESP_LOGD(TAG, "CRC check failed %" PRIu32 "ms after last send; ignoring", millis() - this->last_send_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
        char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
        ESP_LOGVV(TAG, "  (%02X != %02X)  %s", computed_crc, remote_crc,
                  format_hex_pretty_to(hex_buf, this->rx_buffer_.data(), this->rx_buffer_.size()));
        // Fall through: user has opted into accepting mismatched frames, publish the data.
      } else {
        // Demoted to VERBOSE because resync will typically fire on every poll on a lossy bus
        // and we don't want to flood the log. The hex dump stays at VERY_VERBOSE.
        ESP_LOGV(TAG, "CRC check failed %" PRIu32 "ms after last send", millis() - this->last_send_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
        char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
        ESP_LOGVV(TAG, "  (%02X != %02X) %s", computed_crc, remote_crc,
                  format_hex_pretty_to(hex_buf, this->rx_buffer_.data(), this->rx_buffer_.size()));
        return ParseResult::BAD_FRAME;
      }
    }
  }
  std::vector<uint8_t> data(this->rx_buffer_.begin() + data_offset, this->rx_buffer_.begin() + data_offset + data_len);
  bool found = false;
  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      found = true;
      if (this->role == ModbusRole::SERVER) {
        if (function_code == ModbusFunctionCode::READ_HOLDING_REGISTERS ||
            function_code == ModbusFunctionCode::READ_INPUT_REGISTERS) {
          device->on_modbus_read_registers(function_code, uint16_t(data[1]) | (uint16_t(data[0]) << 8),
                                           uint16_t(data[3]) | (uint16_t(data[2]) << 8));
        } else if (function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER ||
                   function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
          device->on_modbus_write_registers(function_code, data);
        }
      } else {  // We're a client
        // Is it an error response?
        if ((function_code & FUNCTION_CODE_EXCEPTION_MASK) == FUNCTION_CODE_EXCEPTION_MASK) {
          uint8_t exception = raw[2];
          ESP_LOGW(TAG,
                   "Error function code: 0x%X exception: %" PRIu8 ", address: %" PRIu8 ", %" PRIu32
                   "ms after last send",
                   function_code, exception, address, millis() - this->last_send_);
          if (this->waiting_for_response_ == address) {
            device->on_modbus_error(function_code & FUNCTION_CODE_MASK, exception);
          } else {
            // Ignore modbus exception not related to a pending command
            ESP_LOGD(TAG, "Ignoring error - not expecting a response from %" PRIu8 "", address);
          }
        } else {  // Not an error response
          if (this->waiting_for_response_ == address) {
            device->on_modbus_data(data);
          } else {
            // Ignore modbus response not related to a pending command
            ESP_LOGW(TAG, "Ignoring response - not expecting a response from %" PRIu8 ", %" PRIu32 "ms after last send",
                     address, millis() - this->last_send_);
          }
        }
      }
    }
  }

  if (!found && this->role == ModbusRole::CLIENT) {
    // Demoted to DEBUG: with byte-walking resync, junk bytes can very occasionally CRC-match an
    // unconfigured address. A legitimate unknown-address frame is still surfaced at DEBUG and
    // users investigating bus setup will see it at that level.
    ESP_LOGD(TAG, "Got frame from unknown address %" PRIu8 ", %" PRIu32 "ms after last send", address,
             millis() - this->last_send_);
  }

  this->clear_rx_buffer_(LOG_STR("parse succeeded"));

  if (this->waiting_for_response_ == address)
    this->waiting_for_response_ = 0;

  return ParseResult::FRAME_PROCESSED;
}

void Modbus::send_next_frame_() {
  if (this->tx_buffer_.empty())
    return;

  if (this->tx_blocked())
    return;

  const ModbusDeviceCommand &frame = this->tx_buffer_.front();

  if (this->role == ModbusRole::CLIENT) {
    this->waiting_for_response_ = frame.data.get()[0];
  }

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
    this->write_array(frame.data.get(), frame.size);
    this->flush();
    this->flow_control_pin_->digital_write(false);
    this->last_send_tx_offset_ = 0;
  } else {
    this->write_array(frame.data.get(), frame.size);
    this->last_send_tx_offset_ = frame.size * MODBUS_BITS_PER_CHAR * MS_PER_SEC / this->parent_->get_baud_rate() + 1;
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
  ESP_LOGV(TAG, "Write: %s %" PRIu32 "ms after last send", format_hex_pretty_to(hex_buf, frame.data.get(), frame.size),
           millis() - this->last_send_);
  this->last_send_ = millis();
  this->tx_buffer_.pop_front();
  if (!this->tx_buffer_.empty()) {
    ESP_LOGV(TAG, "Write queue contains %zu items.", this->tx_buffer_.size());
  }
}

void Modbus::dump_config() {
  ESP_LOGCONFIG(
      TAG,
      "Modbus:\n"
      "  Send Wait Time: %d ms\n"
      "  Turnaround Time: %d ms\n"
      "  Frame Delay: %d ms\n"
      "  Long Rx Buffer Delay: %d ms%s\n"
      "  CRC Disabled: %s",
      this->send_wait_time_, this->turnaround_delay_ms_, this->frame_delay_ms_, this->long_rx_buffer_delay_ms_,
      this->long_rx_buffer_delay_user_set_ ? " (user-set, applies unconditionally)" : "", YESNO(this->disable_crc_));
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
}
float Modbus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

void Modbus::send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
                  uint8_t payload_len, const uint8_t *payload) {
  static const size_t MAX_VALUES = 128;

  // Only check max number of registers for standard function codes
  // Some devices use non standard codes like 0x43
  if (number_of_entities > MAX_VALUES && function_code <= ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
    ESP_LOGE(TAG, "send too many values %d max=%zu", number_of_entities, MAX_VALUES);
    return;
  }

  uint8_t data[MAX_FRAME_SIZE];
  size_t pos = 0;

  data[pos++] = address;
  data[pos++] = function_code;
  if (this->role == ModbusRole::CLIENT) {
    data[pos++] = start_address >> 8;
    data[pos++] = start_address >> 0;
    if (function_code != ModbusFunctionCode::WRITE_SINGLE_COIL &&
        function_code != ModbusFunctionCode::WRITE_SINGLE_REGISTER) {
      data[pos++] = number_of_entities >> 8;
      data[pos++] = number_of_entities >> 0;
    }
  }

  if (payload != nullptr) {
    if (this->role == ModbusRole::SERVER || function_code == ModbusFunctionCode::WRITE_MULTIPLE_COILS ||
        function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {  // Write multiple
      data[pos++] = payload_len;                                          // Byte count is required for write
    } else {
      payload_len = 2;  // Write single register or coil
    }
    if (payload_len + pos + 2 > MAX_FRAME_SIZE) {  // Check if payload fits (accounting for CRC)
      ESP_LOGE(TAG, "Payload too large to send: %d bytes", payload_len);
      return;
    }
    for (int i = 0; i < payload_len; i++) {
      data[pos++] = payload[i];
    }
  }

  this->queue_raw_(data, pos);
}

// Helper function for lambdas
// Send raw command. Except CRC everything must be contained in payload
void Modbus::send_raw(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }
  // Frame size: payload + CRC(2)
  if (payload.size() + 2 > MAX_FRAME_SIZE) {
    ESP_LOGE(TAG, "Attempted to send frame larger than max frame size of %d bytes", MAX_FRAME_SIZE);
    return;
  }
  // Use stack buffer - Modbus frames are small and bounded
  uint8_t data[MAX_FRAME_SIZE];

  std::memcpy(data, payload.data(), payload.size());

  this->queue_raw_(data, payload.size());
}

// Assume data and length is valid and append CRC, then queue for sending. Used internally to avoid unnecessary copying
// of data into vectors
void Modbus::queue_raw_(const uint8_t *data, uint16_t len) {
  if (this->tx_buffer_.size() < MODBUS_TX_BUFFER_SIZE) {
    this->tx_buffer_.emplace_back(data, len);
  } else {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
    char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
    ESP_LOGE(TAG, "Write buffer full, dropped: %s", format_hex_pretty_to(hex_buf, data, len));
  }
}

void Modbus::clear_rx_buffer_(const LogString *reason, bool warn) {
  size_t at = this->rx_buffer_.size();
  if (at > 0) {
    if (warn) {
      ESP_LOGW(TAG, "Clearing buffer of %zu bytes - %s %" PRIu32 "ms after last send", at, LOG_STR_ARG(reason),
               millis() - this->last_send_);
    } else {
      ESP_LOGV(TAG, "Clearing buffer of %zu bytes - %s %" PRIu32 "ms after last send", at, LOG_STR_ARG(reason),
               millis() - this->last_send_);
    }
    this->rx_buffer_.clear();
  }
}

}  // namespace modbus
}  // namespace esphome
