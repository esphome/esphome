#include "modbus.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus {

static const char *const TAG = "modbus";

void Modbus::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }

  // 1750us minimium per spec - rounded up to 2ms.
  // 3.5 characters * 11 bits per character * 1000ms/sec / (bits/sec) (Standard modbus frame delay)
  this->frame_delay_ms_ = std::max(2, (uint16_t) (3.5 * 11 * 1000 / this->parent_->get_baud_rate()) + 1);

  this->long_rx_buffer_delay_ms_ =
      (this->parent_->get_rx_full_threshold() * 11 * 1000 / this->parent_->get_baud_rate()) + 1;
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
  if (this->waiting_for_response_.has_value() &&
      this->last_receive_check_ - this->last_send_ > this->last_send_tx_offset_ + this->send_wait_time_ &&
      (this->rx_buffer_.empty() || this->rx_buffer_[0] != this->waiting_for_response_.value().frame[0])) {
    ESP_LOGW(TAG, "Stop waiting for response from %d %dms after last send",
             this->waiting_for_response_.value().frame[0], this->last_receive_check_ - this->last_send_);
    if (this->waiting_for_response_.value().device)
      this->waiting_for_response_.value().device->on_modbus_no_response();
    this->waiting_for_response_.reset();
  }

  //  If there's no response pending and there's commands in the buffer
  if (!this->tx_blocked() && !this->tx_buffer_.empty())
    this->defer("send_next_frame", [this]() { this->send_next_frame_(); });
}

bool Modbus::timeout_() {
  // If the response frame is finished (including interframe delay) - we timeout.
  // The long_rx_buffer_delay accounts for long responses (larger than the UART rx_full_threshold) to avoid timeouts
  // when the buffer is filling the back half of the response
  const uint16_t timeout = std::max(
      (uint16_t) this->frame_delay_ms_,
      (uint16_t) (this->rx_buffer_.size() > this->parent_->get_rx_full_threshold() - 1 ? this->long_rx_buffer_delay_ms_
                                                                                       : 0));

  return this->last_receive_check_ - this->last_modbus_byte_ > timeout;
}

bool Modbus::tx_blocked() {
  // We use millis() here and elsewhere instead of App.get_loop_component_start_time() to avoid stale timestamps
  // It's critical in all timestamp comparisons that the left timestamp comes before the right one in time
  // If we use a cached value in place of millis() and last_modbus_byte_ is updated inside our loop
  // then the comparison is backwards (small negative which wraps to large positive) and will cause a false timeout
  // So in this component we don't use any cached timestamp values to avoid these annoying bugs
  const uint32_t now = millis();

  // We block transmission in any of these case:
  // 1. There are bytes in the UART Rx buffer
  // 2. There are bytes in our Rx buffer
  // 3. The last sent byte isn't more than frame_delay ms ago (i.e. wait to tell receivers that our previous Tx is done)
  // 4. The last received byte isn't more than frame_delay ms ago (i.e. wait to be sure there isn't more Rx coming)
  // 5. If we're a client - also wait for the turnaround delay, to give the servers time to process the previous message
  return this->available() || !this->rx_buffer_.empty() ||
         (now - this->last_send_ < this->last_send_tx_offset_ + this->frame_delay_ms_ + this->turnaround_delay_ms_) ||
         (now - this->last_modbus_byte_ < this->frame_delay_ms_ + this->turnaround_delay_ms_);
}

bool ModbusClientHub::tx_blocked() {
  // We block transmission in any of these case:
  // 1. We're waiting for a response
  // 2. Any of the base class tx_blocked conditions
  return (this->waiting_for_response_.has_value()) || this->Modbus::tx_blocked();
}

bool ModbusClientHub::tx_buffer_empty() { return this->tx_buffer_.empty(); }

void Modbus::receive_bytes_() {
  bool bytes_received = false;
  while (this->available()) {
    bytes_received = true;
    uint8_t byte;
    this->read_byte(&byte);
    if (this->rx_buffer_.empty()) {
      ESP_LOGV(TAG, "Received first byte %d (0X%x) %dms after last send", byte, byte, millis() - this->last_send_);
    } else {
      ESP_LOGVV(TAG, "Received byte %d (0X%x) %dms after last send", byte, byte, millis() - this->last_send_);
    }
    this->rx_buffer_.push_back(byte);
  }
  this->last_receive_check_ = millis();
  if (bytes_received) {
    this->last_modbus_byte_ = this->last_receive_check_;
  }
}

void ModbusClientHub::parse_modbus_frames() {
  if (!this->rx_buffer_.empty()) {
    size_t size;
    do {
      size = this->rx_buffer_.size();
      if (!this->parse_modbus_server_frame_())
        this->clear_rx_buffer_("parse failed", true);
    } while (!this->rx_buffer_.empty() && size > this->rx_buffer_.size());
    if (this->timeout_())
      this->clear_rx_buffer_("timeout after partial response", true);
  }
}

