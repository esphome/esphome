#include "modbus.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::modbus {

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
  // Receive any available bytes from UART
  this->receive_bytes_();

  // Parse bytes into frames and process them
  this->parse_modbus_frames();
}

void ModbusClientHub::loop() {
  // Call base class to receive bytes and parse frames
  this->Modbus::loop();

  //  If we're past the send_wait_time timeout and response buffer doesn't have the start of the expected response
  if (this->waiting_for_response_.has_value()) {
    ModbusDeviceCommand &wfr = this->waiting_for_response_.value();
    uint8_t expected_address = wfr.frame.data.get()[0];
    if (this->last_receive_check_ - this->last_send_ > this->last_send_tx_offset_ + this->send_wait_time_ &&
        (this->rx_buffer_.empty() || this->rx_buffer_[0] != expected_address)) {
      ESP_LOGW(TAG, "Stop waiting for response from %" PRIu8 " %" PRIu32 "ms after last send", expected_address,
               this->last_receive_check_ - this->last_send_);
      if (wfr.device)
        wfr.device->on_modbus_no_response();
      this->waiting_for_response_.reset();
    }
  }

  //  If there's no response pending and there's commands in the buffer
  this->send_next_frame_();
}

bool Modbus::timeout_() {
  // If the response frame is finished (including interframe delay) - we timeout.
  // The long_rx_buffer_delay accounts for long responses (larger than the UART rx_full_threshold) to avoid timeouts
  // when the buffer is filling the back half of the response
  const uint16_t timeout = std::max(
      (uint16_t) this->frame_delay_ms_,
      (uint16_t) (this->rx_buffer_.size() >= this->parent_->get_rx_full_threshold() ? this->long_rx_buffer_delay_ms_
                                                                                    : 0));

  return this->last_receive_check_ - this->last_modbus_byte_ > timeout;
}

int32_t Modbus::tx_delay_remaining() {
  // We use millis() here and elsewhere instead of App.get_loop_component_start_time() to avoid stale timestamps
  // It's critical in all timestamp comparisons that the left timestamp comes before the right one in time
  // If we use a cached value in place of millis() and last_modbus_byte_ is updated inside our loop
  // then the comparison is backwards (small negative which wraps to large positive) and will cause a false timeout
  // So in this component we don't use any cached timestamp values to avoid these annoying bugs
  const uint32_t now = millis();
  return std::max({(int32_t) 0,
                   (int32_t) (this->last_send_tx_offset_ + this->frame_delay_ms_ - (now - this->last_send_)),
                   (int32_t) (this->frame_delay_ms_ - (now - this->last_modbus_byte_))});
}

int32_t ModbusClientHub::tx_delay_remaining() {
  const uint32_t now = millis();
  // Turnaround delay only applies after a broadcast: no response is expected, so we must give listening devices
  // quiet time to process it before the next request. For normal unicast request/response the received reply already
  // provides the inter-frame timing, so adding turnaround there just throttles throughput.
  const uint16_t turnaround = this->last_send_was_broadcast_ ? this->turnaround_delay_ms_ : 0;
  return std::max(
      {(int32_t) 0,
       (int32_t) (this->last_send_tx_offset_ + this->frame_delay_ms_ + turnaround - (now - this->last_send_)),
       (int32_t) (this->frame_delay_ms_ + turnaround - (now - this->last_modbus_byte_))});
}

bool Modbus::tx_blocked() {
  // We block transmission in any of these cases:
  // 1. There are bytes in the UART Rx buffer
  // 2. There are bytes in our Rx buffer
  // 3. The last sent byte isn't more than tx_delay ms ago (i.e. wait to tell receivers that our previous Tx is done)
  // 4. The last received byte isn't more than tx_delay ms ago (i.e. wait to be sure there isn't more Rx coming)
  // N.B. We allow a small delay (MODBUS_TX_MAX_DELAY_MS) to avoid looping on small delays. This gets handled by
  // send_frame_.
  return this->available() || !this->rx_buffer_.empty() || this->tx_delay_remaining() > MODBUS_TX_MAX_DELAY_MS;
}

