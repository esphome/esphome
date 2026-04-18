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
  static constexpr uint16_t DEFAULT_LONG_RX_BUFFER_DELAY_MS = 50;
  size_t rx_threshold = this->parent_->get_rx_full_threshold();
  this->long_rx_buffer_delay_ms_ =
      rx_threshold != uart::UARTComponent::RX_FULL_THRESHOLD_UNSET
          ? (rx_threshold * MODBUS_BITS_PER_CHAR * MS_PER_SEC / this->parent_->get_baud_rate()) + 1
          : DEFAULT_LONG_RX_BUFFER_DELAY_MS;
}

void Modbus::loop() {
  // First process all available incoming data.
  this->receive_and_parse_modbus_bytes_();

  // If the response frame is finished (including interframe delay) - we timeout.
  // The long_rx_buffer_delay accounts for long responses (larger than the UART rx_full_threshold) to avoid timeouts
  // when the buffer is filling the back half of the response
  const uint16_t timeout = std::max(
      (uint16_t) this->frame_delay_ms_,
      (uint16_t) (this->rx_buffer_.size() >= this->parent_->get_rx_full_threshold() ? this->long_rx_buffer_delay_ms_
                                                                                    : 0));
  // We use millis() here and elsewhere instead of App.get_loop_component_start_time() to avoid stale timestamps
  // It's critical in all timestamp comparisons that the left timestamp comes before the right one in time
  // If we use a cached value in place of millis() and last_modbus_byte_ is updated inside our loop
  // then the comparison is backwards (small negative which wraps to large positive) and will cause a false timeout
  // So in this component we don't use any cached timestamp values to avoid these annoying bugs
  if (!this->rx_buffer_.empty() && millis() - this->last_modbus_byte_ > timeout) {
    // Interframe gap expired. Try one last resync - a valid frame may be
    // buried after leading noise bytes that prevented extraction earlier.
    this->try_extract_frame_();
    if (!this->rx_buffer_.empty()) {
      const size_t dump_size = std::min(this->rx_buffer_.size(), MODBUS_MAX_LOG_BYTES);
      char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
      ESP_LOGD(TAG, "Timeout buffer (%zu bytes) %" PRIu32 "ms after last send: %s", this->rx_buffer_.size(),
               millis() - this->last_send_, format_hex_pretty_to(hex_buf, this->rx_buffer_.data(), dump_size));
      this->clear_rx_buffer_(LOG_STR("timeout after partial response"), true);
    }
  }

  // If send_wait_time has fully elapsed, give up waiting for the current response - even
  // if a partial frame is already in rx_buffer_. Holding off the timeout while bytes are
  // trickling in risks misattributing a late response (that happens to start with the
  // expected device address) to the next queued command. The quarantine block in
  // send_next_frame_ will keep the bus quiet long enough for any stragglers to land and
  // be discarded by dispatch_frame_ (which sees waiting_for_response_ == 0 and ignores them).
  if (this->waiting_for_response_ != 0 &&
      millis() - this->last_send_ > this->last_send_tx_offset_ + this->send_wait_time_) {
    ESP_LOGW(TAG, "Stop waiting for response from %" PRIu8 " %" PRIu32 "ms after last send",
             this->waiting_for_response_, millis() - this->last_send_);
    this->waiting_for_response_ = 0;
    this->waiting_device_ = nullptr;
    this->post_timeout_quarantine_ = true;
    this->post_timeout_ts_ = millis();
    return;
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
  size_t avail = this->available();
  if (avail == 0)
    return;

  // Batch-read all available bytes into rx_buffer_.
  bool was_empty = this->rx_buffer_.empty();
  uint8_t buf[64];
  while (avail > 0) {
    size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read))
      break;
    this->rx_buffer_.insert(this->rx_buffer_.end(), buf, buf + to_read);
    avail -= to_read;
  }
  this->last_modbus_byte_ = millis();

  if (was_empty && !this->rx_buffer_.empty()) {
    const uint8_t first = this->rx_buffer_[0];
    ESP_LOGV(TAG, "Received first byte %" PRIu8 " (0X%x) %" PRIu32 "ms after last send", first, first,
             millis() - this->last_send_);
  }

  // Guard against unbounded growth from continuous noise
  if (this->rx_buffer_.size() > MAX_FRAME_SIZE) {
    size_t excess = this->rx_buffer_.size() - MAX_FRAME_SIZE;
    ESP_LOGW(TAG, "Rx buffer overflow (%zu bytes), discarding oldest %zu", this->rx_buffer_.size(), excess);
    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + excess);
  }

  this->drop_impossible_leading_bytes_();
  this->try_extract_frame_();
}

