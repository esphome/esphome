#include "modbus.h"

#include <algorithm>

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

// Shortest gap between two "no device accepted broadcast" warnings
static constexpr uint32_t UNACCEPTED_BROADCAST_WARN_INTERVAL_MS = 60 * MS_PER_SEC;

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
  // Drain anything owed since the last loop (e.g. an external clear) before the watchdog runs, so it
  // never times out an entry whose pending count has not been drained. No-op when nothing is owed.
  this->sweep_();

  this->Modbus::loop();  // receive bytes and parse frames

  // Send-wait watchdog: only the cheap time check runs at loop rate; expire_waiting_() looks the
  // entry up and holds off if the response has started arriving.
  if (this->waiting_for_response_ &&
      this->last_receive_check_ - this->last_send_ > this->last_send_tx_offset_ + this->send_wait_time_) {
    this->expire_waiting_();
  }

  this->sweep_();  // deliver owed callbacks with the hub quiescent
  this->send_next_frame_();
}

void ModbusClientHub::expire_waiting_() {
  ModbusDeviceCommand *cmd = this->find_waiting_();
  if (cmd == nullptr) {
    this->waiting_for_response_ = false;
    return;
  }
  if (!this->rx_buffer_.empty() && this->rx_buffer_[0] == cmd->frame.address()) {
    // The start of the response is in the buffer: let the frame finish arriving.
    return;
  }
  // Only a genuine WAITING entry warrants the log (a cleared or interrupted shell timing out is expected).
  if (cmd->state == FrameState::WAITING) {
    ESP_LOGW(TAG, "Stop waiting for response from %" PRIu8 " %" PRIu32 "ms after last send", cmd->frame.address(),
             this->last_receive_check_ - this->last_send_);
  }
  // Deliver on_no_response directly, the way the parse path delivers response()/error(): the entry
  // lands in TIMED_OUT and the following sweep reschedules a retry or erases it. Free the
  // wire first so a resend from inside the callback sees it available.
  this->waiting_for_response_ = false;
  this->sweep_needed_ = true;
  cmd->timed_out();
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
  // 1. We're waiting for a response (a waiting entry: WAITING/INTERRUPTED/WAITING_RETIRED/INTERRUPTED_RETIRED)
  // 2. Any of the base class tx_blocked conditions
  return this->waiting_for_response_ || this->Modbus::tx_blocked();
}