bool ModbusClientHub::tx_blocked() {
  // We block transmission in any of these case:
  // 1. We're waiting for a response
  // 2. Any of the base class tx_blocked conditions
  return (this->waiting_for_response_.has_value()) || this->Modbus::tx_blocked();
}

bool ModbusClientHub::tx_buffer_empty() { return this->tx_buffer_.empty(); }

void Modbus::receive_bytes_() {
  this->last_receive_check_ = millis();
  size_t bytes = this->available();

  if (bytes) {
    size_t buffer_size = this->rx_buffer_.size();
    this->last_modbus_byte_ = this->last_receive_check_;
    this->rx_buffer_.resize(buffer_size + bytes);
    if (!this->read_array(this->rx_buffer_.data() + buffer_size, bytes)) {
      this->rx_buffer_.resize(buffer_size);
      return;
    }
    if (buffer_size == 0) {
      ESP_LOGV(TAG, "Received first byte %" PRIu8 " (0X%x) of %zu bytes %" PRIu32 "ms after last send",
               this->rx_buffer_[0], this->rx_buffer_[0], this->rx_buffer_.size(), millis() - this->last_send_);
    }
  }
}

void ModbusClientHub::parse_modbus_frames() {
  if (!this->rx_buffer_.empty()) {
    size_t size;
    do {
      size = this->rx_buffer_.size();
      if (!this->parse_modbus_server_frame_())
        this->clear_rx_buffer_(LOG_STR("parse failed"), true);
    } while (!this->rx_buffer_.empty() && size > this->rx_buffer_.size());
    if (this->timeout_())
      this->clear_rx_buffer_(LOG_STR("timeout after partial response"), true);
  }
}

void ModbusServerHub::parse_modbus_frames() {
  while (!this->rx_buffer_.empty()) {
    size_t size = this->rx_buffer_.size();
    ESP_LOGVV(TAG, "Parsing frames buffer size = %" PRIu32, size);
    bool retry_as_client = false;
    if (this->expecting_peer_response_ != 0) {
      if (!this->parse_modbus_server_frame_()) {
        ESP_LOGV(TAG, "Stop expecting peer response from %" PRIu8 " due to parse failure, and retry parse",
                 this->expecting_peer_response_);
        this->expecting_peer_response_ = 0;
        retry_as_client = true;
      } else if (this->timeout_() && size == this->rx_buffer_.size()) {
        // If we timed out and the above parse attempt did not consume data, stop expecting a response
        ESP_LOGV(TAG,
                 "Stop expecting peer response from %" PRIu8 " due to timeout after partial response, and retry parse",
                 this->expecting_peer_response_);
        this->expecting_peer_response_ = 0;
        retry_as_client = true;
      }
    } else {
      if (!this->parse_modbus_client_frame_())
        this->clear_rx_buffer_(LOG_STR("parse failed"), true);
    }
    // Stop if the buffer didn't shrink (no frame consumed) and no mode switch triggered a retry
    if (!retry_as_client && size <= this->rx_buffer_.size())
      break;
  }
  if (this->timeout_())
    this->clear_rx_buffer_(LOG_STR("timeout after partial response"), true);
}

uint16_t Modbus::find_custom_frame_end_(uint16_t min_length) const {
  // Custom functions could be any length - we have to rely on the CRC to determine completeness.
  // If a CRC match is never found, the buffer will eventually overflow and be cleared.
  const uint8_t *raw = &this->rx_buffer_[0];
  const size_t size = this->rx_buffer_.size();
  for (uint16_t len = min_length; len <= std::min(size, size_t(MAX_FRAME_SIZE)); len++) {
    if (crc16(raw, len) == 0)
      return len;
  }
  return 0;
}