void Modbus::drop_impossible_leading_bytes_() {
  while (!this->rx_buffer_.empty()) {
    const uint8_t first = this->rx_buffer_.front();
    bool impossible = false;

    if (this->waiting_for_response_ != 0) {
      // While a response is pending, any leading byte that does not match the expected
      // device address cannot begin a valid RTU response frame.
      impossible = first != this->waiting_for_response_;
    } else {
      // With no response pending, only trim explicit NUL noise eagerly.
      impossible = first == 0x00;
    }

    if (!impossible) {
      break;
    }

    // ESP_LOGV, not ESP_LOGD: on a continuously noisy bus this fires once per byte and
    // would flood the default log. Operators enable V when they need to investigate.
    ESP_LOGV(TAG, "Dropping impossible leading byte 0x%02X while waiting for addr=%" PRIu8, first,
             this->waiting_for_response_);
    this->rx_buffer_.erase(this->rx_buffer_.begin());
  }
}

// Examine rx_buffer_ starting at byte 0 for a complete, valid Modbus RTU frame.
// Returns:
//   > 0  total frame length (including CRC) - valid frame ready to dispatch
//     0  incomplete frame, need more bytes
//    -1  CRC failure or implausible header - caller should resync
int Modbus::check_frame_() const {
  const size_t len = this->rx_buffer_.size();
  // Absolute minimum: addr(1) + FC(1) + CRC(2) = 4 bytes. Standard FCs need more than this
  // and the check further down handles their lengths; user-defined FCs can legally be this short.
  if (len < 4)
    return 0;

  const uint8_t *raw = this->rx_buffer_.data();
  uint8_t function_code = raw[1];

  // --- User-defined function codes: unknown payload length, speculative CRC ---
  // Per https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf Ch 5
  if (((function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_1_INIT) &&
       (function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_1_END)) ||
      ((function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_2_INIT) &&
       (function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_2_END))) {
    // Try CRC at every possible frame end (min 4 bytes: addr+FC+CRC_L+CRC_H)
    for (size_t try_len = 4; try_len <= len && try_len <= MAX_FRAME_SIZE; try_len++) {
      size_t crc_pos = try_len - 2;
      uint16_t computed = crc16(raw, crc_pos);
      uint16_t remote = uint16_t(raw[crc_pos]) | (uint16_t(raw[crc_pos + 1]) << 8);
      if (computed == remote)
        return static_cast<int>(try_len);
    }
    return (len < MAX_FRAME_SIZE) ? 0 : -1;
  }

  // Standard function codes need at least an exception-response shape:
  // addr(1) + FC(1) + exception(1) + CRC(2) = 5 bytes.
  if (len < 5)
    return 0;

  // --- Standard function codes: deterministic frame length ---
  uint8_t data_offset = 3;
  uint8_t data_len = raw[2];

  // See also https://en.wikipedia.org/wiki/Modbus
  if (this->role == ModbusRole::SERVER) {
    // data starts at 2 and length is 4 for read registers commands
    if (function_code == ModbusFunctionCode::READ_COILS || function_code == ModbusFunctionCode::READ_DISCRETE_INPUTS ||
        function_code == ModbusFunctionCode::READ_HOLDING_REGISTERS ||
        function_code == ModbusFunctionCode::READ_INPUT_REGISTERS ||
        function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER) {
      data_offset = 2;
      data_len = 4;
    } else if (function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
      if (len < 7)
        return 0;  // Need raw[6] for byte count
      data_offset = 2;
      // starting address (2 bytes) + quantity of registers (2 bytes) + byte count itself (1 byte) + actual byte count
      data_len = 2 + 2 + 1 + raw[6];
    }
  } else {
    // CLIENT mode - the response for write command mirrors the requests and data starts at offset 2 instead of 3 for
    // read commands
    if (function_code == ModbusFunctionCode::WRITE_SINGLE_COIL ||
        function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER ||
        function_code == ModbusFunctionCode::WRITE_MULTIPLE_COILS ||
        function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
      data_offset = 2;
      data_len = 4;
    }
    // else: read response - data_offset=3, data_len=raw[2] (defaults)
  }

  // Error ( msb indicates error ) overrides any previous layout
  // response format:  Byte[0] = device address, Byte[1] function code | 0x80 , Byte[2] exception code, Byte[3-4] crc
  if ((function_code & FUNCTION_CODE_EXCEPTION_MASK) == FUNCTION_CODE_EXCEPTION_MASK) {
    data_offset = 2;
    data_len = 1;
  }

  size_t frame_len = static_cast<size_t>(data_offset) + data_len + 2;

  // Implausible length - noise probably corrupted the header
  if (frame_len > MAX_FRAME_SIZE)
    return -1;

  // Not enough bytes yet
  if (len < frame_len)
    return 0;

  // CRC validation
  uint16_t computed = crc16(raw, data_offset + data_len);
  uint16_t remote = uint16_t(raw[data_offset + data_len]) | (uint16_t(raw[data_offset + data_len + 1]) << 8);

  if (computed != remote) {
    if (this->disable_crc_) {
      ESP_LOGD(TAG, "CRC check failed %" PRIu32 "ms after last send; ignoring", millis() - this->last_send_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
      char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
      ESP_LOGVV(TAG, "  (%02X != %02X)  %s", computed, remote,
                format_hex_pretty_to(hex_buf, this->rx_buffer_.data(), this->rx_buffer_.size()));
      return static_cast<int>(frame_len);
    }
    {
      const size_t dump_size = std::min(this->rx_buffer_.size(), MODBUS_MAX_LOG_BYTES);
      char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
      ESP_LOGD(TAG, "CRC check failed %" PRIu32 "ms after last send (%04X != %04X) %zu bytes: %s",
               millis() - this->last_send_, computed, remote, this->rx_buffer_.size(),
               format_hex_pretty_to(hex_buf, this->rx_buffer_.data(), dump_size));
    }
    return -1;
  }

  return static_cast<int>(frame_len);
}

void Modbus::try_extract_frame_() {
  // Standard Modbus RTU frames are at least 5 bytes, but user-defined function
  // codes can legally have no payload and therefore be only 4 bytes
  // (addr + function + CRC16).
  static constexpr size_t MIN_FRAME_SIZE = 4;
  // Upper bound on how much leading noise we will skip before giving up and clearing
  // the buffer. 16 bytes covers the common "a few stray bits before the real frame"
  // case without risking unbounded CPU on a line that is just continuously garbage.
  static constexpr size_t MAX_RESYNC_DROPS = 16;
  size_t drops = 0;

  while (this->rx_buffer_.size() >= MIN_FRAME_SIZE) {
    int frame_len = this->check_frame_();

    if (frame_len > 0) {
      // Valid frame found
      if (drops > 0) {
        ESP_LOGD(TAG, "Resync: recovered frame after dropping %zu noise byte(s)", drops);
      }
      this->dispatch_frame_(static_cast<size_t>(frame_len));
      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + frame_len);
      drops = 0;
      continue;  // Check for another frame in remaining bytes
    }

    if (frame_len == 0) {
      return;  // Incomplete - wait for more bytes
    }

    // frame_len == -1: CRC failure or bad header - try resync by dropping byte 0
    if (drops >= MAX_RESYNC_DROPS) {
      ESP_LOGW(TAG, "Resync exhausted after dropping %zu bytes, clearing buffer", drops);
      this->clear_rx_buffer_(LOG_STR("resync exhausted"), true);
      return;
    }
    // ESP_LOGV for per-byte drops - the recovery summary at ESP_LOGD covers the useful signal.
    ESP_LOGV(TAG, "Resync: dropping leading byte 0x%02X (%zu bytes remain)", this->rx_buffer_[0],
             this->rx_buffer_.size() - 1);
    this->rx_buffer_.erase(this->rx_buffer_.begin());
    drops++;
  }
}