void ModbusServerHub::parse_modbus_frames() {
  if (!this->rx_buffer_.empty()) {
    size_t size;
    do {
      size = this->rx_buffer_.size();
      ESP_LOGVV(TAG, "Parsing frames buffer size = %d ", size);
      if (this->expecting_peer_response_ != 0) {
        if (!this->parse_modbus_server_frame_()) {
          ESP_LOGV(TAG, "Stop expecting peer response from %d due to parse failure, and retry parse",
                   this->expecting_peer_response_);
          this->expecting_peer_response_ = 0;
          size++;  // force retry parse as client frame
        } else if (this->timeout_() && size == this->rx_buffer_.size()) {
          // If we timed out and the above parse attempt did not then we also stop expecting a response
          ESP_LOGV(TAG, "Stop expecting peer response from %d due to timeout after partial response, and retry parse",
                   this->expecting_peer_response_);
          this->expecting_peer_response_ = 0;
          size++;  // force retry parse as client frame
        }
      } else {
        if (!this->parse_modbus_client_frame_())
          this->clear_rx_buffer_("parse failed", true);
      }
    } while (!this->rx_buffer_.empty() && size > this->rx_buffer_.size());
    if (this->timeout_())
      this->clear_rx_buffer_("timeout after partial response", true);
  }
}

