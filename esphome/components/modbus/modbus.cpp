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

  // Send-wait watchdog: past the timeout with no start-of-response in sight, the wait ends.
  // WAITING becomes TIMED_OUT (the sweep delivers on_no_response()); a WAITING_DELETED shell
  // retires silently; an INTERRUPTED shell releases here, applying the retry decision the sweep
  // recorded when it delivered the interruption's on_no_response(). Only the cheap time check
  // runs at loop rate; expire_waiting_() looks the entry up and holds off if the response has
  // started arriving.
  if (this->waiting_for_response_ &&
      this->last_receive_check_ - this->last_send_ > this->last_send_tx_offset_ + this->send_wait_time_) {
    this->expire_waiting_();
  }

  this->sweep_();  // deliver owed callbacks with the hub quiescent
  // Then transmit - possibly a frame the sweep or watchdog just returned to READY.
  this->send_next_frame_();
}

void ModbusClientHub::expire_waiting_() {
  ModbusDeviceCommand *cmd = this->find_waiting_();
  if (cmd == nullptr) {
    this->waiting_for_response_ = false;
  } else if (!this->rx_buffer_.empty() && this->rx_buffer_[0] == cmd->frame.address()) {
    // The start of the response is in the buffer: let the frame finish arriving.
  } else if (cmd->state == FrameState::WAITING) {
    ESP_LOGW(TAG, "Stop waiting for response from %" PRIu8 " %" PRIu32 "ms after last send", cmd->frame.address(),
             this->last_receive_check_ - this->last_send_);
    cmd->state = FrameState::TIMED_OUT;
    this->sweep_needed_ = true;
    this->waiting_for_response_ = false;
  } else if (cmd->state == FrameState::WAITING_DELETED) {
    retire_(*cmd);
    this->sweep_needed_ = true;
    this->waiting_for_response_ = false;
  } else if (cmd->state == FrameState::INTERRUPTED_NOTIFIED) {  // retry decision recorded
    if (cmd->retry_after_interrupt || --cmd->pending != 0) {
      // Wire-equivalent to the old requeue-behind-the-shell: the retry (or an absorbed request's
      // run) becomes sendable exactly when the shell stops blocking, re-stamped to the tail.
      cmd->state = FrameState::READY;
      cmd->seq = this->next_seq_++;
      cmd->retry_after_interrupt = false;
    } else {
      retire_(*cmd);
      this->sweep_needed_ = true;
    }
    this->waiting_for_response_ = false;
  }
  // else: INTERRUPTED but the sweep hasn't delivered on_no_response() yet; release next loop.
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
  return std::max({(int32_t) 0,
                   (int32_t) (this->last_send_tx_offset_ + this->frame_delay_ms_ + this->turnaround_delay_ms_ -
                              (now - this->last_send_)),
                   (int32_t) (this->frame_delay_ms_ + this->turnaround_delay_ms_ - (now - this->last_modbus_byte_))});
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
  // 1. We're waiting for a response (a WAITING/INTERRUPTED/WAITING_DELETED entry)
  // 2. Any of the base class tx_blocked conditions
  return this->waiting_for_response_ || this->Modbus::tx_blocked();
}

bool ModbusClientHub::tx_buffer_empty() {
  // Empty means nothing is awaiting transmission; entries in other states are mid-transaction or
  // owed bookkeeping, not queued sends.
  for (const auto &cmd : this->tx_buffer_) {
    if (cmd.state == FrameState::READY)
      return false;
  }
  return true;
}

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
  // The PDU is the frame without the leading address and the trailing CRC.
  std::span<const uint8_t> pdu(this->rx_buffer_.data() + 1, frame_length - 3);

  this->process_modbus_server_frame(address, pdu);
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

  this->process_modbus_client_frame_(address, function_code, data);

  return true;
}