void Modbus::dispatch_frame_(size_t frame_len) {
  const uint8_t *raw = this->rx_buffer_.data();
  uint8_t address = raw[0];
  uint8_t function_code = raw[1];

  uint8_t data_offset;
  uint8_t data_len;

  // Determine data layout (mirrors check_frame_ logic)
  if (((function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_1_INIT) &&
       (function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_1_END)) ||
      ((function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_2_INIT) &&
       (function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_2_END))) {
    data_offset = 1;
    data_len = static_cast<uint8_t>(frame_len - 3);  // frame minus addr(1) and CRC(2)
    ESP_LOGD(TAG, "User-defined function %02X found", function_code);
  } else {
    data_offset = 3;
    data_len = raw[2];

    if (this->role == ModbusRole::SERVER) {
      if (function_code == ModbusFunctionCode::READ_COILS ||
          function_code == ModbusFunctionCode::READ_DISCRETE_INPUTS ||
          function_code == ModbusFunctionCode::READ_HOLDING_REGISTERS ||
          function_code == ModbusFunctionCode::READ_INPUT_REGISTERS ||
          function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER) {
        data_offset = 2;
        data_len = 4;
      } else if (function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
        data_offset = 2;
        data_len = 2 + 2 + 1 + raw[6];
      }
    } else {
      if (function_code == ModbusFunctionCode::WRITE_SINGLE_COIL ||
          function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER ||
          function_code == ModbusFunctionCode::WRITE_MULTIPLE_COILS ||
          function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
        data_offset = 2;
        data_len = 4;
      }
    }

    if ((function_code & FUNCTION_CODE_EXCEPTION_MASK) == FUNCTION_CODE_EXCEPTION_MASK) {
      data_offset = 2;
      data_len = 1;
    }
  }

  std::vector<uint8_t> data(raw + data_offset, raw + data_offset + data_len);
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
            if (this->waiting_device_ == nullptr || this->waiting_device_ == device) {
              device->on_modbus_error(function_code & FUNCTION_CODE_MASK, exception);
            }
          } else {
            // Ignore modbus exception not related to a pending command
            ESP_LOGD(TAG, "Ignoring error - not expecting a response from %" PRIu8 "", address);
          }
        } else {  // Not an error response
          if (this->waiting_for_response_ == address) {
            if (this->waiting_device_ == nullptr || this->waiting_device_ == device) {
              device->on_modbus_data(data);
            }
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
    ESP_LOGW(TAG, "Got frame from unknown address %" PRIu8 ", %" PRIu32 "ms after last send", address,
             millis() - this->last_send_);
  }

  if (this->waiting_for_response_ == address) {
    this->waiting_for_response_ = 0;
    this->waiting_device_ = nullptr;
  }
}