bool Modbus::parse_modbus_server_frame_() {
  size_t size = this->rx_buffer_.size();
  uint16_t frame_length = helpers::server_frame_length(this->rx_buffer_.data(), this->rx_buffer_.size());

  if (size < frame_length)
    return true;

  uint8_t address = this->rx_buffer_[0];
  uint8_t function_code = this->rx_buffer_[1];

  if (helpers::is_function_code_custom(function_code)) {
    frame_length = this->find_custom_frame_end_(frame_length);
    if (frame_length == 0)
      return size < MAX_FRAME_SIZE;  // Continue to parse until we hit max size
    ESP_LOGD(TAG, "User-defined function %02X found", function_code);
  } else {
    if (crc16(&this->rx_buffer_[0], frame_length) != 0)
      return false;
  }

  // Process before clearing: process_modbus_server_frame (receiving a response or peer message) never sends a reply
  // synchronously. We can safely point directly into rx_buffer_ and avoid a copy.
  uint8_t data_offset = helpers::server_frame_data_offset(this->rx_buffer_.data(), this->rx_buffer_.size());
  const uint8_t *data = this->rx_buffer_.data() + data_offset;
  uint16_t data_len = frame_length - 2 - data_offset;

  this->process_modbus_server_frame(address, function_code, data, data_len);
  this->clear_rx_buffer_(LOG_STR("parse succeeded"), false, frame_length);

  return true;
}

bool ModbusServerHub::parse_modbus_client_frame_() {
  size_t size = this->rx_buffer_.size();
  uint16_t frame_length = helpers::client_frame_length(this->rx_buffer_.data(), this->rx_buffer_.size());

  if (size < frame_length)
    return true;

  uint8_t address = this->rx_buffer_[0];
  uint8_t function_code = this->rx_buffer_[1];

  if (helpers::is_function_code_custom(function_code)) {
    frame_length = this->find_custom_frame_end_(frame_length);
    if (frame_length == 0)
      return size < MAX_FRAME_SIZE;  // Continue to parse until we hit max size
    ESP_LOGD(TAG, "User-defined function %02X found", function_code);
  } else {
    if (crc16(&this->rx_buffer_[0], frame_length) != 0)
      return false;
  }

  // Clear before processing: process_modbus_client_frame_ dispatches to a server device which sends
  // a response immediately. We need to clear the rx buffer first so the response doesn't snag tx_blocked.
  // This requires copying the frame data to a local buffer beforehand.
  uint8_t data_offset = helpers::client_frame_data_offset(this->rx_buffer_.data(), this->rx_buffer_.size());
  uint16_t data_len = frame_length - 2 - data_offset;
  uint8_t data[MAX_FRAME_SIZE] = {};
  std::memcpy(data, this->rx_buffer_.data() + data_offset, data_len);
  this->clear_rx_buffer_(LOG_STR("parse succeeded"), false, frame_length);

  this->process_modbus_client_frame_(address, function_code, data, data_len);

  return true;
}