// Bounds contract, enforced by the parser (parse_modbus_server_frame_) rather than locally:
// - pdu is never empty: helpers::server_pdu_length() returns at least MIN_PDU_SIZE (1) on every
//   branch, and find_custom_frame_end_() only ever lengthens the frame, so the PDU always holds
//   at least the function code.
// - When the exception bit is set, pdu has at least 2 bytes: server_pdu_length() checks the
//   exception bit before anything else and pins those PDUs to 2 bytes, so the exception code
//   read below is always present.
// Keep those guarantees in mind when changing server_pdu_length() or adding callers.
void ModbusClientHub::process_modbus_server_frame(uint8_t address, std::span<const uint8_t> pdu) {
  const uint8_t function_code = pdu[0];
  ModbusDeviceCommand *cmd = this->waiting_for_response_ ? this->find_waiting_() : nullptr;
  if (cmd == nullptr) {
    ESP_LOGW(TAG,
             "Received unexpected frame from address %" PRIu8 ", function code 0x%X, %" PRIu32 "ms after last send",
             address, function_code, this->last_modbus_byte_ - this->last_send_);
    return;
  }

  // Check if the response matches the expected address and function code
  const uint8_t expected_address = cmd->frame.address();
  const uint8_t expected_function_code = cmd->frame.pdu()[0];
  if (expected_address != address || expected_function_code != (function_code & FUNCTION_CODE_MASK)) {
    ESP_LOGW(TAG,
             "Received incorrect frame address %" PRIu8 " <> %" PRIu8 " or function code 0x%X <> 0x%X, %" PRIu32
             "ms after last send",
             address, expected_address, (function_code & FUNCTION_CODE_MASK), expected_function_code,
             this->last_modbus_byte_ - this->last_send_);
    // The transaction is interrupted: the entry becomes an INTERRUPTED shell that keeps tx blocked
    // until the send-wait timeout. The sweep delivers on_no_response() and records the retry
    // decision (-> INTERRUPTED_NOTIFIED); the timeout applies it. A shell already past WAITING
    // (interrupted or cleared) just keeps waiting out the clock.
    if (cmd->state == FrameState::WAITING) {
      cmd->state = FrameState::INTERRUPTED;
      this->sweep_needed_ = true;
    }
    return;
  }

  if (cmd->state == FrameState::WAITING_DELETED) {
    // The cleared frame's response arrived: the shell is spent, unblock silently.
    retire_(*cmd);
    this->waiting_for_response_ = false;
    this->sweep_needed_ = true;
    return;
  }
  if (cmd->state != FrameState::WAITING) {  // an INTERRUPTED shell: its transaction already ended
    ESP_LOGW(TAG,
             "Ignoring response from %" PRIu8 " - transmission interrupted by previous unexpected response, %" PRIu32
             "ms after last send",
             address, this->last_modbus_byte_ - this->last_send_);
    return;
  }

  // A valid response for the WAITING entry. The callbacks fire AT PARSE TIME so the response span
  // can point straight into the rx buffer (zero copy); the sweep does the lifecycle bookkeeping
  // afterwards. The RECEIVED_* state is set BEFORE the callbacks run: a clear_tx_queue_*() call
  // from inside them flips the entry to DELETED, which cancels the re-run/continuous bookkeeping -
  // "stop polling now" from inside on_response()/on_error() must actually stop it.
  this->waiting_for_response_ = false;
  this->sweep_needed_ = true;
  ModbusClientDevice *device = cmd->device;
  // The request PDU is the sent frame without the leading address and the trailing CRC.
  std::span<const uint8_t> request_pdu = cmd->frame.pdu();
  if (helpers::is_function_code_exception(function_code)) {
    uint8_t exception = pdu[1];  // exception frames are fixed-length, so the code is always present
    ESP_LOGW(TAG, "Error function code: 0x%X exception: %" PRIu8 ", address: %" PRIu8 ", %" PRIu32 "ms after last send",
             function_code, exception, address, this->last_modbus_byte_ - this->last_send_);
    cmd->state = FrameState::RECEIVED_EXCEPTION;
    if (device != nullptr)
      device->on_error(request_pdu, static_cast<ExceptionCode>(exception));
  } else {
    cmd->state = FrameState::RECEIVED_RESPONSE;
    if (device != nullptr) {
      device->on_response(request_pdu, pdu);
    } else {
      ESP_LOGV(TAG, "Ignoring response from %" PRIu8 " - no callback device set, %" PRIu32 "ms after last send",
               address, this->last_modbus_byte_ - this->last_send_);
    }
  }
}