void Modbus::send_next_frame_() {
  if (this->tx_buffer_.empty())
    return;

  if (this->post_timeout_quarantine_) {
    // Wait until the wire is idle AND at least a short settling window has passed before
    // sending the next frame. The window is frame_delay_ms_ (guaranteed interframe silence
    // per the Modbus spec) plus a fraction of send_wait_time_ - long enough for a straggler
    // response from the previous command to arrive and be discarded, short enough to avoid
    // halving throughput. A quarter of send_wait_time_ is the balance point we've observed
    // works for JANZ and ZIV E-Redes meters in practice.
    const uint32_t quarantine_window = this->frame_delay_ms_ + this->send_wait_time_ / 4;
    if (this->available() || !this->rx_buffer_.empty() || millis() - this->post_timeout_ts_ < quarantine_window) {
      return;
    }
    this->post_timeout_quarantine_ = false;
  }

  if (this->tx_blocked())
    return;

  const ModbusDeviceCommand &frame = this->tx_buffer_.front();

  if (this->role == ModbusRole::CLIENT) {
    this->waiting_for_response_ = frame.data.get()[0];
    this->waiting_device_ = frame.sender;
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
  ESP_LOGCONFIG(TAG,
                "Modbus:\n"
                "  Send Wait Time: %d ms\n"
                "  Turnaround Time: %d ms\n"
                "  Frame Delay: %d ms\n"
                "  Long Rx Buffer Delay: %d ms\n"
                "  CRC Disabled: %s",
                this->send_wait_time_, this->turnaround_delay_ms_, this->frame_delay_ms_,
                this->long_rx_buffer_delay_ms_, YESNO(this->disable_crc_));
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
}
float Modbus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

void Modbus::send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
                  uint8_t payload_len, const uint8_t *payload, ModbusDevice *sender) {
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

  this->queue_raw_(data, pos, sender);
}

// Helper function for lambdas
// Send raw command. Except CRC everything must be contained in payload
void Modbus::send_raw(const std::vector<uint8_t> &payload, ModbusDevice *sender) {
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

  this->queue_raw_(data, payload.size(), sender);
}

// Assume data and length is valid and append CRC, then queue for sending. Used internally to avoid unnecessary copying
// of data into vectors
void Modbus::queue_raw_(const uint8_t *data, uint16_t len, ModbusDevice *sender) {
  if (this->tx_buffer_.size() < MODBUS_TX_BUFFER_SIZE) {
    this->tx_buffer_.emplace_back(data, len, sender);
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