void ModbusClientHub::process_modbus_server_frame(uint8_t address, uint8_t function_code, const uint8_t *data,
                                                  uint16_t len) {
  if (!this->waiting_for_response_.has_value()) {
    ESP_LOGW(TAG,
             "Received unexpected frame from address %" PRIu8 ", function code 0x%X, %" PRIu32 "ms after last send",
             address, function_code, this->last_modbus_byte_ - this->last_send_);
    return;
  } else {  // We are waiting for a response
    // Check if the response matches the expected address and function code

    ModbusDeviceCommand &wfr = this->waiting_for_response_.value();
    uint8_t expected_address = wfr.frame.data.get()[0];
    uint8_t expected_function_code = wfr.frame.data.get()[1];
    if (expected_address != address || expected_function_code != (function_code & FUNCTION_CODE_MASK)) {
      ESP_LOGW(TAG,
               "Received incorrect frame address %" PRIu8 " <> %" PRIu8 " or function code 0x%X <> 0x%X, %" PRIu32
               "ms after last send",
               address, expected_address, (function_code & FUNCTION_CODE_MASK), expected_function_code,
               this->last_modbus_byte_ - this->last_send_);
      // Invalidate the waiting device so it won't process this response.
      if (wfr.device)
        wfr.device->on_modbus_no_response();
      wfr.interrupted = true;
      wfr.device = nullptr;
      return;
    }

    if (wfr.interrupted) {
      ESP_LOGW(TAG,
               "Ignoring response from %" PRIu8 " - transmission interrupted by previous unexpected response, %" PRIu32
               "ms after last send",
               address, this->last_modbus_byte_ - this->last_send_);
      return;
    } else {  // We have a valid device waiting for this response

      ModbusClientDevice *device = wfr.device;
      this->waiting_for_response_.reset();
      // Is it an error response?
      if (helpers::is_function_code_exception(function_code)) {
        uint8_t exception = len > 0 ? data[0] : 0;
        ESP_LOGW(TAG,
                 "Error function code: 0x%X exception: %" PRIu8 ", address: %" PRIu8 ", %" PRIu32 "ms after last send",
                 function_code, exception, address, this->last_modbus_byte_ - this->last_send_);
        if (device)
          device->on_modbus_error(function_code & FUNCTION_CODE_MASK, exception);

      } else if (device) {  // Not an error response
        // on_modbus_data is existing public API taking const std::vector<uint8_t>&
        device->on_modbus_data(std::vector<uint8_t>(data, data + len));
      } else {  // Not an error response, but no device to respond to
        ESP_LOGV(TAG, "Ignoring response from %" PRIu8 " - no callback device set, %" PRIu32 "ms after last send",
                 address, this->last_modbus_byte_ - this->last_send_);
      }
    }
  }
}

void ModbusServerHub::process_modbus_server_frame(uint8_t address, uint8_t function_code, const uint8_t *, uint16_t) {
  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      ESP_LOGE(TAG, "Unexpected response from address %" PRIu8 ", which is mapped to this device.", address);
    }
  }

  if (this->expecting_peer_response_ == address) {
    ESP_LOGV(TAG, "Expected response from peer %" PRIu8 " received", address);
  } else {
    ESP_LOGV(TAG, "Unexpected response from peer %" PRIu8 " received", address);
  }

  // This always resets, even if the address doesn't match.
  // If an unexpected response is received, we can't trust that a correct response will follow (it shouldn't).
  this->expecting_peer_response_ = 0;
}

void ModbusServerHub::process_modbus_client_frame_(uint8_t address, uint8_t function_code, const uint8_t *data,
                                                   uint16_t len) {
  bool found = false;

  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      found = true;

      if (static_cast<ModbusFunctionCode>(function_code) == ModbusFunctionCode::READ_HOLDING_REGISTERS ||
          static_cast<ModbusFunctionCode>(function_code) == ModbusFunctionCode::READ_INPUT_REGISTERS) {
        device->on_modbus_read_registers(function_code, helpers::get_data<uint16_t>(data, 0),
                                         helpers::get_data<uint16_t>(data, 2));
      } else if (static_cast<ModbusFunctionCode>(function_code) == ModbusFunctionCode::WRITE_SINGLE_REGISTER ||
                 static_cast<ModbusFunctionCode>(function_code) == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
        device->on_modbus_write_registers(function_code, std::vector<uint8_t>(data, data + len));
      } else {
        ESP_LOGW(TAG, "Unsupported function code %" PRIu8, function_code);
        device->send_error(function_code, ModbusExceptionCode::ILLEGAL_FUNCTION);
      }
    }
  }

  if (!found) {
    this->expecting_peer_response_ = address;
    ESP_LOGV(TAG, "Request to peer %" PRIu8 " received", address);
  }
}