bool Modbus::parse_modbus_server_frame_() {
  size_t size = this->rx_buffer_.size();
  uint16_t frame_length = server_frame_length(this->rx_buffer_);

  if (size < frame_length)
    return true;

  const uint8_t *raw = &this->rx_buffer_[0];

  uint8_t address = this->rx_buffer_[0];
  uint8_t function_code = this->rx_buffer_[1];

  if (is_function_code_custom(function_code)) {
    // Custom functions could be any length - we have to rely on the CRC to determine completeness.
    // If a CRC match is never found, the buffer will eventually overflow and be cleared.
    bool found = false;
    for (; frame_length <= std::min(size, size_t(MAX_FRAME_SIZE)); frame_length++) {
      if (crc16(raw, frame_length) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return size < MAX_FRAME_SIZE;  // Continue to parse until we hit max size

    ESP_LOGD(TAG, "User-defined function %02X found", function_code);
  } else {
    if (crc16(raw, frame_length) != 0) {
      return false;
    }
  }

  // We have a valid frame
  uint8_t data_offset = server_frame_data_offset(this->rx_buffer_);
  std::vector<uint8_t> data(this->rx_buffer_.begin() + data_offset, this->rx_buffer_.begin() + frame_length - 2);

  this->clear_rx_buffer_("parse succeeded", false, frame_length);

  this->process_modbus_server_frame(address, function_code, data);

  return true;
}

bool ModbusServerHub::parse_modbus_client_frame_() {
  size_t size = this->rx_buffer_.size();
  uint16_t frame_length = client_frame_length(this->rx_buffer_);

  if (size < frame_length)
    return true;

  const uint8_t *raw = &this->rx_buffer_[0];

  uint8_t address = raw[0];
  uint8_t function_code = raw[1];

  if (is_function_code_custom(function_code)) {
    // Custom functions could be any length - we have to rely on the CRC to determine completeness.
    // If a CRC match is never found, the buffer will eventually overflow and be cleared.
    bool found = false;
    for (; frame_length <= std::min(size, size_t(MAX_FRAME_SIZE)); frame_length++) {
      if (crc16(raw, frame_length) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return size < MAX_FRAME_SIZE;  // Continue to parse until we hit max size

    ESP_LOGD(TAG, "User-defined function %02X found", function_code);
  } else {
    if (crc16(raw, frame_length) != 0) {
      return false;
    }
  }

  // We have a valid frame
  uint8_t data_offset = client_frame_data_offset(this->rx_buffer_);
  std::vector<uint8_t> data(this->rx_buffer_.begin() + data_offset, this->rx_buffer_.begin() + frame_length - 2);

  this->clear_rx_buffer_("parse succeeded", false, frame_length);

  this->process_modbus_client_frame_(address, function_code, data);

  return true;
}

void ModbusClientHub::process_modbus_server_frame(uint8_t address, uint8_t function_code,
                                                  const std::vector<uint8_t> &data) {
  if (!this->waiting_for_response_.has_value()) {
    ESP_LOGW(TAG, "Received unexpected frame from address %d, function code 0x%X, %dms after last send", address,
             function_code, this->last_modbus_byte_ - this->last_send_);
    return;
  } else {  // We are waiting for a response
    // Check if the response matches the expected address and function code

    ModbusDeviceCommand &wfr = this->waiting_for_response_.value();
    uint8_t expected_address = wfr.frame[0];
    uint8_t expected_function_code = wfr.frame[1];
    if (expected_address != address || expected_function_code != (function_code & FUNCTION_CODE_MASK)) {
      ESP_LOGW(TAG, "Received incorrect frame address %d <> %d or function code 0x%X <> 0x%X, %dms after last send",
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
      ESP_LOGW(
          TAG,
          "Ignoring response from %d - transmission interrupted by previous unexpected response, %dms after last send",
          address, this->last_modbus_byte_ - this->last_send_);
      return;
    } else {  // We have a valid device waiting for this response

      this->waiting_for_response_.reset();
      // Is it an error response?
      if (is_function_code_exception(function_code)) {
        uint8_t exception = data[0];
        ESP_LOGW(TAG, "Error function code: 0x%X exception: %d, address: %d, %dms after last send", function_code,
                 exception, address, this->last_modbus_byte_ - this->last_send_);
        if (wfr.device)
          wfr.device->on_modbus_error(function_code & FUNCTION_CODE_MASK, exception);

      } else if (wfr.device) {  // Not an error response
        wfr.device->on_modbus_data(data);
      } else {  // Not an error response, but no device to respond to
        ESP_LOGV(TAG, "Ignoring response from %d - no callback device set, %dms after last send", address,
                 this->last_modbus_byte_ - this->last_send_);
      }
    }
  }
}

void ModbusServerHub::process_modbus_server_frame(uint8_t address, uint8_t function_code,
                                                  const std::vector<uint8_t> &) {
  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      ESP_LOGE(TAG, "Unexpected response from address %d, which is mapped to this device.", address);
    }
  }

  if (this->expecting_peer_response_ == address) {
    ESP_LOGV(TAG, "Expected response from peer %d received", address);
  } else {
    ESP_LOGV(TAG, "Unexpected response from peer %d received", address);
  }

  this->expecting_peer_response_ = 0;
}

void ModbusServerHub::process_modbus_client_frame_(uint8_t address, uint8_t function_code,
                                                   const std::vector<uint8_t> &data) {
  bool found = false;

  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      found = true;

      if (function_code == ModbusFunctionCode::READ_HOLDING_REGISTERS ||
          function_code == ModbusFunctionCode::READ_INPUT_REGISTERS) {
        device->on_modbus_read_registers(function_code, get_data<uint16_t>(data, 0), get_data<uint16_t>(data, 2));
      } else if (function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER ||
                 function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
        device->on_modbus_write_registers(function_code, data);
      } else {
        ESP_LOGW(TAG, "Unsupported function code %d", function_code);
        device->send_error(function_code, ModbusExceptionCode::ILLEGAL_FUNCTION);
      }
    }
  }

  if (!found) {
    this->expecting_peer_response_ = address;
    ESP_LOGV(TAG, "Request to peer %d received", address);
  }
}

bool Modbus::send_frame_(const std::vector<uint8_t> &frame) {
  if (this->tx_blocked()) {
    ESP_LOGE(TAG, "Attempted to send while transmission blocked");
    return false;
  }
  if (frame.size() > MAX_FRAME_SIZE) {
    ESP_LOGE(TAG, "Attempted to send frame larger than max frame size of %d bytes", MAX_FRAME_SIZE);
    return false;
  }

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
    this->write_array(frame);
    this->flush();
    this->flow_control_pin_->digital_write(false);
    this->last_send_tx_offset_ = 0;
  } else {
    this->write_array(frame);
    this->last_send_tx_offset_ = frame.size() * 11 * 1000 / this->parent_->get_baud_rate() + 1;
  }

  uint32_t now = millis();
  ESP_LOGV(TAG, "Write: %s %dms after last send, %dms after last receive", format_hex_pretty(frame).c_str(),
           now - this->last_send_, now - this->last_modbus_byte_);
  this->last_send_ = now;
  return true;
}

void ModbusClientHub::send_next_frame_() {
  if (this->tx_buffer_.empty()) {
    ESP_LOGE(TAG, "Attempted to send from empty tx buffer");
    return;
  }

  if (this->tx_blocked()) {
    ESP_LOGE(TAG, "Attempted to send while transmission blocked");
    return;
  }

  ModbusDeviceCommand command = this->tx_buffer_.front();

  if (!this->send_frame_(command.frame)) {
    if (command.device)
      command.device->on_modbus_not_sent();
  }

  this->waiting_for_response_ = std::move(command);

  this->tx_buffer_.pop_front();

  if (!this->tx_buffer_.empty()) {
    ESP_LOGV(TAG, "Write queue contains %d items.", this->tx_buffer_.size());
  }
}

void ModbusClientHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus:");
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  ESP_LOGCONFIG(TAG,
                "  Send Wait Time: %d ms\n"
                "  Turnaround Time: %d ms\n"
                "  Frame Delay: %d ms\n"
                "  Long Rx Buffer Delay: %d ms",
                this->send_wait_time_, this->turnaround_delay_ms_, this->frame_delay_ms_,
                this->long_rx_buffer_delay_ms_);
}
void ModbusServerHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus:");
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  ESP_LOGCONFIG(TAG,
                "  Frame Delay: %d ms\n"
                "  Long Rx Buffer Delay: %d ms",
                this->frame_delay_ms_, this->long_rx_buffer_delay_ms_);
}

float Modbus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

void ModbusClientHub::send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
                           ModbusClientDevice *device, bool allow_duplicates) {
  ESP_LOGVV(TAG, "ModbusClient::send address=%d function_code=0x%X start_address=%d number_of_entities=%d ", address,
            function_code, start_address, number_of_entities);
  std::vector<uint8_t> data;
  data.push_back(address);
  create_client_pdu(data, (ModbusFunctionCode) function_code, start_address, number_of_entities);
  this->send_raw(data, device, allow_duplicates);
}

void ModbusServerHub::send(uint8_t address, uint8_t function_code, std::vector<uint8_t> &&payload) {
  payload.insert(payload.begin(), std::initializer_list<uint8_t>{address, function_code});
  this->send_raw(payload);
}

// Helper function for lambdas
// Send raw command for client pushes to queue. Except CRC everything must be contained in payload
void ModbusClientHub::send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device, bool allow_duplicates) {
  if (payload.empty()) {
    if (device)
      device->on_modbus_not_sent();
    return;
  }
  std::vector<uint8_t> frame = add_crc_to_payload(payload);

  if (!is_client_frame_length_valid(frame)) {
    ESP_LOGW(TAG, "Frame is incorrect length, sending: %s", format_hex_pretty(frame).c_str());
    // TODO: Decide whether to actually drop frames of incorrect length. How common is this?
    //  if (device)
    //    device->on_modbus_not_sent();
    //  return;
  }

  if (!allow_duplicates) {
    for (const auto &item : this->tx_buffer_) {
      if (item.frame == frame && item.device == device) {
        ESP_LOGW(TAG, "Frame already in tx queue, dropping: %s", format_hex_pretty(frame).c_str());
        if (device)
          device->on_modbus_not_sent();
        return;
      }
    }
  }

  if (this->tx_buffer_.size() < MODBUS_TX_BUFFER_SIZE) {
    ESP_LOGV(TAG, "Adding frame to tx queue: %s", format_hex_pretty(frame).c_str());
    this->tx_buffer_.push_back({device, frame});
  } else {
    ESP_LOGE(TAG, "Write buffer full, dropped: %s", format_hex_pretty(frame).c_str());
    if (device)
      device->on_modbus_not_sent();
  }
}

void ModbusClientHub::clear_tx_queue_for_address(uint8_t address, bool clear_sent) {
  // Remove any pending commands for this address from the tx buffer
  auto &tx_buffer = this->tx_buffer_;
  tx_buffer.erase(std::remove_if(tx_buffer.begin(), tx_buffer.end(),
                                 [address](const ModbusDeviceCommand &cmd) { return cmd.frame[0] == address; }),
                  tx_buffer.end());

  if (clear_sent && this->waiting_for_response_.has_value() && this->waiting_for_response_.value().device) {
    if (this->waiting_for_response_.value().frame[0] == address) {
      ESP_LOGV(TAG, "Clearing waiting for response for address %d", address);
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

// Send raw command for server replies immediately. Except CRC everything must be contained in payload
void ModbusServerHub::send_raw(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }
  std::vector<uint8_t> frame = add_crc_to_payload(payload);

  const uint32_t now = millis();
  if (now - this->last_modbus_byte_ < this->frame_delay_ms_) {
    this->set_timeout("send_frame", (this->frame_delay_ms_ - (now - this->last_modbus_byte_)),
                      [this, frame] { this->send_frame_(frame); });
  } else {
    this->send_frame_(frame);
  }
}

void Modbus::clear_rx_buffer_(const std::string &reason, bool warn, size_t bytes_to_clear) {
  size_t bytes = this->rx_buffer_.size();
  if (bytes_to_clear > 0 && bytes >= bytes_to_clear)
    bytes = bytes_to_clear;
  if (bytes > 0) {
    if (warn) {
      ESP_LOGW(TAG, "Clearing buffer of %d bytes - %s %dms after last send", bytes, reason.c_str(),
               millis() - this->last_send_);
    } else {
      ESP_LOGV(TAG, "Clearing buffer of %d bytes - %s %dms after last send", bytes, reason.c_str(),
               millis() - this->last_send_);
    }
    if (bytes == this->rx_buffer_.size()) {
      this->rx_buffer_.clear();
    } else {
      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + bytes);
    }
  }
}

}  // namespace modbus
}  // namespace esphome