void ModbusServerHub::process_modbus_server_frame(uint8_t address, std::span<const uint8_t>) {
  if (this->find_device_(address) != nullptr) {
    ESP_LOGE(TAG, "Unexpected response from address %" PRIu8 ", which is mapped to this device.", address);
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

ModbusServerDevice *ModbusServerHub::find_device_(uint8_t address) {
  for (auto *device : this->devices_) {
    if (device->get_address() == address) {
      return device;
    }
  }
  return nullptr;
}

bool ModbusServerHub::check_register_range_(uint8_t address, uint8_t function_code, uint16_t start_address,
                                            uint16_t number_of_registers) {
  if ((uint32_t) start_address + number_of_registers > 0x10000u) {
    ESP_LOGW(TAG, "Register address out of range - start: %" PRIu16 " num: %" PRIu16, start_address,
             number_of_registers);
    this->send_exception_(address, function_code, ExceptionCode::ILLEGAL_DATA_ADDRESS);
    return false;
  }
  return true;
}

void ModbusServerHub::process_modbus_client_frame_(uint8_t address, uint8_t function_code, const uint8_t *data) {
  ModbusServerDevice *device = this->find_device_(address);
  if (device == nullptr) {
    this->expecting_peer_response_ = address;
    ESP_LOGV(TAG, "Request to peer %" PRIu8 " received", address);
    return;
  }

  ResponseStatus status;
  uint8_t response_buffer[modbus::MAX_RAW_SIZE];
  const uint8_t *response_data = response_buffer;
  uint16_t response_len = 0;

  switch (static_cast<FunctionCode>(function_code)) {
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS: {
      // PDU data: start address(2) + quantity(2).
      uint16_t start_address = helpers::get_data<uint16_t>(data, 0);
      uint16_t number_of_registers = helpers::get_data<uint16_t>(data, 2);
      if (number_of_registers == 0 || number_of_registers > MAX_NUM_OF_REGISTERS_TO_READ) {
        ESP_LOGW(TAG, "Invalid number of registers %" PRIu16, number_of_registers);
        this->send_exception_(address, function_code, ExceptionCode::ILLEGAL_DATA_VALUE);
        return;
      }
      if (!this->check_register_range_(address, function_code, start_address, number_of_registers)) {
        return;
      }
      RegisterValues registers;
      if (static_cast<FunctionCode>(function_code) == FunctionCode::READ_HOLDING_REGISTERS) {
        status = device->on_read_holding_registers(start_address, number_of_registers, registers);
      } else {
        status = device->on_read_input_registers(start_address, number_of_registers, registers);
      }

      // A handler that returns an exception leaves registers partially filled, so check the exception
      // first and forward it before validating the register count on the success path.
      if (status.has_value()) {
        this->send_exception_(address, function_code, status.value());
        return;
      }

      if (registers.size() != number_of_registers) {
        ESP_LOGE(TAG, "Incorrect response %" PRIu16 " requested, %zu returned", number_of_registers, registers.size());
        this->send_exception_(address, function_code, ExceptionCode::SERVICE_DEVICE_FAILURE);
        return;
      }

      response_buffer[response_len++] = static_cast<uint8_t>(number_of_registers * 2);  // actual byte count
      for (auto r : registers) {
        auto register_bytes = decode_value(r);
        response_buffer[response_len++] = register_bytes[0];
        response_buffer[response_len++] = register_bytes[1];
      }
      break;
    }
    case FunctionCode::WRITE_SINGLE_REGISTER:
    case FunctionCode::WRITE_MULTIPLE_REGISTERS: {
      // PDU data: start address(2) [+ quantity(2) + byte count(1)] + register values.
      // A single-register write always targets one register; for a multiple-register write the
      // quantity is in the frame and its byte count must equal quantity * 2. The register values are
      // assembled into registers below so the handler doesn't have to know the request framing.
      uint16_t start_address = helpers::get_data<uint16_t>(data, 0);
      uint16_t number_of_registers = 1;
      uint16_t values_offset = 2;  // single write: values follow the 2-byte start address
      if (static_cast<FunctionCode>(function_code) == FunctionCode::WRITE_MULTIPLE_REGISTERS) {
        number_of_registers = helpers::get_data<uint16_t>(data, 2);
        uint8_t number_of_bytes = helpers::get_data<uint8_t>(data, 4);
        values_offset = 5;  // multiple write: values follow start address(2) + quantity(2) + byte count(1)
        if (number_of_registers == 0 || number_of_registers > MAX_NUM_OF_REGISTERS_TO_WRITE ||
            number_of_registers * 2 != number_of_bytes) {
          ESP_LOGW(TAG, "Invalid number of registers %" PRIu16 " or bytes %" PRIu8, number_of_registers,
                   number_of_bytes);
          this->send_exception_(address, function_code, ExceptionCode::ILLEGAL_DATA_VALUE);
          return;
        }
        if (!this->check_register_range_(address, function_code, start_address, number_of_registers)) {
          return;
        }
      }
      // Assemble the register values (host byte order) so the handler never sees wire framing.
      RegisterValues registers;
      for (uint16_t i = 0; i < number_of_registers; i++) {
        registers.push_back(helpers::get_data<uint16_t>(data, values_offset + i * 2));
      }
      status = device->on_write_registers(start_address, registers);
      response_data = data;  // echo the request header per Modbus 6.6, 6.12
      response_len = 4;
      break;
    }
    default:
      ESP_LOGW(TAG, "Unsupported function code %" PRIu8, function_code);
      this->send_exception_(address, function_code, ExceptionCode::ILLEGAL_FUNCTION);
      return;
  }
  if (status.has_value()) {
    this->send_exception_(address, function_code, status.value());
  } else {
    this->send_response_(address, function_code, response_data, response_len);
  }
}

bool Modbus::send_frame_(const ModbusFrame &frame) {
  if (this->tx_blocked()) {
    ESP_LOGE(TAG, "Attempted to send while transmission blocked");
    return false;
  }
  if (frame.size() > MAX_FRAME_SIZE) {
    ESP_LOGE(TAG, "Attempted to send frame larger than max frame size of %" PRIu16 " bytes", MAX_FRAME_SIZE);
    return false;
  }

  const int32_t tx_delay_remaining = this->tx_delay_remaining();
  if (tx_delay_remaining > 0) {
    delay(tx_delay_remaining);
  }

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
    this->write_array(frame.data.data(), frame.size());
    this->flush();
    this->flow_control_pin_->digital_write(false);
    this->last_send_tx_offset_ = 0;
  } else {
    this->write_array(frame.data.data(), frame.size());
    this->last_send_tx_offset_ = frame.size() * MODBUS_BITS_PER_CHAR * MS_PER_SEC / this->parent_->get_baud_rate() + 1;
  }

  uint32_t now = millis();
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
  ESP_LOGV(TAG, "Write: %s %" PRIu32 "ms after last send, %" PRIu32 "ms after last receive",
           format_hex_pretty_to(hex_buf, frame.data.data(), frame.size()), now - this->last_send_,
           now - this->last_modbus_byte_);
  this->last_send_ = now;
  return true;
}

void ModbusClientHub::send_next_frame_() {
  if (this->tx_blocked())
    return;

  ModbusDeviceCommand *cmd = this->select_next_ready_();
  if (cmd == nullptr)
    return;

  if (this->send_frame_(cmd->frame)) {
    cmd->state = FrameState::WAITING;
    this->waiting_for_response_ = true;
    if (cmd->device != nullptr)
      cmd->device->on_sent(cmd->frame.pdu());
  } else {
    // Transmit failure: the failed attempt owes exactly one on_not_sent(), delivered by the sweep;
    // remaining pending (an absorbed duplicate) then returns the entry to READY for its own run.
    cmd->state = FrameState::REFUSED;
    this->sweep_needed_ = true;
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

void ModbusServerHub::send_response_(uint8_t address, uint8_t function_code, const uint8_t *payload,
                                     uint16_t payload_len) {
  // Build the raw frame (address + function code + payload) in a stack buffer; it's consumed
  // immediately by send_raw_ and a full raw frame never exceeds MAX_RAW_SIZE.
  if (payload_len + 2 > MAX_RAW_SIZE) {
    ESP_LOGE(TAG, "Server response too large (%" PRIu16 " bytes)", static_cast<uint16_t>(payload_len + 2));
    return;
  }
  uint8_t raw_frame[MAX_RAW_SIZE];
  raw_frame[0] = address;
  raw_frame[1] = function_code;
  std::memcpy(raw_frame + 2, payload, payload_len);
  this->send_raw_(raw_frame, payload_len + 2);
}

void ModbusServerHub::send_exception_(uint8_t address, uint8_t function_code, ExceptionCode exception_code) {
  uint8_t raw_frame[3];
  raw_frame[0] = address;
  raw_frame[1] = function_code | FUNCTION_CODE_EXCEPTION_MASK;
  raw_frame[2] = static_cast<uint8_t>(exception_code);
  this->send_raw_(raw_frame, 3);
}

ModbusDeviceCommand *ModbusClientHub::find_waiting_() {
  for (auto &cmd : this->tx_buffer_) {
    switch (cmd.state) {
      case FrameState::WAITING:
      case FrameState::INTERRUPTED:
      case FrameState::INTERRUPTED_NOTIFIED:
      case FrameState::WAITING_DELETED:
        return &cmd;
      default:
        break;
    }
  }
  return nullptr;
}

ModbusDeviceCommand *ModbusClientHub::select_next_ready_() {
  // Ordering is a property of selection, not storage: WRITE class outranks reads, one-shot reads
  // outrank continuous polls, and within a group the oldest seq goes first - round-robin fair, with
  // absorbed duplicates keeping their entry's place (see the seq field doc).
  const auto older = [](const ModbusDeviceCommand &a, const ModbusDeviceCommand &b) {
    return static_cast<int16_t>(a.seq - b.seq) < 0;  // wrap-safe
  };
  ModbusDeviceCommand *best = nullptr;
  for (auto &cmd : this->tx_buffer_) {
    if (cmd.state != FrameState::READY)
      continue;
    if (best == nullptr || cmd.priority > best->priority ||
        (cmd.priority == best->priority &&
         (best->continuous != cmd.continuous ? !cmd.continuous : older(cmd, *best)))) {
      best = &cmd;
    }
  }
  return best;
}

size_t ModbusClientHub::live_count_() const {
  // Entries counted against the transmit-queue cap: REFUSED entries live in the reserve, and
  // *_DELETED entries are already resolved (awaiting the sweep's erase pass).
  size_t count = 0;
  for (const auto &cmd : this->tx_buffer_) {
    switch (cmd.state) {
      case FrameState::REFUSED:
      case FrameState::DELETED:
      case FrameState::WAITING_DELETED:
        break;
      default:
        count++;
    }
  }
  return count;
}

void ModbusClientHub::refuse_(ModbusClientDevice *device, uint8_t address, std::span<const uint8_t> pdu) {
  if (device == nullptr)
    return;  // no owner to notify; the caller already logged the drop
  // One refusal delivery per device per sweep: a handler that reacts to its own on_not_sent() with
  // another doomed send (directly or through a device cycle) must not livelock the sweep.
  for (const auto &cmd : this->tx_buffer_) {
    if (cmd.served_not_sent && cmd.device == device) {
      ESP_LOGV(TAG, "Suppressing repeated refusal for address %" PRIu8 " within one sweep", address);
      return;
    }
  }
  size_t refused = 0;
  for (const auto &cmd : this->tx_buffer_) {
    if (cmd.state == FrameState::REFUSED)
      refused++;
  }
  if (refused >= MODBUS_REFUSED_RESERVE) {
    ESP_LOGE(TAG, "Refusal reserve full, dropping on_not_sent for address %" PRIu8, address);
    return;
  }
  auto &cmd = this->tx_buffer_.emplace_back(device, address, pdu);
  cmd.state = FrameState::REFUSED;
  this->sweep_needed_ = true;
}

uint8_t ModbusClientHub::servable_cap_(const ModbusDeviceCommand &cmd) {
  // A requeueable one-shot read absorbs one extra request (this run + one re-run); everything
  // else - writes, custom codes, continuous polls - serves exactly one.
  const uint8_t fc = cmd.frame.pdu()[0];
  const bool requeueable = !helpers::is_function_code_exception(fc) && helpers::is_function_code_read(fc);
  return (requeueable && !cmd.continuous) ? 2 : 1;
}

void ModbusClientHub::sweep_() {
  if (!this->sweep_needed_)
    return;
  this->sweep_needed_ = false;
  // Service loop: handle ONE entry per pass, then rescan - a handler may flip any entry's state
  // (send, clear), so no iterator survives a callback. Indices and references DO survive: entries
  // only ever leave the container in the erase pass below, and deque appends invalidate neither.
  // Termination: every service either decrements a pending count or moves the entry along the
  // transition table toward READY/removal; nothing a handler can do re-creates swept work except
  // an explicit new send, which starts with pending within its cap.
  bool progressed = true;
  while (progressed) {
    progressed = false;
    for (size_t i = 0; i != this->tx_buffer_.size() && !progressed; i++) {
      ModbusDeviceCommand &cmd = this->tx_buffer_[i];
      switch (cmd.state) {
        case FrameState::RECEIVED_RESPONSE: {
          // Callbacks fired at parse time; this is pure lifecycle bookkeeping.
          if (cmd.continuous) {
            cmd.state = FrameState::READY;  // the subscription's next cycle
            cmd.seq = this->next_seq_++;
          } else if (--cmd.pending == 0) {
            retire_(cmd);  // the one-shot consumed its last request
          } else {
            cmd.state = FrameState::READY;  // an absorbed duplicate still owes a run
            cmd.seq = this->next_seq_++;
          }
          progressed = true;
          break;
        }
        case FrameState::RECEIVED_EXCEPTION: {
          // An exception ends a continuous poll; one-shot consumption matches the response path.
          if (cmd.continuous || --cmd.pending == 0) {
            retire_(cmd);
          } else {
            cmd.state = FrameState::READY;
            cmd.seq = this->next_seq_++;
          }
          progressed = true;
          break;
        }
        case FrameState::TIMED_OUT: {
          bool retry = false;
          if (cmd.device != nullptr)
            retry = cmd.device->on_no_response(cmd.frame.pdu());
          if (cmd.state != FrameState::TIMED_OUT) {
            // A clear from inside the handler took over; its state wins (and cancels the retry).
          } else if (retry) {
            // A granted retry is resolve-then-re-request: the new attempt queues at the tail.
            cmd.state = FrameState::READY;
            cmd.seq = this->next_seq_++;
          } else if (!cmd.continuous && --cmd.pending != 0) {
            cmd.state = FrameState::READY;  // one request resolved; the survivor re-stamps
            cmd.seq = this->next_seq_++;
          } else {
            retire_(cmd);
          }
          progressed = true;
          break;
        }
        case FrameState::INTERRUPTED: {
          // Advance BEFORE the callback, so a clear from inside it (-> WAITING_DELETED) wins and
          // is never overwritten; the decision recorded below is then moot.
          cmd.state = FrameState::INTERRUPTED_NOTIFIED;
          bool retry = false;
          if (cmd.device != nullptr)
            retry = cmd.device->on_no_response(cmd.frame.pdu());
          cmd.retry_after_interrupt = retry;  // applied when the timeout releases the shell
          progressed = true;
          break;
        }
        case FrameState::REFUSED: {
          cmd.served_not_sent = true;
          if (cmd.device != nullptr)
            cmd.device->trigger_not_sent(cmd.frame.pdu());
          if (cmd.state == FrameState::REFUSED) {  // a clear from inside the handler wins otherwise
            if (!cmd.continuous && --cmd.pending != 0) {
              // A transmit failure's refusal resolved one request; the absorbed one gets its run.
              cmd.state = FrameState::READY;
              cmd.seq = this->next_seq_++;
            } else {
              retire_(cmd);
            }
          }
          progressed = true;
          break;
        }
        case FrameState::DELETED: {
          if (cmd.pending == 0)
            break;  // silent retiree; the erase pass removes it
          // An address-scoped clear owes one on_not_sent() per accepted request.
          if (cmd.device == nullptr) {
            cmd.pending = 0;
          } else {
            cmd.device->trigger_not_sent(cmd.frame.pdu());
            // A device-scoped clear from inside the handler silences the rest.
            cmd.pending = (cmd.device == nullptr || cmd.pending == 0) ? 0 : cmd.pending - 1;
          }
          progressed = true;
          break;
        }
        default: {  // READY / WAITING: only serviced to bleed pending down to the servable cap
          if (cmd.pending <= servable_cap_(cmd) || cmd.served_not_sent)
            break;  // served entries wait for the next sweep, bounding handler re-send loops
          cmd.served_not_sent = true;
          if (cmd.device != nullptr)
            cmd.device->trigger_not_sent(cmd.frame.pdu());
          if (cmd.pending != 0) {  // a clear inside the handler may have drained it
            cmd.pending--;
            cmd.seq = this->next_seq_++;  // a request resolved; the place in line moves up
          }
          progressed = true;
          break;
        }
      }
    }
  }
  // Erase pass: the ONLY place entries leave the container (the index/reference stability above
  // and in the parse path relies on this).
  this->tx_buffer_.erase(std::remove_if(this->tx_buffer_.begin(), this->tx_buffer_.end(),
                                        [](const ModbusDeviceCommand &cmd) {
                                          return cmd.state == FrameState::DELETED && cmd.pending == 0;
                                        }),
                         this->tx_buffer_.end());
  // Reset the per-sweep delivery markers; anything a marker deferred re-arms the next sweep.
  for (auto &cmd : this->tx_buffer_) {
    cmd.served_not_sent = false;
    if (cmd.state != FrameState::DELETED && cmd.pending > servable_cap_(cmd))
      this->sweep_needed_ = true;
  }
}

// Raw send for client: pushes to tx queue. Everything except the CRC must be contained in payload.
void ModbusClientHub::send_pdu(uint8_t address, std::span<const uint8_t> pdu, ModbusClientDevice *device,
                               CommandOptions options) {
  // Validation failures never enter the state machine - nothing was accepted, and an oversize PDU
  // cannot even be stored in a frame entry - so their terminal is delivered inline. These are the
  // only on_not_sent() deliveries that don't come from the sweep.
  if (pdu.empty()) {
    if (device != nullptr)
      device->trigger_not_sent(pdu);
    return;
  }
  // Bound the PDU so the wire frame (address + pdu + CRC) stays within the Modbus RTU 256-byte limit.
  if (pdu.size() > MAX_PDU_SIZE) {
    ESP_LOGE(TAG, "Frame too large, dropped: %" PRIu8 ":%zu bytes", address, pdu.size());
    if (device != nullptr)
      device->trigger_not_sent(pdu);
    return;
  }

  // Writes outrank reads on the wire; the priority is derived from the frame, never chosen by
  // callers. The read-modify-write function codes (0x16/0x17) mutate registers, so they rank as
  // writes for ordering - is_function_code_read() already keeps them non-requeueable. continuous is
  // ignored for every mutating code (re-writing a value forever is never intended).
  // is_function_code_write() masks the exception bit, so exception-flagged codes are excluded
  // explicitly: a nonsense 0x8x frame built in a lambda must not take WRITE-class ordering.
  const auto request_code = static_cast<FunctionCode>(pdu[0]);
  const bool mutates = !helpers::is_function_code_exception(pdu[0]) &&
                       (helpers::is_function_code_write(pdu[0]) || request_code == FunctionCode::MASK_WRITE_REGISTER ||
                        request_code == FunctionCode::READ_WRITE_MULTIPLE_REGISTERS);
  const CommandPriority priority = mutates ? CommandPriority::WRITE : CommandPriority::READ;
  bool continuous = false;
  if (options.continuous) {
    if (mutates) {
      ESP_LOGV(TAG, "continuous is ignored for a mutating function (0x%X, address %" PRIu8 ")", pdu[0], address);
    } else {
      continuous = true;
    }
  }

  // A frame arriving THROUGH send_pdu() that is identical to a live entry with the same owner is
  // not queued twice; the duplicate resolves against that entry (see the lifecycle contract):
  //   - anonymous (device == nullptr): dropped - with no callbacks there is no lifecycle to absorb
  //     into, and no owner to route a re-run to
  //   - the incoming request is continuous: the entry BECOMES the continuous poll; the conversion
  //     supersedes any requests the entry had absorbed (the caller opted into streaming semantics,
  //     and the poll's responses are the accounting from then on)
  //   - the entry is a continuous poll: absorbed uncounted - the poll's next response serves it
  //   - both one-shots: pending increments (without bound); the sweep bleeds anything over the
  //     servable cap (2 for requeueable reads, 1 for writes/custom) as on_not_sent() deliveries
  // REFUSED and *_DELETED entries are dead for dedup purposes: a new identical send queues fresh.
  for (auto &item : this->tx_buffer_) {
    switch (item.state) {
      case FrameState::REFUSED:
      case FrameState::DELETED:
      case FrameState::WAITING_DELETED:
        continue;
      default:
        break;
    }
    if (item.device != device || !item.same_frame(address, pdu))
      continue;
    if (device == nullptr) {
      // Reads are idempotent, so their drop is routine (DEBUG); a dropped write/custom is a
      // wire-behavior difference the caller cannot observe without a device, so it warns.
      const bool requeueable = !helpers::is_function_code_exception(pdu[0]) && helpers::is_function_code_read(pdu[0]);
      if (requeueable) {
        ESP_LOGD(TAG, "Anonymous duplicate of active frame for %" PRIu8 " (function 0x%X), dropped", address, pdu[0]);
      } else {
        ESP_LOGW(TAG,
                 "Anonymous duplicate of active frame for %" PRIu8 " (function 0x%X), dropped - register a "
                 "device for delivery accounting",
                 address, pdu[0]);
      }
    } else if (continuous) {
      item.continuous = true;
      item.pending = 1;
      ESP_LOGV(TAG, "Frame already active for %" PRIu8 ", now polled continuously", address);
    } else if (item.continuous) {
      ESP_LOGV(TAG, "Frame already active for %" PRIu8 " as a continuous poll", address);
    } else {
      // An absorbed duplicate leaves seq alone: the entry's place in line belongs to its oldest
      // outstanding request, and that one is still unresolved.
      item.pending++;
      if (item.pending > servable_cap_(item))
        this->sweep_needed_ = true;  // the excess resolves as on_not_sent() at the next sweep
      ESP_LOGV(TAG, "Frame already active for %" PRIu8 ", request absorbed (pending %" PRIu8 ")", address,
               item.pending);
    }
    return;
  }

  if (this->live_count_() >= MODBUS_TX_BUFFER_SIZE) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
    char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
    ESP_LOGE(TAG, "Write buffer full, dropped: %" PRIu8 ":%s", address,
             format_hex_pretty_to(hex_buf, pdu.data(), pdu.size()));
    this->refuse_(device, address, pdu);
    return;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
  ESP_LOGV(TAG, "Adding frame to tx queue: %" PRIu8 ":%s", address,
           format_hex_pretty_to(hex_buf, pdu.data(), pdu.size()));
  auto &cmd = this->tx_buffer_.emplace_back(device, address, pdu, priority);
  cmd.continuous = continuous;
  cmd.seq = this->next_seq_++;
}

void ModbusClientHub::clear_tx_queue_for_address(uint8_t address, bool clear_sent) {
  // A clear is a pure state flip; the next sweep delivers every owed on_not_sent() from a
  // quiescent hub. Other devices talking to the same physical device (e.g. a modbus_client action
  // alongside a controller that just went offline) must observe the drop, so DELETED entries keep
  // their pending count and resolve one terminal per accepted request.
  for (auto &cmd : this->tx_buffer_) {
    if (cmd.frame.address() != address)
      continue;
    switch (cmd.state) {
      case FrameState::READY:
      case FrameState::TIMED_OUT:
        cmd.state = FrameState::DELETED;
        this->sweep_needed_ = true;
        break;
      case FrameState::RECEIVED_RESPONSE:
      case FrameState::RECEIVED_EXCEPTION:
        // Cleared from inside its own on_response()/on_error(): that callback was the resolution,
        // so the pending re-run/continuous cycle is cancelled silently (stop-polling-now).
        retire_(cmd);
        cmd.device = nullptr;
        this->sweep_needed_ = true;
        break;
      case FrameState::WAITING:
      case FrameState::INTERRUPTED:
      case FrameState::INTERRUPTED_NOTIFIED:
        // On the wire: only detached when clear_sent is set. The shell keeps blocking until the
        // send-wait timeout (or the response arrives) and is swept silently.
        if (clear_sent) {
          ESP_LOGV(TAG, "Clearing waiting for response for address %" PRIu8, address);
          cmd.state = FrameState::WAITING_DELETED;
          cmd.silent_delete = true;
          cmd.pending = 0;
          cmd.device = nullptr;  // never deliver through a detached shell
          this->sweep_needed_ = true;
        }
        break;
      default:  // REFUSED already owes exactly its terminal; *_DELETED is already resolved
        break;
    }
  }
}

void ModbusClientHub::clear_tx_queue_for_device(ModbusClientDevice *device) {
  // Silent teardown (supersede semantics): the caller's own frames vanish without callbacks; see
  // the lifecycle note on ModbusClientDevice.
  for (auto &cmd : this->tx_buffer_) {
    if (cmd.device != device)
      continue;
    switch (cmd.state) {
      case FrameState::WAITING:
      case FrameState::INTERRUPTED:
      case FrameState::INTERRUPTED_NOTIFIED:
        // On the wire: keeps blocking until the send-wait timeout, then swept silently.
        cmd.state = FrameState::WAITING_DELETED;
        cmd.silent_delete = true;
        cmd.pending = 0;
        break;
      case FrameState::WAITING_DELETED:
        break;
      default:  // queued and terminal entries (including REFUSED) retire silently
        retire_(cmd);
        break;
    }
    cmd.device = nullptr;  // the device is going away (or superseding); never deliver through it
    this->sweep_needed_ = true;
  }
}

void ModbusClientHub::send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device) {
  if (payload.size() < 2) {
    if (device != nullptr)
      device->trigger_not_sent({});  // too short to contain a PDU
    return;
  }
  this->send_pdu(payload[0], std::span<const uint8_t>(payload).subspan(1), device);
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
    // without a heap allocation. Only one server reply is ever waiting, and the named timeout ensures
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

void ModbusClientDevice::dispatch_response_(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                                            ResponseStatus status) {
  if (request_pdu.empty())
    return;
  auto function_code = static_cast<FunctionCode>(request_pdu[0]);
  // All standard requests handled below are function code + start address + count/value (5 bytes);
  // anything shorter cannot be parsed and is handed to the catch-all.
  if (request_pdu.size() < READ_PDU_SIZE) {
    this->on_custom_response(request_pdu, response_pdu, status);
    return;
  }
  const uint16_t start_address = helpers::get_data<uint16_t>(request_pdu.data(), 1);
  // count for reads/multi-writes, value for single writes
  const uint16_t count_or_value = helpers::get_data<uint16_t>(request_pdu.data(), 3);

  // Gatekeeper for the typed dispatch below: anything that is not a standard-conformant transaction is
  // handed to on_custom_response() with the raw PDUs, so the decode cases can trust every length, byte
  // count, and quantity field without re-clamping.
  //  - The REQUEST must be standard: nothing upstream validates a caller-built request PDU, so its
  //    internal byte count, quantity, and address range are checked here (is_client_pdu_standard()).
  //  - On success, the RESPONSE must be standard (self-consistent; the frame parser already guarantees
  //    most of this, but the check keeps the safety proof local), and a read response's length must also
  //    match the REQUESTED count - the per-PDU checks cannot see that relationship, and a short but
  //    self-consistent response must be diverted, never silently clamped and delivered as complete.
  //  - On failure (status engaged) the response is empty by design (see on_error()), so only the request
  //    is validated.
  bool custom = !helpers::is_client_pdu_standard(request_pdu.data(), request_pdu.size());
  if (!custom && !status.has_value()) {
    custom = !helpers::is_server_pdu_standard(response_pdu.data(), response_pdu.size());
    if (!custom && helpers::is_function_code_read(static_cast<uint8_t>(function_code))) {
      const bool bits =
          function_code == FunctionCode::READ_COILS || function_code == FunctionCode::READ_DISCRETE_INPUTS;
      const size_t expected_data_size =
          bits ? packed_bit_bytes(count_or_value) : static_cast<size_t>(count_or_value) * 2;
      if (response_pdu.size() != expected_data_size + 2) {
        ESP_LOGD(TAG, "Response length %zu does not match request (expected %zu) for function code 0x%X",
                 response_pdu.size(), expected_data_size + 2, static_cast<uint8_t>(function_code));
        custom = true;
      }
    }
  }
  if (custom) {
    this->on_custom_response(request_pdu, response_pdu, status);
    return;
  }

  switch (function_code) {
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS: {
      // Decode the big-endian register words into host byte order. The gate guarantees a success response
      // carries exactly count_or_value registers (and count_or_value <= MAX_NUM_OF_REGISTERS_TO_READ, the
      // capacity of RegisterValues); a mismatch was diverted to on_custom_response(), never clamped. On
      // failure the registers span is empty.
      RegisterValues registers;
      if (!status.has_value()) {
        for (size_t i = 0; i != count_or_value; i++) {
          registers.push_back(helpers::get_data<uint16_t>(response_pdu.data(), 2 + 2 * i));
        }
      }
      std::span<const uint16_t> register_span(registers.data(), registers.size());
      if (function_code == FunctionCode::READ_HOLDING_REGISTERS) {
        this->on_read_holding_registers(start_address, register_span, status);
      } else {
        this->on_read_input_registers(start_address, register_span, status);
      }
      break;
    }
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS: {
      // Deliver the bits packed as on the wire; the gate guarantees a success response carries exactly
      // (count_or_value + 7) / 8 data bytes. On failure the view is empty AND the count is zero -
      // PackedBits::operator[] is unchecked, so size() must never promise bits with no bytes behind them.
      std::span<const uint8_t> packed_bytes;
      uint16_t count = 0;
      if (!status.has_value()) {
        packed_bytes = response_pdu.subspan(2);
        count = count_or_value;
      }
      PackedBits bits(packed_bytes, count);
      if (function_code == FunctionCode::READ_COILS) {
        this->on_read_coils(start_address, bits, status);
      } else {
        this->on_read_discrete_inputs(start_address, bits, status);
      }
      break;
    }
    // Single-write acks echo the value: on success that echo is device-confirmed state - the one
    // write whose acknowledgement carries a real read-back - so it is preferred over the request
    // copy. On an exception the response has no value and the request copy is the only one.
    case FunctionCode::WRITE_SINGLE_REGISTER:
    case FunctionCode::WRITE_SINGLE_COIL: {
      const uint16_t value = (!status.has_value() && response_pdu.size() >= WRITE_SINGLE_PDU_SIZE)
                                 ? helpers::get_data<uint16_t>(response_pdu.data(), 3)
                                 : count_or_value;
      if (function_code == FunctionCode::WRITE_SINGLE_REGISTER) {
        this->on_write_single_register(start_address, value, status);
      } else {
        this->on_write_single_coil(start_address, value == 0xFF00, status);
      }
      break;
    }
    case FunctionCode::WRITE_MULTIPLE_REGISTERS: {
      // Request layout: [0] function code, [1..2] start address, [3..4] register count, [5] byte count,
      // [6..] register data. The gate guarantees the request carries exactly count_or_value registers
      // (<= MAX_NUM_OF_REGISTERS_TO_WRITE, within RegisterValues capacity). Decoded from the request and
      // delivered regardless of status - see the write-acknowledgement note in modbus.h.
      RegisterValues registers;
      for (size_t i = 0; i != count_or_value; i++) {
        registers.push_back(helpers::get_data<uint16_t>(request_pdu.data(), 6 + 2 * i));
      }
      std::span<const uint16_t> register_span(registers.data(), registers.size());
      this->on_write_multiple_registers(start_address, register_span, status);
      break;
    }
    case FunctionCode::WRITE_MULTIPLE_COILS: {
      // Request layout: [0] function code, [1..2] start address, [3..4] coil count, [5] byte count,
      // [6..] packed bits. The gate guarantees the request carries exactly (count_or_value + 7) / 8 packed
      // bytes. Decoded from the request and delivered regardless of status - see the write-acknowledgement
      // note in modbus.h.
      std::span<const uint8_t> packed_bytes = request_pdu.subspan(6);
      PackedBits bits(packed_bytes, count_or_value);
      this->on_write_multiple_coils(start_address, bits, status);
      break;
    }
    default:
      this->on_custom_response(request_pdu, response_pdu, status);
      break;
  }
}

// Default on_custom_response handler to warn when responses unexpectedly trigger on_custom_response
void ModbusClientDevice::on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                                            ResponseStatus status) {
  // The dispatcher never calls this with an empty request, but this is a public virtual - stay safe.
  const uint8_t function_code = request_pdu.empty() ? 0 : request_pdu[0];
  // Warn once per device, then drop to VERBOSE: a mildly non-conformant peer answers every poll,
  // and an unhandled-response warning per transaction would flood the log permanently.
  if (!this->custom_response_warned_) {
    this->custom_response_warned_ = true;
    ESP_LOGW(TAG, "Non-standard request or response for function code 0x%X. No on_custom_response handler declared",
             function_code);
  } else {
    ESP_LOGV(TAG, "Non-standard request or response for function code 0x%X (unhandled)", function_code);
  }
}

}  // namespace esphome::modbus