bool Modbus::send_frame_(const ModbusFrame &frame) {
  if (this->tx_blocked()) {
    ESP_LOGE(TAG, "Attempted to send while transmission blocked");
    return false;
  }
  if (frame.size > MAX_FRAME_SIZE) {
    ESP_LOGE(TAG, "Attempted to send frame larger than max frame size of %" PRIu16 " bytes", MAX_FRAME_SIZE);
    return false;
  }

  const int32_t tx_delay_remaining = this->tx_delay_remaining();
  if (tx_delay_remaining > 0) {
    delay(tx_delay_remaining);
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

  uint32_t now = millis();
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
  ESP_LOGV(TAG, "Write: %s %" PRIu32 "ms after last send, %" PRIu32 "ms after last receive",
           format_hex_pretty_to(hex_buf, frame.data.get(), frame.size), now - this->last_send_,
           now - this->last_modbus_byte_);
  this->last_send_ = now;
  this->last_send_was_broadcast_ = frame.size > 0 && frame.data[0] == 0;
  return true;
}

void ModbusClientHub::send_next_frame_() {
  if (this->tx_buffer_.empty()) {
    return;
  }

  if (this->tx_blocked()) {
    return;
  }

  ModbusDeviceCommand &command = this->tx_buffer_.front();

  if (this->send_frame_(command.frame)) {
    if (!this->last_send_was_broadcast_)
      this->waiting_for_response_ = std::move(command);
  } else {
    if (command.device)
      command.device->on_modbus_not_sent();
  }

  this->tx_buffer_.pop_front();

  if (!this->tx_buffer_.empty()) {
    ESP_LOGV(TAG, "Write queue contains %zu items.", this->tx_buffer_.size());
  }
}

void ModbusClientHub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Modbus:\n"
                "  Send Wait Time: %" PRIu16 " ms\n"
                "  Turnaround Time: %" PRIu16 " ms\n"
                "  Frame Delay: %" PRIu16 " ms\n"
                "  Long Rx Buffer Delay: %" PRIu16 " ms",
                this->send_wait_time_, this->turnaround_delay_ms_, this->frame_delay_ms_,
                this->long_rx_buffer_delay_ms_);
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
}
void ModbusServerHub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Modbus:\n"
                "  Frame Delay: %" PRIu16 " ms\n"
                "  Long Rx Buffer Delay: %" PRIu16 " ms",
                this->frame_delay_ms_, this->long_rx_buffer_delay_ms_);
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
}

float Modbus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

void ModbusServerHub::send(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &payload) {
  const uint16_t len = static_cast<uint16_t>(2 + payload.size());
  if (len > MAX_RAW_SIZE) {
    ESP_LOGE(TAG, "Server send frame too large (%" PRIu16 " bytes)", len);
    return;
  }
  uint8_t raw_frame[MAX_RAW_SIZE];
  raw_frame[0] = address;
  raw_frame[1] = function_code;
  std::memcpy(raw_frame + 2, payload.data(), payload.size());
  this->send_raw_(raw_frame, len);
}

// Raw send for client: pushes to tx queue. Everything except the CRC must be contained in payload.
void ModbusClientHub::queue_raw_(uint8_t address, const uint8_t *pdu, uint16_t pdu_len, ModbusClientDevice *device) {
  if (pdu_len == 0) {
    if (device)
      device->on_modbus_not_sent();
    return;
  }

  if (this->tx_buffer_.size() < MODBUS_TX_BUFFER_SIZE) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
    ESP_LOGV(TAG, "Adding frame to tx queue: %" PRIu8 ":%s", address, format_hex_pretty_to(hex_buf, pdu, pdu_len));
    this->tx_buffer_.emplace_back(device, address, pdu, pdu_len);
  } else {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
    char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
    ESP_LOGE(TAG, "Write buffer full, dropped: %" PRIu8 ":%s", address, format_hex_pretty_to(hex_buf, pdu, pdu_len));
    if (device)
      device->on_modbus_not_sent();
  }
}

