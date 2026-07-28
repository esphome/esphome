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
    uint8_t expected_address = wfr.frame.address();
    if (this->last_receive_check_ - this->last_send_ > this->last_send_tx_offset_ + this->send_wait_time_ &&
        (this->rx_buffer_.empty() || this->rx_buffer_[0] != expected_address)) {
      ESP_LOGW(TAG, "Stop waiting for response from %" PRIu8 " %" PRIu32 "ms after last send", expected_address,
               this->last_receive_check_ - this->last_send_);
      this->notify_no_response_(wfr);
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
  if (!this->waiting_for_response_.has_value()) {
    ESP_LOGW(TAG,
             "Received unexpected frame from address %" PRIu8 ", function code 0x%X, %" PRIu32 "ms after last send",
             address, function_code, this->last_modbus_byte_ - this->last_send_);
    return;
  } else {  // We are waiting for a response
    // Check if the response matches the expected address and function code

    ModbusDeviceCommand &wfr = this->waiting_for_response_.value();
    uint8_t expected_address = wfr.frame.address();
    uint8_t expected_function_code = wfr.frame.pdu()[0];
    if (expected_address != address || expected_function_code != (function_code & FUNCTION_CODE_MASK)) {
      ESP_LOGW(TAG,
               "Received incorrect frame address %" PRIu8 " <> %" PRIu8 " or function code 0x%X <> 0x%X, %" PRIu32
               "ms after last send",
               address, expected_address, (function_code & FUNCTION_CODE_MASK), expected_function_code,
               this->last_modbus_byte_ - this->last_send_);
      // Invalidate the device; the entry survives as an interrupted shell so the late response is ignored.
      // A retry requested here stays queued behind the shell until the send-wait timeout clears it.
      this->notify_no_response_(wfr);
      wfr.interrupted = true;
      return;
    }

    if (wfr.interrupted) {
      ESP_LOGW(TAG,
               "Ignoring response from %" PRIu8 " - transmission interrupted by previous unexpected response, %" PRIu32
               "ms after last send",
               address, this->last_modbus_byte_ - this->last_send_);
      return;
    } else {  // We have a valid device waiting for this response

      // Move the command out of the waiting slot so the request PDU stays alive for the callback.
      ModbusDeviceCommand command = std::move(this->waiting_for_response_.value());
      this->waiting_for_response_.reset();
      ModbusClientDevice *device = command.device;
      // The request PDU is the sent frame without the leading address and the trailing CRC.
      std::span<const uint8_t> request_pdu = command.frame.pdu();
      // Is it an error response?
      if (helpers::is_function_code_exception(function_code)) {
        uint8_t exception = pdu[1];  // exception frames are fixed-length, so the code is always present
        ESP_LOGW(TAG,
                 "Error function code: 0x%X exception: %" PRIu8 ", address: %" PRIu8 ", %" PRIu32 "ms after last send",
                 function_code, exception, address, this->last_modbus_byte_ - this->last_send_);
        if (device)
          device->on_error(request_pdu, static_cast<ExceptionCode>(exception));
      } else if (device) {  // Not an error response
        device->on_response(request_pdu, pdu);
      } else {  // Not an error response, but no device to respond to
        ESP_LOGV(TAG, "Ignoring response from %" PRIu8 " - no callback device set, %" PRIu32 "ms after last send",
                 address, this->last_modbus_byte_ - this->last_send_);
      }
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
  if (this->tx_buffer_.empty()) {
    return;
  }

  if (this->tx_blocked()) {
    return;
  }

  // Move the command out and pop BEFORE attempting the send: no callback may run while the frame still
  // sits in the queue (the same principle as the clear sweep). A failure callback that sends would
  // otherwise queue a new frame and pop_front() could discard the wrong one - and the deque
  // reference / PDU span could be invalidated mid-callback.
  ModbusDeviceCommand command = std::move(this->tx_buffer_.front());
  this->tx_buffer_.pop_front();
  ModbusClientDevice *device = command.device;
  const bool sent = this->send_frame_(command.frame);

  if (sent) {
    // The frame now lives in the waiting slot; its PDU is the frame without the leading address and
    // trailing CRC.
    ModbusDeviceCommand &wfr = this->waiting_for_response_.emplace(std::move(command));
    if (device != nullptr)
      device->on_sent(wfr.frame.pdu());
  } else {
    if (device != nullptr)
      device->trigger_not_sent(command.frame.pdu());
  }

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

void ModbusClientHub::notify_no_response_(ModbusDeviceCommand &wfr) {
  if (wfr.device == nullptr)
    return;
  const bool retry = wfr.device->on_no_response(wfr.frame.pdu());
  // The callback may have detached the device (e.g. clear_tx_queue_for_device()); honor the detach
  // over the retry request rather than re-queueing a frame that can no longer be routed.
  if (retry && wfr.device != nullptr)
    this->requeue_waiting_frame_(wfr);
  // The old transaction is over either way; never deliver anything else to the device through it.
  wfr.device = nullptr;
}

void ModbusClientHub::requeue_waiting_frame_(ModbusDeviceCommand &wfr) {
  const ModbusFrame &frame = wfr.frame;
  if (this->tx_buffer_.size() >= MODBUS_TX_BUFFER_SIZE) {
    ESP_LOGE(TAG, "Write buffer full, dropped retry for address %" PRIu8, frame.address());
    if (wfr.device != nullptr)
      wfr.device->trigger_not_sent(frame.pdu());
    return;
  }
  // Re-queue a copy (not a move): the waiting entry may have to survive as an interrupted shell.
  this->tx_buffer_.emplace_back(wfr.device, frame.address(), frame.pdu());
}

// Raw send for client: pushes to tx queue. Everything except the CRC must be contained in payload.
void ModbusClientHub::send_pdu(uint8_t address, std::span<const uint8_t> pdu, ModbusClientDevice *device) {
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

  if (this->tx_buffer_.size() < MODBUS_TX_BUFFER_SIZE) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
    ESP_LOGV(TAG, "Adding frame to tx queue: %" PRIu8 ":%s", address,
             format_hex_pretty_to(hex_buf, pdu.data(), pdu.size()));
    this->tx_buffer_.emplace_back(device, address, pdu);
  } else {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
    char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
    ESP_LOGE(TAG, "Write buffer full, dropped: %" PRIu8 ":%s", address,
             format_hex_pretty_to(hex_buf, pdu.data(), pdu.size()));
    if (device != nullptr)
      device->trigger_not_sent(pdu);
  }
}

void ModbusClientHub::clear_tx_queue_for_address(uint8_t address, bool clear_sent) {
  // Drop the queued frames for this address, delivering on_not_sent() to each frame's owner: other
  // devices talking to the same physical device (e.g. a modbus_client action alongside a controller that
  // just went offline) must observe the drop, or their command never resolves. Mark first, then sweep
  // only marked frames: anything a callback re-queues is unmarked, so it
  // is never swept - or re-notified - by the clear that triggered it. Each marked frame is moved out and erased BEFORE
  // its callback runs, so handlers see a consistent queue; termination is guaranteed because only the initially-marked
  // frames are ever swept.
  for (auto &cmd : this->tx_buffer_) {
    if (cmd.frame.address() == address)
      cmd.marked_for_deletion = true;
  }
  for (;;) {
    auto it = std::find_if(this->tx_buffer_.begin(), this->tx_buffer_.end(),
                           [](const ModbusDeviceCommand &cmd) { return cmd.marked_for_deletion; });
    if (it == this->tx_buffer_.end())
      break;
    ModbusDeviceCommand dropped = std::move(*it);
    this->tx_buffer_.erase(it);
    // The sweep delivers through the same per-device guard as refusals: a device clearing from inside
    // its own on_not_sent() gets its remaining frames resolved silently (documented in the lifecycle
    // contract), other owners are notified normally, and every nested clear stays bounded.
    if (dropped.device != nullptr)
      dropped.device->trigger_not_sent(dropped.frame.pdu());
  }

  if (clear_sent && this->waiting_for_response_.has_value() && this->waiting_for_response_.value().device) {
    if (this->waiting_for_response_.value().frame.address() == address) {
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
          bits ? (static_cast<size_t>(count_or_value) + 7) / 8 : static_cast<size_t>(count_or_value) * 2;
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