bool ModbusClientHub::tx_buffer_empty() {
  // "Empty" for ready_for_immediate_send(): no one-shot is queued ahead of the caller. Entries in
  // other states are mid-transaction or owed bookkeeping, not queued sends - and a READY continuous
  // poll does not count either, since it ranks below every one-shot, so a new send goes out first.
  for (const auto &cmd : this->tx_buffer_) {
    if (cmd.state == FrameState::READY && !cmd.continuous)
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
    // A broadcast is a client request, never a peer response; clear any stale expectation (RTU is half-duplex).
    const bool is_broadcast = this->rx_buffer_[0] == BROADCAST_ADDRESS;
    if (is_broadcast)
      this->expecting_peer_response_ = 0;
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

uint16_t Modbus::find_frame_end_by_crc_(uint16_t min_length) const {
  // Unknown-length functions (user-defined codes, unimplemented management codes, unassigned values)
  // could be any length - we have to rely on the CRC to determine completeness.
  // If a CRC match is never found, the buffer will eventually overflow and be cleared.
  const uint8_t *raw = &this->rx_buffer_[0];
  const size_t size = this->rx_buffer_.size();
  const auto max_len = static_cast<uint16_t>(std::min(size, size_t(MAX_FRAME_SIZE)));
  if (min_length > max_len)
    return 0;
  // The Modbus CRC (poly 0xa001, refin/refout false) keeps its running state in the returned value,
  // so we seed once over the first min_length bytes and extend one byte at a time instead of
  // recomputing the whole prefix for every candidate length.
  uint16_t crc = crc16(raw, min_length);
  if (crc == 0)
    return min_length;
  for (uint16_t len = min_length; len < max_len; len++) {
    crc = crc16(&raw[len], 1, crc);
    if (crc == 0)
      return len + 1;
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

  if (helpers::is_function_code_unknown_length(function_code)) {
    frame_length = this->find_frame_end_by_crc_(frame_length);
    if (frame_length == 0)
      return size < MAX_FRAME_SIZE;  // Continue to parse until we hit max size
    ESP_LOGD(TAG, "Unknown-length function %02X found", function_code);
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

  if (helpers::is_function_code_unknown_length(function_code)) {
    frame_length = this->find_frame_end_by_crc_(frame_length);
    if (frame_length == 0)
      return size < MAX_FRAME_SIZE;  // Continue to parse until we hit max size
    ESP_LOGD(TAG, "Unknown-length function %02X found", function_code);
  } else {
    if (crc16(&this->rx_buffer_[0], frame_length) != 0)
      return false;
  }

  // Clear before processing: process_modbus_client_frame_ dispatches to a server device which sends
  // a response immediately. We need to clear the rx buffer first so the response doesn't snag tx_blocked.
  // This requires copying the frame data to a local buffer beforehand.
  uint8_t data_offset = helpers::client_frame_data_offset(this->rx_buffer_.data(), this->rx_buffer_.size());
  uint16_t data_len = frame_length - 2 - data_offset;
  uint8_t data_buffer[MAX_FRAME_SIZE] = {};
  std::memcpy(data_buffer, this->rx_buffer_.data() + data_offset, data_len);
  std::span<const uint8_t> data(data_buffer, data_len);
  this->clear_rx_buffer_(LOG_STR("parse succeeded"), false, frame_length);

  if (address == BROADCAST_ADDRESS) {
    // Keep the unicast response buffers out of the broadcast call chain.
    this->process_broadcast_frame_(function_code, data);
  } else {
    this->process_modbus_client_frame_(address, function_code, data);
  }

  return true;
}

// The parser (parse_modbus_server_frame_) guarantees the bounds relied on here: pdu is never empty,
// and an exception-flagged pdu is at least 2 bytes. Keep that in mind when changing server_pdu_length().
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
    // Unexpected frame: flip a WAITING entry to an INTERRUPTED shell that ignores the rest of this
    // transaction and blocks tx until the send-wait timeout, where it gets its on_no_response.
    cmd->interrupt();
    return;
  }

  if (cmd->state == FrameState::INTERRUPTED || cmd->state == FrameState::INTERRUPTED_RETIRED) {
    // An interrupted shell keeps blocking until the send-wait timeout; a late response for it is
    // ignored and does NOT free the wire. The distrust survives a clear (INTERRUPTED_RETIRED), so a
    // cleared-interrupted frame still ends in on_no_response rather than delivering a late response.
    ESP_LOGW(TAG,
             "Ignoring response from %" PRIu8 " - transmission interrupted by previous unexpected response, %" PRIu32
             "ms after last send",
             address, this->last_modbus_byte_ - this->last_send_);
    return;
  }

  // Deliver at parse time so the response span can point into the rx buffer (zero copy). error()/
  // response() set the state and consume the request BEFORE the callback, so a clear from inside it
  // ("stop polling now") wins. A device-less shell runs no callback and the sweep erases it.
  this->waiting_for_response_ = false;
  this->sweep_needed_ = true;
  if (helpers::is_function_code_exception(function_code)) {
    uint8_t exception = pdu[1];  // exception frames are fixed-length, so the code is always present
    ESP_LOGW(TAG, "Error function code: 0x%X exception: %" PRIu8 ", address: %" PRIu8 ", %" PRIu32 "ms after last send",
             function_code, exception, address, this->last_modbus_byte_ - this->last_send_);
    cmd->error(static_cast<ExceptionCode>(exception));
  } else if (!cmd->response(pdu)) {
    ESP_LOGV(TAG, "Ignoring response from %" PRIu8 " - no callback device set, %" PRIu32 "ms after last send", address,
             this->last_modbus_byte_ - this->last_send_);
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

ResponseStatus ModbusServerHub::check_address_range_(uint16_t start_address, uint16_t count) {
  if (!helpers::address_range_fits(start_address, count)) {
    ESP_LOGW(TAG, "Address out of range - start: %" PRIu16 " num: %" PRIu16, start_address, count);
    return ExceptionCode::ILLEGAL_DATA_ADDRESS;
  }
  return std::nullopt;
}

// Write PDU layout after the function code: start address(2) [+ quantity(2) + byte count(1)] + register values.
// The value subspans taken at these offsets stay in range because client_pdu_length() clamps the byte count to the
// same maximum the callers' number_of_registers * 2 == number_of_bytes guard enforces.
static constexpr size_t WRITE_SINGLE_VALUES_OFFSET = 2;
static constexpr size_t WRITE_MULTIPLE_VALUES_OFFSET = 5;
// FC 0x17 writes follow read start(2) + read quantity(2) + write start(2) + write quantity(2) + byte count(1).
static constexpr size_t READ_WRITE_VALUES_OFFSET = 9;
// A coil write (FC 0x0F) is function(1) + start(2) + quantity(2) + byte count(1) + packed bits. The largest
// one (MAX_NUM_OF_COILS_TO_WRITE coils) must fit the received request PDU, so the value subspan taken at
// WRITE_MULTIPLE_VALUES_OFFSET can never run past it.
static_assert(1 + WRITE_MULTIPLE_VALUES_OFFSET + packed_bit_bytes(MAX_NUM_OF_COILS_TO_WRITE) <= MAX_PDU_SIZE,
              "the largest FC 0x0F coil write must fit within MAX_PDU_SIZE");

ResponseStatus ModbusServerHub::parse_write_single_(std::span<const uint8_t> data, uint16_t &start_address,
                                                    RegisterValues &registers) {
  start_address = helpers::get_data<uint16_t>(data.data(), 0);
  // No range check needed: one register can never push start_address + 1 past the address space.
  this->assemble_registers_(data.subspan(WRITE_SINGLE_VALUES_OFFSET, sizeof(uint16_t)), registers);
  return std::nullopt;
}

ResponseStatus ModbusServerHub::parse_write_multiple_(std::span<const uint8_t> data, uint16_t &start_address,
                                                      RegisterValues &registers) {
  start_address = helpers::get_data<uint16_t>(data.data(), 0);
  uint16_t number_of_registers = helpers::get_data<uint16_t>(data.data(), 2);
  uint8_t number_of_bytes = helpers::get_data<uint8_t>(data.data(), 4);
  if (number_of_registers == 0 || number_of_registers > MAX_NUM_OF_REGISTERS_TO_WRITE ||
      number_of_registers * 2 != number_of_bytes) {
    ESP_LOGW(TAG, "Invalid number of registers %" PRIu16 " or bytes %" PRIu8, number_of_registers, number_of_bytes);
    return ExceptionCode::ILLEGAL_DATA_VALUE;
  }
  if (ResponseStatus status = this->check_address_range_(start_address, number_of_registers); status.has_value()) {
    return status;
  }
  this->assemble_registers_(data.subspan(WRITE_MULTIPLE_VALUES_OFFSET, number_of_bytes), registers);
  return std::nullopt;
}

ResponseStatus ModbusServerHub::parse_read_request_(std::span<const uint8_t> data, uint16_t max_entities,
                                                    const LogString *entity_name, uint16_t &start_address,
                                                    uint16_t &count) {
  // Every read request is start address(2) + quantity(2); only the protocol ceiling differs per function
  // code, so registers and coils/discrete inputs validate through here and cannot drift apart.
  start_address = helpers::get_data<uint16_t>(data.data(), 0);
  count = helpers::get_data<uint16_t>(data.data(), 2);
  if (count == 0 || count > max_entities) {
    ESP_LOGW(TAG, "Invalid number of %s %" PRIu16, LOG_STR_ARG(entity_name), count);
    return ExceptionCode::ILLEGAL_DATA_VALUE;
  }
  return this->check_address_range_(start_address, count);
}

ResponseStatus ModbusServerHub::parse_write_single_coil_(std::span<const uint8_t> data, uint16_t &start_address,
                                                         bool &value) {
  start_address = helpers::get_data<uint16_t>(data.data(), 0);
  const uint16_t raw_value = helpers::get_data<uint16_t>(data.data(), WRITE_SINGLE_VALUES_OFFSET);
  if (raw_value != 0xFF00 && raw_value != 0x0000) {
    ESP_LOGW(TAG, "Invalid coil value 0x%04X", raw_value);
    return ExceptionCode::ILLEGAL_DATA_VALUE;
  }
  // No range check needed: one coil can never push start_address + 1 past the address space.
  value = raw_value == 0xFF00;
  return std::nullopt;
}

ResponseStatus ModbusServerHub::parse_write_multiple_coils_(std::span<const uint8_t> data, uint16_t &start_address,
                                                            uint16_t &count, std::span<const uint8_t> &packed_bytes) {
  start_address = helpers::get_data<uint16_t>(data.data(), 0);
  const uint16_t number_of_bits = helpers::get_data<uint16_t>(data.data(), 2);
  const uint8_t number_of_bytes = helpers::get_data<uint8_t>(data.data(), 4);
  if (number_of_bits == 0 || number_of_bits > MAX_NUM_OF_COILS_TO_WRITE ||
      packed_bit_bytes(number_of_bits) != number_of_bytes) {
    ESP_LOGW(TAG, "Invalid number of coils %" PRIu16 " or bytes %" PRIu8, number_of_bits, number_of_bytes);
    return ExceptionCode::ILLEGAL_DATA_VALUE;
  }
  if (ResponseStatus status = this->check_address_range_(start_address, number_of_bits); status.has_value()) {
    return status;
  }
  count = number_of_bits;
  // coil values follow start(2) + quantity(2) + byte count(1)
  packed_bytes = data.subspan(WRITE_MULTIPLE_VALUES_OFFSET, number_of_bytes);
  return std::nullopt;
}

void ModbusServerHub::assemble_registers_(std::span<const uint8_t> values, RegisterValues &registers) {
  for (size_t offset = 0; offset + 1 < values.size(); offset += 2) {
    registers.push_back(helpers::get_data<uint16_t>(values.data(), offset));
  }
}

void ModbusServerHub::process_broadcast_frame_(uint8_t function_code, std::span<const uint8_t> data) {
  // Broadcasts are only meaningful for writes and are never answered (Modbus 4.1 / 6.12), so an unsupported
  // function code or a validation failure is silently dropped instead of replying with an exception. Both
  // register writes (FC 0x06/0x10) and coil writes (FC 0x05/0x0F) are broadcastable by spec, and each shares
  // its parser with the addressed path so a broadcast is validated exactly as the unicast form would be.
  uint16_t start_address;
  RegisterValues registers;
  uint16_t coil_count = 0;
  std::span<const uint8_t> packed_bytes;
  uint8_t single_bit = 0;  // backs packed_bytes for a single-coil write, so it must outlive the loop below
  bool coils = false;
  ResponseStatus status;
  switch (static_cast<FunctionCode>(function_code)) {
    case FunctionCode::WRITE_SINGLE_REGISTER:
      status = this->parse_write_single_(data, start_address, registers);
      break;
    case FunctionCode::WRITE_MULTIPLE_REGISTERS:
      status = this->parse_write_multiple_(data, start_address, registers);
      break;
    case FunctionCode::WRITE_SINGLE_COIL: {
      coils = true;
      bool value = false;
      status = this->parse_write_single_coil_(data, start_address, value);
      single_bit = value ? 0x01 : 0x00;
      coil_count = 1;
      packed_bytes = std::span<const uint8_t>(&single_bit, 1);
      break;
    }
    case FunctionCode::WRITE_MULTIPLE_COILS:
      coils = true;
      status = this->parse_write_multiple_coils_(data, start_address, coil_count, packed_bytes);
      break;
    default:
      // Reads and read/write require a reply, so they are not valid as broadcasts.
      ESP_LOGV(TAG, "Ignoring broadcast with unsupported function code %" PRIu8, function_code);
      return;
  }
  if (status.has_value()) {
    return;
  }
  // A broadcast is never answered, so a rejecting device has no other feedback channel: report the
  // per-device outcome at V, and warn if the write reached nobody at all.
  bool accepted = false;
  for (auto *device : this->devices_) {
    // Same handlers as an addressed write - a device cannot tell a broadcast apart, and does not need
    // to: the hub owns the difference, which is only that no reply is ever sent.
    const ResponseStatus device_status =
        coils ? device->on_write_coils(start_address, PackedBits(packed_bytes, coil_count))
              : device->on_write_registers(start_address, registers);
    if (device_status.has_value()) {
      ESP_LOGV(TAG, "Device %" PRIu8 " rejected broadcast write with exception %" PRIu8, device->get_address(),
               static_cast<uint8_t>(device_status.value()));
    } else {
      accepted = true;
    }
  }
  if (!accepted && !this->devices_.empty()) {
    const uint16_t entity_count = coils ? coil_count : static_cast<uint16_t>(registers.size());
    const LogString *const entity_name = coils ? LOG_STR("coils") : LOG_STR("registers");
    // Warn at most once per interval, then drop to VERBOSE: on a shared bus a broadcast aimed at other nodes
    // repeats forever, so warning per frame would flood the log.
    const uint32_t now = millis();
    if (this->last_unaccepted_broadcast_warn_ == 0 ||
        now - this->last_unaccepted_broadcast_warn_ > UNACCEPTED_BROADCAST_WARN_INTERVAL_MS) {
      this->last_unaccepted_broadcast_warn_ = now;
      ESP_LOGW(TAG, "No device accepted broadcast write of %" PRIu16 " %s at 0x%04X", entity_count,
               LOG_STR_ARG(entity_name), start_address);
    } else {
      ESP_LOGV(TAG, "No device accepted broadcast write of %" PRIu16 " %s at 0x%04X", entity_count,
               LOG_STR_ARG(entity_name), start_address);
    }
  }
}

bool ModbusServerHub::build_or_reject_read_response_(uint8_t address, uint8_t function_code, ResponseStatus status,
                                                     uint16_t number_of_registers, const RegisterValues &registers,
                                                     std::span<uint8_t> response_buffer, uint16_t &response_len) {
  // A handler that returns an exception leaves registers partially filled, so check the exception
  // first and forward it before validating the register count on the success path.
  if (this->rejected_(address, function_code, status)) {
    return false;
  }

  if (registers.size() != number_of_registers) {
    ESP_LOGE(TAG, "Incorrect response %" PRIu16 " requested, %zu returned", number_of_registers, registers.size());
    this->send_exception_(address, function_code, ExceptionCode::SERVICE_DEVICE_FAILURE);
    return false;
  }

  // The byte count is a single byte, so the count must stay within the protocol read limit; above it the
  // static_cast<uint8_t>(number_of_registers * 2) below would silently truncate the byte count.
  if (number_of_registers > MAX_NUM_OF_REGISTERS_TO_READ) {
    ESP_LOGE(TAG, "Read response of %" PRIu16 " registers exceeds the limit of %" PRIu16, number_of_registers,
             MAX_NUM_OF_REGISTERS_TO_READ);
    this->send_exception_(address, function_code, ExceptionCode::SERVICE_DEVICE_FAILURE);
    return false;
  }

  // Byte count(1) + two bytes per register. Checked here rather than at the call sites so the bound travels with
  // the write itself: a future caller starting at a non-zero response_len, or passing a smaller buffer, is
  // rejected instead of overrunning it before send_response_'s size guard can fire.
  const size_t required = static_cast<size_t>(response_len) + 1 + static_cast<size_t>(number_of_registers) * 2;
  if (required > response_buffer.size()) {
    ESP_LOGE(TAG, "Read response needs %zu bytes but only %zu are available", required, response_buffer.size());
    this->send_exception_(address, function_code, ExceptionCode::SERVICE_DEVICE_FAILURE);
    return false;
  }

  response_buffer[response_len++] = static_cast<uint8_t>(number_of_registers * 2);  // actual byte count
  for (auto r : registers) {
    auto register_bytes = decode_value(r);
    response_buffer[response_len++] = register_bytes[0];
    response_buffer[response_len++] = register_bytes[1];
  }
  return true;
}

void ModbusServerHub::process_modbus_client_frame_(uint8_t address, uint8_t function_code,
                                                   std::span<const uint8_t> data) {
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
      uint16_t start_address;
      uint16_t number_of_registers;
      status = this->parse_read_request_(data, MAX_NUM_OF_REGISTERS_TO_READ, LOG_STR("registers"), start_address,
                                         number_of_registers);
      if (this->rejected_(address, function_code, status)) {
        return;
      }
      RegisterValues registers;
      if (static_cast<FunctionCode>(function_code) == FunctionCode::READ_HOLDING_REGISTERS) {
        status = device->on_read_holding_registers(start_address, number_of_registers, registers);
      } else {
        status = device->on_read_input_registers(start_address, number_of_registers, registers);
      }

      if (!this->build_or_reject_read_response_(address, function_code, status, number_of_registers, registers,
                                                response_buffer, response_len)) {
        return;
      }
      break;
    }
    case FunctionCode::WRITE_SINGLE_REGISTER:
    case FunctionCode::WRITE_MULTIPLE_REGISTERS: {
      // Parse and validate the write PDU into host-order register values; reply with an exception on failure.
      uint16_t start_address;
      RegisterValues registers;
      if (static_cast<FunctionCode>(function_code) == FunctionCode::WRITE_SINGLE_REGISTER) {
        status = this->parse_write_single_(data, start_address, registers);
      } else {
        status = this->parse_write_multiple_(data, start_address, registers);
      }
      if (this->rejected_(address, function_code, status)) {
        return;
      }
      status = device->on_write_registers(start_address, registers);
      response_data = data.data();  // echo the request header per Modbus 6.6, 6.12
      response_len = 4;
      break;
    }
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS: {
      uint16_t start_address;
      uint16_t number_of_bits;
      status =
          this->parse_read_request_(data, MAX_NUM_OF_COILS_TO_READ, LOG_STR("bits"), start_address, number_of_bits);
      if (this->rejected_(address, function_code, status)) {
        return;
      }
      // Response: byte count(1) + packed bytes, written straight into the pre-zeroed response buffer. It
      // always fits: the parse above caps the count, and a static_assert bounds that against MAX_RAW_SIZE.
      const uint8_t byte_count = static_cast<uint8_t>(packed_bit_bytes(number_of_bits));
      response_buffer[response_len++] = byte_count;
      // Take the packed-bytes span off a span that knows response_buffer's real size, so a future non-zero
      // response_len (e.g. a prefix written before the packed data) is a bounds error, not a silent overrun.
      std::span<uint8_t> packed_out = std::span<uint8_t>(response_buffer).subspan(response_len, byte_count);
      std::fill(packed_out.begin(), packed_out.end(), 0);
      MutablePackedBits bits(packed_out, number_of_bits);
      if (static_cast<FunctionCode>(function_code) == FunctionCode::READ_COILS) {
        status = device->on_read_coils(start_address, bits);
      } else {
        status = device->on_read_discrete_inputs(start_address, bits);
      }
      if (this->rejected_(address, function_code, status)) {
        return;
      }
      response_len += byte_count;
      break;
    }
    case FunctionCode::WRITE_SINGLE_COIL: {
      // A single coil is handed to the device as a one-bit packed view, the same form a multiple-coil
      // write takes, so a device only ever implements one coil write handler.
      uint16_t start_address;
      bool value = false;
      status = this->parse_write_single_coil_(data, start_address, value);
      if (this->rejected_(address, function_code, status)) {
        return;
      }
      const uint8_t single_bit = value ? 0x01 : 0x00;
      status = device->on_write_coils(start_address, PackedBits(std::span<const uint8_t>(&single_bit, 1), 1));
      response_data = data.data();  // echo the request header per Modbus 6.5, 6.11
      response_len = 4;
      break;
    }
    case FunctionCode::WRITE_MULTIPLE_COILS: {
      // Parse and validate the coil write PDU into a packed-bit view; reply with an exception on failure.
      uint16_t start_address;
      uint16_t count;
      std::span<const uint8_t> packed_bytes;
      status = this->parse_write_multiple_coils_(data, start_address, count, packed_bytes);
      if (this->rejected_(address, function_code, status)) {
        return;
      }
      status = device->on_write_coils(start_address, PackedBits(packed_bytes, count));
      response_data = data.data();  // echo the request header per Modbus 6.5, 6.11
      response_len = 4;
      break;
    }
    case FunctionCode::READ_WRITE_MULTIPLE_REGISTERS: {
      // PDU data: read start address(2) + read quantity(2) + write start address(2) + write quantity(2) +
      // write byte count(1) + write register values. Per Modbus 6.17 the write is performed before the read.
      uint16_t read_start_address = helpers::get_data<uint16_t>(data.data(), 0);
      uint16_t number_of_registers = helpers::get_data<uint16_t>(data.data(), 2);
      uint16_t write_start_address = helpers::get_data<uint16_t>(data.data(), 4);
      uint16_t number_of_write_registers = helpers::get_data<uint16_t>(data.data(), 6);
      uint8_t number_of_bytes = helpers::get_data<uint8_t>(data.data(), 8);
      if (number_of_registers == 0 || number_of_registers > MAX_NUM_OF_REGISTERS_TO_READ ||
          number_of_write_registers == 0 || number_of_write_registers > MAX_NUM_OF_REGISTERS_TO_WRITE_RW ||
          number_of_write_registers * 2 != number_of_bytes) {
        ESP_LOGW(TAG, "Invalid number of registers (read %" PRIu16 ", write %" PRIu16 ") or bytes %" PRIu8,
                 number_of_registers, number_of_write_registers, number_of_bytes);
        this->send_exception_(address, function_code, ExceptionCode::ILLEGAL_DATA_VALUE);
        return;
      }
      status = this->check_address_range_(read_start_address, number_of_registers);
      if (!status.has_value()) {
        status = this->check_address_range_(write_start_address, number_of_write_registers);
      }
      if (this->rejected_(address, function_code, status)) {
        return;
      }
      // Perform the write first (Modbus 6.17). Scoped so the write values are off the stack before the read
      // values are allocated, keeping only one RegisterValues buffer live at a time.
      {
        RegisterValues write_registers;
        this->assemble_registers_(data.subspan(READ_WRITE_VALUES_OFFSET, number_of_bytes), write_registers);
        // Dispatch to the standalone write and read handlers so any device implementing those supports 0x17
        // without a dedicated handler; a device that maps registers by address reconstructs the read response
        // from the values it just stored.
        status = device->on_write_registers(write_start_address, write_registers);
      }
      if (this->rejected_(address, function_code, status)) {
        return;
      }
      RegisterValues registers;
      status = device->on_read_holding_registers(read_start_address, number_of_registers, registers);

      if (!this->build_or_reject_read_response_(address, function_code, status, number_of_registers, registers,
                                                response_buffer, response_len)) {
        return;
      }
      break;
    }
    default:
      ESP_LOGW(TAG, "Unsupported function code %" PRIu8, function_code);
      this->send_exception_(address, function_code, ExceptionCode::ILLEGAL_FUNCTION);
      return;
  }
  if (!this->rejected_(address, function_code, status)) {
    this->send_response_(address, function_code, response_data, response_len);
  }
}

// Callers gate on tx_blocked() first, but the pre-send delay below can span several ms, so re-check
// after it and refuse (return false) if a byte arrived in that window rather than transmit over it.
bool Modbus::send_frame_(const ModbusFrame &frame) {
  const int32_t tx_delay_remaining = this->tx_delay_remaining();
  if (tx_delay_remaining > 0) {
    delay(tx_delay_remaining);
  }

  // The delay above can span several ms; a byte arriving in that window blocks transmission after the
  // caller's gate already passed. Don't collide with the incoming frame - leave the entry to retry.
  if (this->tx_blocked()) {
    return false;
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

  if (!this->send_frame_(cmd->frame)) {
    ESP_LOGV(TAG, "Send deferred for %" PRIu8 ": a frame arrived during the send delay, will retry",
             cmd->frame.address());
    return;
  }

  cmd->sent();
  if (cmd->frame.address() == BROADCAST_ADDRESS) {
    // A broadcast (address 0) is never answered (Modbus 4.1), so it is fire-and-forget: on_sent above
    // reports the transmission, and the entry then retires with no terminal callback instead of
    // occupying the waiting slot until the send-wait timeout expires. The turnaround delay already
    // spaces the next frame; the following sweep erases the entry.
    ESP_LOGV(TAG, "Broadcast to address 0 sent; no reply expected (fire-and-forget)");
    cmd->complete_broadcast();
    this->sweep_needed_ = true;
    return;
  }
  this->waiting_for_response_ = true;
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

bool ModbusServerHub::rejected_(uint8_t address, uint8_t function_code, ResponseStatus status) {
  if (!status.has_value())
    return false;
  // The one place a rejection becomes an exception reply, so the log carries the transaction context a
  // device handler never has: which client-facing address and function code drew which exception. DEBUG
  // rather than WARN because an exception reply is a normal protocol outcome and arrives per frame - a
  // probing or broken client would otherwise flood the log. The parse helpers still WARN with specifics.
  ESP_LOGD(TAG, "Exception %" PRIu8 " replied to function 0x%02X for address %" PRIu8,
           static_cast<uint8_t>(status.value()), function_code, address);
  this->send_exception_(address, function_code, status.value());
  return true;
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
    if (cmd.waiting_state())
      return &cmd;
  }
  return nullptr;
}

ModbusDeviceCommand *ModbusClientHub::select_next_ready_() {
  // Class first (WRITE, then one-shot READ, then CONTINUOUS), oldest within a class. seq is a
  // free-running counter, so compare each entry's AGE against it (correct across the full range).
  const uint16_t now = this->next_seq_;
  const auto age = [now](const ModbusDeviceCommand &cmd) -> uint16_t { return now - cmd.seq; };
  const auto older = [&age](const ModbusDeviceCommand &a, const ModbusDeviceCommand &b) { return age(a) > age(b); };
  ModbusDeviceCommand *best = nullptr;
  for (auto &cmd : this->tx_buffer_) {
    if (cmd.state != FrameState::READY)
      continue;
    if (best == nullptr || cmd.priority() > best->priority() ||
        (cmd.priority() == best->priority() && older(cmd, *best))) {
      best = &cmd;
    }
  }
  return best;
}

bool ModbusDeviceCommand::sent() {
  this->state = FrameState::WAITING;
  // on_sent() is not a terminal, so nothing is consumed.
  if (this->device == nullptr)
    return false;
  this->device->on_sent(this->frame.pdu());
  return true;
}

bool ModbusDeviceCommand::notify_retired() {
  if (!this->decrement_pending())
    return false;  // nothing owed - stop the sweep draining this entry
  if (this->device != nullptr)
    this->device->on_not_sent(this->frame.pdu());
  return true;  // consumed one debt (delivered, or silent when device-less) - keep draining to zero
}

bool ModbusDeviceCommand::response(std::span<const uint8_t> response_pdu) {
  this->state = this->state == FrameState::WAITING_RETIRED ? FrameState::RETIRED : FrameState::RECEIVED_RESPONSE;
  // A continuous poll is never consumed by its own response; a one-shot consumes one request here.
  if (!this->continuous)
    this->decrement_pending();
  if (this->device == nullptr)
    return false;
  this->device->on_response(this->frame.pdu(), response_pdu);
  return true;
}

bool ModbusDeviceCommand::error(ExceptionCode exception_code) {
  this->state = this->state == FrameState::WAITING_RETIRED ? FrameState::RETIRED : FrameState::RECEIVED_EXCEPTION;
  // An exception ends a continuous poll too, so decrement unconditionally.
  this->decrement_pending();
  if (this->device == nullptr)
    return false;
  this->device->on_error(this->frame.pdu(), exception_code);
  return true;
}

bool ModbusDeviceCommand::interrupt() {
  // An unexpected frame distrusts the transaction. A cleared-but-still-waiting shell distrusts too, so
  // the interrupt survives the clear in either order (WAITING_RETIRED -> INTERRUPTED_RETIRED).
  if (this->state == FrameState::WAITING) {
    this->state = FrameState::INTERRUPTED;
    return true;
  }
  if (this->state == FrameState::WAITING_RETIRED) {
    this->state = FrameState::INTERRUPTED_RETIRED;
    return true;
  }
  return false;
}

bool ModbusDeviceCommand::timed_out() {
  this->state = FrameState::TIMED_OUT;  // advance BEFORE the callback so a clear from inside it wins
  this->decrement_pending();            // resolve this request (WAITING-origin, so pending >= 1)
  if (this->device == nullptr)
    return false;  // resolved, no one to tell
  if (this->device->on_no_response(this->frame.pdu()))
    this->increment_pending();  // granted retry = re-request (capped)
  return true;
}

void ModbusClientHub::sweep_() {
  if (!this->sweep_needed_)
    return;
  this->sweep_needed_ = false;
  // Serve only the entries present now: a callback may append (a re-send), but those sit beyond
  // work_set and are left for the next sweep, which bounds the work and is the termination argument.
  // Entries leave the container only in the erase pass below, so indices/references stay valid.
  const size_t work_set = this->tx_buffer_.size();
  // Restart the walk after every callback: a handler may have moved any entry to any state.
  bool callback_ran = true;
  while (callback_ran) {
    callback_ran = false;
    for (size_t i = 0; i != work_set && !callback_ran; i++) {
      ModbusDeviceCommand &cmd = this->tx_buffer_[i];
      switch (cmd.state) {
        case FrameState::RECEIVED_RESPONSE:
        case FrameState::RECEIVED_EXCEPTION:
        case FrameState::TIMED_OUT:
          // Off the wire, callback already delivered: reschedule what is still pending, else erase.
          if (cmd.pending)
            cmd.requeue(this->next_seq_++);
          break;
        case FrameState::RETIRED:
          // Owes one on_not_sent() per accepted request; notify_retired() consumes one and reports
          // whether a debt remained, so the restart loop drains the entry to zero - even a device-less
          // shell with pending > 1 (no callback fires, but it still drains rather than stranding).
          callback_ran = cmd.notify_retired();
          break;
        case FrameState::WAITING_RETIRED:
        case FrameState::INTERRUPTED_RETIRED:
          // Cleared shell: drain only the un-run duplicates; the request in flight keeps pending 1
          // and gets its usual callback when it resolves.
          if (cmd.pending > 1)
            callback_ran = cmd.notify_retired();
          break;
        default:  // READY / WAITING / INTERRUPTED: idle or waiting for a response, nothing owed until the timeout
          break;
      }
    }
  }
  // Erase pass: the only place entries leave the container. Storage order carries no meaning, so a
  // finished entry is swap-and-popped; walking backwards means a moved-down entry is already seen.
  for (size_t i = this->tx_buffer_.size(); i-- > 0;) {
    const ModbusDeviceCommand &cmd = this->tx_buffer_[i];
    // pending == 0 is erasable, but shells still waiting for a response are exempt until it resolves.
    if (cmd.pending != 0 || cmd.waiting_state())
      continue;
    if (i + 1 != this->tx_buffer_.size())
      this->tx_buffer_[i] = std::move(this->tx_buffer_.back());
    this->tx_buffer_.pop_back();
  }
}

// Raw send for client: pushes to tx queue. Everything except the CRC must be contained in payload.
bool ModbusClientHub::queue_pdu(uint8_t address, std::span<const uint8_t> pdu, ModbusClientDevice *device,
                                CommandOptions options) {
  // Requests refused here never enter the machine and get no callback - the false return is it.
  if (pdu.empty()) {
    ESP_LOGW(TAG, "Empty PDU refused for address %" PRIu8, address);
    return false;
  }
  // Bound the PDU so the wire frame (address + pdu + CRC) stays within the Modbus RTU 256-byte limit.
  if (pdu.size() > MAX_PDU_SIZE) {
    ESP_LOGE(TAG, "Frame too large, refused: %" PRIu8 ":%zu bytes", address, pdu.size());
    return false;
  }
  // classify() drives both the broadcast guard and the continuous check below; compute it once.
  const CommandPriority priority = ModbusDeviceCommand::classify(pdu[0]);

  // A broadcast (address 0) is never answered (Modbus 4.1), so it is only meaningful for a command that
  // changes state. Refuse a broadcast that expects a reply - anything but a write or a custom/vendor code -
  // as it could never deliver a result, so the caller learns via the false return (and on_not_sent).
  // 0x17 (read/write multiple) is a knowing inclusion: classify() treats it as a write, so its write half
  // lands on every server and its unanswerable read half is simply discarded. An exception-flagged custom
  // code (0x80 bit set) is refused: is_function_code_custom() masks that bit away, so exclude it explicitly
  // here to match classify()'s exception-first handling of the write side.
  if (address == BROADCAST_ADDRESS && priority != CommandPriority::WRITE &&
      (!helpers::is_function_code_custom(pdu[0]) || helpers::is_function_code_exception(pdu[0]))) {
    ESP_LOGW(TAG, "Broadcast refused for function 0x%X: a broadcast (address 0) is never answered", pdu[0]);
    return false;
  }

  // continuous is ignored for every mutating code (re-writing a value forever is never intended).
  const bool mutates = priority == CommandPriority::WRITE;
  bool continuous = false;
  if (options.continuous) {
    if (mutates) {
      ESP_LOGV(TAG, "continuous is ignored for a mutating function (0x%X, address %" PRIu8 ")", pdu[0], address);
    } else {
      continuous = true;
    }
  }

  // A duplicate of a live entry with the same owner is not queued twice; it resolves against that
  // entry: anonymous -> dropped; continuous incoming -> convert the entry to a poll; one-shot onto a
  // poll -> downgrade the poll to one-shot; both one-shots -> pending++ below the cap, else refused.
  for (auto &item : this->tx_buffer_) {
    if (item.state == FrameState::RETIRED || item.state == FrameState::WAITING_RETIRED ||
        item.state == FrameState::INTERRUPTED_RETIRED)
      continue;  // cleared, on their way out: a new identical send queues fresh, never absorbs
    if (item.device != device || !item.same_frame(address, pdu))
      continue;
    if (device == nullptr) {
      // A dropped read is routine (DEBUG); a dropped write/custom warns (unobservable without a device).
      const bool requeueable =
          !helpers::is_function_code_exception(pdu[0]) && helpers::is_function_code_read_only(pdu[0]);
      if (requeueable) {
        ESP_LOGD(TAG, "Anonymous duplicate of active frame for %" PRIu8 " (function 0x%X), dropped", address, pdu[0]);
      } else {
        ESP_LOGW(TAG,
                 "Anonymous duplicate of active frame for %" PRIu8 " (function 0x%X), dropped - register a "
                 "device for delivery accounting",
                 address, pdu[0]);
      }
      return false;  // dropped: no entry, no callbacks - the refusal is the return value
    }
    if (continuous) {
      item.make_continuous(true);
      ESP_LOGV(TAG, "Frame already active for %" PRIu8 ", now polled continuously", address);
    } else if (item.continuous) {
      // A one-shot duplicate downgrades the poll to a one-shot: it runs one more cycle to serve this
      // request, then stops (mirrors continuous incoming converting a one-shot the other way).
      item.make_continuous(false);
      ESP_LOGV(TAG, "Frame already active for %" PRIu8 ", downgraded from continuous to one-shot", address);
    } else if (!item.increment_pending()) {
      // At the servable cap, so refused. (An absorbed duplicate leaves seq alone - the entry keeps
      // its place in line, held by its oldest outstanding request.)
      ESP_LOGD(TAG, "Frame already active for %" PRIu8 " with %" PRIu8 " requests pending, refused", address,
               item.pending);
      return false;
    } else {
      ESP_LOGV(TAG, "Frame already active for %" PRIu8 ", request absorbed (pending %" PRIu8 ")", address,
               item.pending);
    }
    return true;
  }

  // Backstop counts every entry; dead ones are gone by the sweep's end, so at worst they cost one
  // refusal at the very cap for one loop.
  if (this->tx_buffer_.size() >= MODBUS_TX_BUFFER_SIZE) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
    char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
    ESP_LOGE(TAG, "Write buffer full, refused: %" PRIu8 ":%s", address,
             format_hex_pretty_to(hex_buf, pdu.data(), pdu.size()));
    return false;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MODBUS_MAX_LOG_BYTES)];
#endif
  ESP_LOGV(TAG, "Adding frame to tx queue: %" PRIu8 ":%s", address,
           format_hex_pretty_to(hex_buf, pdu.data(), pdu.size()));
  this->tx_buffer_.emplace_back(device, address, pdu, continuous, this->next_seq_++);
  return true;
}

void ModbusClientHub::clear_tx_queue_for_address(uint8_t address) {
  // A clear is a pure state flip; the sweep delivers every owed on_not_sent() from a quiescent hub.
  for (auto &cmd : this->tx_buffer_) {
    if (cmd.frame.address() != address)
      continue;
    cmd.retire();
    this->sweep_needed_ = true;
  }
}

void ModbusClientHub::clear_tx_queue_for_device(ModbusClientDevice *device) {
  // Silent teardown (supersede semantics): the caller's own frames vanish without callbacks; see
  // the lifecycle note on ModbusClientDevice.
  for (auto &cmd : this->tx_buffer_) {
    if (cmd.device != device)
      continue;
    cmd.silent_retire();
    this->sweep_needed_ = true;
  }
}

void ModbusClientHub::send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device) {
  if (payload.size() < 2) {
    ESP_LOGW(TAG, "send_raw() payload too short to contain a PDU, refused");
    return;
  }
  this->queue_pdu(payload[0], std::span<const uint8_t>(payload).subspan(1), device);
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

  // If blocked now (frame delay not elapsed at low baud, or a frame arriving), defer rather than
  // busy-waiting the loop; send_frame_ itself re-checks after its delay, so the deferred callback
  // just reports whatever it returns.
  if (this->tx_blocked()) {
    // Stash the raw payload in a single member buffer so the deferred callback can rebuild the frame
    // without a heap allocation. Only one server reply is ever waiting, so a single buffer suffices.
    std::memcpy(this->deferred_payload_.data(), payload, len);
    this->deferred_payload_len_ = len;
    this->set_timeout("deferred_send", this->tx_delay_remaining(), [this]() {
      ModbusFrame frame(this->deferred_payload_[0], this->deferred_payload_.data() + 1,
                        this->deferred_payload_len_ - 1);
      if (!this->send_frame_(frame))
        ESP_LOGE(TAG, "Deferred server reply dropped: transmission still blocked");
    });
    return;
  }

  ModbusFrame frame(payload[0], payload + 1, len - 1);
  if (!this->send_frame_(frame))
    ESP_LOGE(TAG, "Server reply dropped: a frame arrived during the send delay");
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
  if (!custom && succeeded(status)) {
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
    case FunctionCode::READ_INPUT_REGISTERS:
    // FC 0x17 lands here too: its read start address and read quantity sit at the same request offsets as a
    // plain read's (bytes 1..2 and 3..4), so start_address and count_or_value already hold the read block; its
    // response carries only that read data, and the write half is confirmed by the response arriving at all.
    // An exception routes here as well (the gate only validates the request when status is set), delivering
    // empty registers with the error in status - so a 0x17 subclass handles success and failure in the one
    // on_read_holding_registers() callback and never needs to also override on_error().
    case FunctionCode::READ_WRITE_MULTIPLE_REGISTERS: {
      // Decode the big-endian register words into host byte order. The gate guarantees a success response
      // carries exactly count_or_value registers (and count_or_value <= MAX_NUM_OF_REGISTERS_TO_READ, the
      // capacity of RegisterValues); a mismatch was diverted to on_custom_response(), never clamped. On
      // failure the registers span is empty.
      RegisterValues registers;
      if (succeeded(status)) {
        for (size_t i = 0; i != count_or_value; i++) {
          registers.push_back(helpers::get_data<uint16_t>(response_pdu.data(), 2 + 2 * i));
        }
      }
      std::span<const uint16_t> register_span(registers.data(), registers.size());
      if (function_code == FunctionCode::READ_INPUT_REGISTERS) {
        this->on_read_input_registers(start_address, register_span, status);
      } else if (function_code == FunctionCode::READ_HOLDING_REGISTERS ||
                 function_code == FunctionCode::READ_WRITE_MULTIPLE_REGISTERS) {
        this->on_read_holding_registers(start_address, register_span, status);
      } else {
        // Unreachable for the current case labels; match explicitly so a function code added to this group
        // later is diverted to on_custom_response() rather than silently delivered as a holding read.
        this->on_custom_response(request_pdu, response_pdu, status);
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
      if (succeeded(status)) {
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
      const uint16_t value = (succeeded(status) && response_pdu.size() >= WRITE_SINGLE_PDU_SIZE)
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