void ModbusClientHub::clear_tx_queue_for_address(uint8_t address, bool clear_sent) {
  // Remove any pending commands for this address from the tx buffer
  auto &tx_buffer = this->tx_buffer_;
  tx_buffer.erase(std::remove_if(tx_buffer.begin(), tx_buffer.end(),
                                 [address](const ModbusDeviceCommand &cmd) { return cmd.frame.data[0] == address; }),
                  tx_buffer.end());

  if (clear_sent && this->waiting_for_response_.has_value() && this->waiting_for_response_.value().device) {
    if (this->waiting_for_response_.value().frame.data[0] == address) {
      ESP_LOGV(TAG, "Clearing waiting for response for address %" PRIu8, address);
      // Invalidate the waiting device so it won't process a response.
      this->waiting_for_response_.value().device = nullptr;
    }
  }
}
void ModbusClientHub::clear_tx_queue_for_device(ModbusClientDevice *device) {
  // Remove any pending commands for this address from the tx buffer
  auto &tx_buffer = this->tx_buffer_;
  tx_buffer.erase(std::remove_if(tx_buffer.begin(), tx_buffer.end(),
                                 [device](const ModbusDeviceCommand &cmd) { return cmd.device == device; }),
                  tx_buffer.end());

  if (this->waiting_for_response_.has_value() && this->waiting_for_response_.value().device) {
    if (this->waiting_for_response_.value().device == device) {
      ESP_LOGV(TAG, "Clearing waiting for response");
      // Invalidate the waiting device so it won't process a response.
      this->waiting_for_response_.value().device = nullptr;
    }
  }
}

void ModbusClientHub::send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device) {
  if (payload.size() < 2) {
    if (device)
      device->on_modbus_not_sent();
    return;
  }
  this->queue_raw_(payload[0], payload.data() + 1, static_cast<uint16_t>(payload.size() - 1), device);
}

// Send raw command for server replies immediately. Except CRC everything must be contained in payload
void ModbusServerHub::send_raw_(const uint8_t *payload, uint16_t len) {
  if (len == 0) {
    return;
  }
  if (len > MAX_RAW_SIZE) {
    ESP_LOGE(TAG, "Server send frame too large (%" PRIu16 " bytes)", len);
    return;
  }

  // In the rare case that the server is blocked (frame delay has not elapsed), we delay the send.
  // This should only happen at low baud rates with long frame delays.
  if (this->tx_blocked()) {
    // Stash the raw payload in a single member buffer so the deferred callback can rebuild the frame
    // without a heap allocation. Only one server reply is ever in flight, and the named timeout ensures
    // only one deferred send is pending, so a single buffer is sufficient.
    std::memcpy(this->deferred_payload_.data(), payload, len);
    this->deferred_payload_len_ = len;
    this->set_timeout("deferred_send", this->tx_delay_remaining(), [this]() {
      ModbusFrame frame(this->deferred_payload_[0], this->deferred_payload_.data() + 1,
                        this->deferred_payload_len_ - 1);
      this->send_frame_(frame);
    });
  } else {
    ModbusFrame frame(payload[0], payload + 1, len - 1);
    this->send_frame_(frame);
  }
}

void Modbus::clear_rx_buffer_(const LogString *reason, bool warn, size_t bytes_to_clear) {
  size_t bytes = this->rx_buffer_.size();
  if (bytes_to_clear > 0 && bytes >= bytes_to_clear)
    bytes = bytes_to_clear;
  if (bytes > 0) {
    if (warn) {
      ESP_LOGW(TAG, "Clearing buffer of %zu bytes - %s %" PRIu32 "ms after last send", bytes, LOG_STR_ARG(reason),
               millis() - this->last_send_);
    } else {
      ESP_LOGV(TAG, "Clearing buffer of %zu bytes - %s %" PRIu32 "ms after last send", bytes, LOG_STR_ARG(reason),
               millis() - this->last_send_);
    }
    if (bytes == this->rx_buffer_.size()) {
      this->rx_buffer_.clear();
    } else {
      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + bytes);
    }
  }
}

}  // namespace esphome::modbus
