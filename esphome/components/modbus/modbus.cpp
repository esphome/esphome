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

  this->frame_delay_ms_ =
      std::max(2,  // 1750us minimium per spec - rounded up to 2ms.
                   // 3.5 characters * 11 bits per character * 1000ms/sec / (bits/sec) (Standard modbus frame delay)
               (uint16_t) (3.5 * 11 * 1000 / this->parent_->get_baud_rate()) + 1);

  this->long_rx_buffer_delay_ms_ =
      (this->parent_->get_rx_full_threshold() * 11 * 1000 / this->parent_->get_baud_rate()) + 1;
}

void ModbusClient::loop() {
  // First process all available incoming data.
  this->receive_and_parse_modbus_bytes_();

  // If the response frame is finished (including interframe delay) - we timeout.
  // The long_rx_buffer_delay accounts for long responses (larger than the UART rx_full_threshold) to avoid timeouts
  // when the buffer is filling the back half of the response
  const uint16_t timeout = std::max(
      (uint16_t) this->frame_delay_ms_,
      (uint16_t) (this->rx_buffer_.size() > this->parent_->get_rx_full_threshold() - 1 ? this->long_rx_buffer_delay_ms_
                                                                                       : 0));
  // We use millis() here and elsewhere instead of App.get_loop_component_start_time() to avoid stale timestamps
  // It's critical in all timestamp comparisons that the left timestamp comes before the right one in time
  // If we use a cached value in place of millis() and last_modbus_byte_ is updated inside our loop
  // then the comparison is backwards (small negative which wraps to large positive) and will cause a false timeout
  // So in this component we don't use any cached timestamp values to avoid these annoying bugs
  if (millis() - this->last_modbus_byte_ > timeout) {
    clear_rx_buffer_("timeout after partial response", true);
  }

  //  If we're past the send_wait_time timeout and response buffer doesn't have the start of the expected response
  if (this->waiting_for_response_.has_value() &&
      millis() - this->last_send_ > this->last_send_tx_offset_ + this->send_wait_time_ &&
      (this->rx_buffer_.empty() || this->rx_buffer_[0] != this->waiting_for_response_.value().frame[0])) {
    ESP_LOGW(TAG, "Stop waiting for response from %d %dms after last send",
             this->waiting_for_response_.value().frame[0], millis() - this->last_send_);
    if (this->waiting_for_response_.value().device)
      this->waiting_for_response_.value().device->on_modbus_no_response();
    this->waiting_for_response_.reset();
  }

  //  If there's no response pending and there's commands in the buffer
  if (!this->tx_blocked() && !this->tx_buffer_.empty())
    this->defer("send_next_frame", [this]() { this->send_next_frame_(); });
}

void ModbusServer::loop() {
  // First process all available incoming data.
  this->receive_and_parse_modbus_bytes_();

  // If the response frame is finished (including interframe delay) - we timeout.
  // The long_rx_buffer_delay accounts for long responses (larger than the UART rx_full_threshold) to avoid timeouts
  // when the buffer is filling the back half of the response
  const uint16_t timeout = std::max(
      (uint16_t) this->frame_delay_ms_,
      (uint16_t) (this->rx_buffer_.size() > this->parent_->get_rx_full_threshold() - 1 ? this->long_rx_buffer_delay_ms_
                                                                                       : 0));
  // We use millis() here and elsewhere instead of App.get_loop_component_start_time() to avoid stale timestamps
  // It's critical in all timestamp comparisons that the left timestamp comes before the right one in time
  // If we use a cached value in place of millis() and last_modbus_byte_ is updated inside our loop
  // then the comparison is backwards (small negative which wraps to large positive) and will cause a false timeout
  // So in this component we don't use any cached timestamp values to avoid these annoying bugs
  if (millis() - this->last_modbus_byte_ > timeout) {
    if (this->rx_buffer_.size() > 0 && this->expecting_peer_response_ != 0) {
      ESP_LOGV(TAG, "Stop waiting for peer response from %d", this->expecting_peer_response_);
      this->expecting_peer_response_ = 0;
      this->parse_modbus_client_byte_(std::nullopt);  // try re-parse
    } else
      clear_rx_buffer_("timeout after partial response", true);
  }
}

bool Modbus::tx_blocked() {
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

bool ModbusClient::tx_blocked() {
  // We block transmission in any of these case:
  // 1. We're waiting for a response
  // 2. Any of the base class tx_blocked conditions
  return (this->waiting_for_response_.has_value()) || this->Modbus::tx_blocked();
}

bool ModbusClient::tx_buffer_empty() { return this->tx_buffer_.empty(); }

void Modbus::receive_and_parse_modbus_bytes_() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    if (this->rx_buffer_.empty()) {
      ESP_LOGV(TAG, "Received first byte %d (0X%x) %dms after last send", byte, byte, millis() - this->last_send_);
    } else {
      ESP_LOGVV(TAG, "Received byte %d (0X%x) %dms after last send", byte, byte, millis() - this->last_send_);
    }

    this->last_modbus_byte_ = millis();
    this->parse_modbus_byte_(byte);
  }
}

void ModbusClient::parse_modbus_byte_(uint8_t byte) {
  if (!this->parse_modbus_server_byte_(byte))
    this->clear_rx_buffer_("parse failed", true);
}

void ModbusServer::parse_modbus_byte_(uint8_t byte) {
  if (this->expecting_peer_response_ != 0) {
    if (!this->parse_modbus_server_byte_(byte)) {
      ESP_LOGV(TAG, "Stop expecting peer response from %d due to parse failure, and retry parse",
               this->expecting_peer_response_);
      this->expecting_peer_response_ = 0;
      if (!this->parse_modbus_client_byte_(std::nullopt)) {
        this->clear_rx_buffer_("parse failed", true);
      }
    }

  } else {
    if (!this->parse_modbus_client_byte_(byte))
      this->clear_rx_buffer_("parse failed", true);
  }
}

// TODO: Clean up the function - it's quite long and has some repeated code paths.
bool Modbus::parse_modbus_server_byte_(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  if (at >= MAX_FRAME_SIZE) {
    ESP_LOGW(TAG, "RX buffer exceeded max frame size of %d bytes", MAX_FRAME_SIZE);
    return false;
  }
  this->rx_buffer_.push_back(byte);

  // Smallest possible frame is 5 bytes: address(1) + function(1) + exception(1) + CRC(2)
  if (at < 4)
    return true;

  const uint8_t *raw = &this->rx_buffer_[0];

  uint8_t address = raw[0];
  uint8_t function_code = raw[1];

  uint8_t data_len = raw[2];
  uint8_t data_offset = 3;

  if (is_function_code_custom(function_code)) {
    // Handle user-defined function, since we don't know how big this ought to be,
    // ideally we should delegate the entire length detection to whatever handler is
    // installed, but wait, there is the CRC, and if we get a hit there is a good
    // chance that this is a complete message ... admittedly there is a small chance is
    // isn't but that is quite small given the purpose of the CRC in the first place

    data_offset = 2;
    data_len = at - 2;

    uint16_t computed_crc = crc16(raw, data_offset + data_len);
    uint16_t remote_crc = get_data<uint16_t>(this->rx_buffer_, data_offset + data_len);

    if (computed_crc != remote_crc)
      return true;

    ESP_LOGD(TAG, "User-defined function %02X found", function_code);

  } else {
    // the response for write command mirrors the requests and data starts at offset 2 instead of 3 for read commands
    if (is_function_code_write(function_code)) {
      data_offset = 2;
      data_len = 4;
    }

    // Error ( msb indicates error )
    // response format:  Byte[0] = device address, Byte[1] function code | 0x80 , Byte[2] exception code, Byte[3-4] crc
    if (is_function_code_exception(function_code)) {
      data_offset = 2;
      data_len = 1;
    }

    // Byte 3+data_len: CRC_LO (over all bytes)
    if (at <= data_offset + data_len)
      return true;

    // Byte data_offset+len+1: CRC_HI (over all bytes)
    uint16_t computed_crc = crc16(raw, data_offset + data_len);
    uint16_t remote_crc = get_data<uint16_t>(this->rx_buffer_, data_offset + data_len);
    if (computed_crc != remote_crc) {
      return false;
    }
  }

  std::vector<uint8_t> data(this->rx_buffer_.begin() + data_offset, this->rx_buffer_.begin() + data_offset + data_len);

  this->clear_rx_buffer_("parse succeeded");

  this->process_modbus_server_frame_(address, function_code, data);

  return true;
}

bool ModbusServer::parse_modbus_client_byte_(std::optional<uint8_t> byte) {
  size_t at = this->rx_buffer_.size();
  if (at >= MAX_FRAME_SIZE) {
    ESP_LOGW(TAG, "RX buffer exceeded max frame size of %d bytes", MAX_FRAME_SIZE);
    return false;
  }
  if (byte.has_value())
    this->rx_buffer_.push_back(byte.value());
  else if (at > 0)
    at--;  // we're being called to re-parse existing buffer

  if (at < 4)
    return true;
  const uint8_t *raw = &this->rx_buffer_[0];

  uint8_t address = raw[0];
  uint8_t function_code = raw[1];

  if (this->expecting_peer_response_ != 0 && address != this->expecting_peer_response_) {
    ESP_LOGVV(TAG, "Received frame with address %d while expecting response from %d. Assume new client command.",
              address, this->expecting_peer_response_);
    this->expecting_peer_response_ = 0;
  }

  uint8_t data_len = raw[2];
  uint8_t data_offset = 3;

  if (is_function_code_custom(function_code)) {
    // Handle user-defined function, since we don't know how big this ought to be,
    // ideally we should delegate the entire length detection to whatever handler is
    // installed, but wait, there is the CRC, and if we get a hit there is a good
    // chance that this is a complete message ... admittedly there is a small chance is
    // isn't but that is quite small given the purpose of the CRC in the first place

    data_offset = 2;
    data_len = at - 2;

    uint16_t computed_crc = crc16(raw, data_offset + data_len);
    uint16_t remote_crc = get_data<uint16_t>(this->rx_buffer_, data_offset + data_len);

    if (computed_crc != remote_crc)
      return true;

    ESP_LOGD(TAG, "User-defined function %02X found", function_code);

  } else {
    // data starts at 2 and length is 4 for read registers commands

    if (is_function_code_read(function_code) || function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER) {
      data_offset = 2;
      data_len = 4;
    } else if (function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
      if (at < 6) {
        return true;
      }
      data_offset = 2;
      // starting address (2 bytes) + quantity of registers (2 bytes) + byte count itself (1 byte) + actual byte count
      data_len = 2 + 2 + 1 + raw[6];
    }

    // Byte 3+data_len: CRC_LO (over all bytes)
    if (at <= data_offset + data_len)
      return true;

    // Byte data_offset+len+1: CRC_HI (over all bytes)
    uint16_t computed_crc = crc16(raw, data_offset + data_len);
    uint16_t remote_crc = get_data<uint16_t>(this->rx_buffer_, data_offset + data_len);
    if (computed_crc != remote_crc) {
      // Don't log CRC errors for expected responses from peers - we'll try again first
      if (this->expecting_peer_response_ == 0) {
        ESP_LOGW(TAG, "CRC check failed %dms after last send", millis() - this->last_send_);
        ESP_LOGVV(TAG, "  (%02X != %02X) %s", computed_crc, remote_crc, format_hex_pretty(this->rx_buffer_).c_str());
      }
      return false;
    }
  }

  std::vector<uint8_t> data(this->rx_buffer_.begin() + data_offset, this->rx_buffer_.begin() + data_offset + data_len);

  this->clear_rx_buffer_("parse succeeded");

  this->process_modbus_client_frame_(address, function_code, data);

  return true;
}

void ModbusClient::process_modbus_server_frame_(uint8_t address, uint8_t function_code,
                                                const std::vector<uint8_t> &data) {
  if (!this->waiting_for_response_.has_value()) {
    ESP_LOGW(TAG, "Received unexpected frame from address %d, function code 0x%X, %dms after last send", address,
             function_code, millis() - this->last_send_);
    return;
  } else {  // We are waiting for a response
    // Check if the response matches the expected address and function code

    uint8_t expected_address = this->waiting_for_response_.value().frame[0];
    uint8_t expected_function_code = this->waiting_for_response_.value().frame[1];
    if (expected_address != address || expected_function_code != (function_code & FUNCTION_CODE_MASK)) {
      ESP_LOGW(TAG, "Received incorrect frame address %d <> %d or function code 0x%X <> 0x%X, %dms after last send",
               address, expected_address, (function_code & FUNCTION_CODE_MASK), expected_function_code,
               millis() - this->last_send_);
      // Invalidate the waiting device so it won't process this response.
      this->waiting_for_response_.value().device->on_modbus_no_response();
      this->waiting_for_response_.value().device = nullptr;
      return;
    }
    ModbusClientDevice *device = this->waiting_for_response_.value().device;
    if (device == nullptr) {
      ESP_LOGW(
          TAG,
          "Ignoring response from %d - transmission interrupted by previous unexpected response, %dms after last send",
          address, millis() - this->last_send_);
      return;
    } else {  // We have a valid device waiting for this response

      // Is it an error response?
      if ((function_code & FUNCTION_CODE_EXCEPTION_MASK) == FUNCTION_CODE_EXCEPTION_MASK) {
        uint8_t exception = data[0];
        ESP_LOGW(TAG, "Error function code: 0x%X exception: %d, address: %d, %dms after last send", function_code,
                 exception, address, millis() - this->last_send_);
        device->on_modbus_error(function_code & FUNCTION_CODE_MASK, exception);

      } else {  // Not an error response
        device->on_modbus_data(data);
      }
      this->waiting_for_response_.reset();
    }
  }
}

void ModbusServer::process_modbus_server_frame_(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &) {
  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      ESP_LOGE(TAG, "Unexpected response from address %d, which is mapped to this device.", address);
    }
  }

  if (this->expecting_peer_response_ == address)
    ESP_LOGV(TAG, "Expected response from peer %d received", address);
  else
    ESP_LOGV(TAG, "Unexpected response from peer %d received", address);

  this->expecting_peer_response_ = 0;
}

void ModbusServer::process_modbus_client_frame_(uint8_t address, uint8_t function_code,
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
      }
    }
  }

  if (!found) {
    this->expecting_peer_response_ = address;
    ESP_LOGV(TAG, "Request to peer %d received", address);
  }
}

void Modbus::send_frame_(const std::vector<uint8_t> &data) {
  if (this->tx_blocked()) {
    ESP_LOGE(TAG, "Attempted to send while transmission blocked");
    return;
  }
  if (data.size() > MAX_FRAME_SIZE) {
    ESP_LOGE(TAG, "Attempted to send frame larger than max frame size of %d bytes", MAX_FRAME_SIZE);
    return;
  }

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
    this->write_array(data);
    this->flush();
    this->flow_control_pin_->digital_write(false);
    this->last_send_tx_offset_ = 0;
  } else {
    this->write_array(data);
    this->last_send_tx_offset_ = data.size() * 11 * 1000 / this->parent_->get_baud_rate() + 1;
  }

  ESP_LOGV(TAG, "Write: %s %dms after last send, %dms after last receive", format_hex_pretty(data).c_str(),
           millis() - this->last_send_, millis() - this->last_modbus_byte_);
  this->last_send_ = millis();
}

void ModbusClient::send_next_frame_() {
  if (this->tx_buffer_.empty()) {
    ESP_LOGE(TAG, "Attempted to send from empty tx buffer");
    return;
  }

  if (this->tx_blocked()) {
    ESP_LOGE(TAG, "Attempted to send while transmission blocked");
    return;
  }

  ModbusDeviceCommand command = this->tx_buffer_.front();

  this->send_frame_(command.frame);

  this->waiting_for_response_ = std::move(command);

  this->tx_buffer_.pop_front();

  if (!this->tx_buffer_.empty()) {
    ESP_LOGV(TAG, "Write queue contains %d items.", this->tx_buffer_.size());
  }
}

void ModbusClient::dump_config() {
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
void ModbusServer::dump_config() {
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

void ModbusClient::send(ModbusClientDevice *device, uint8_t address, uint8_t function_code, uint16_t start_address,
                        uint16_t number_of_entities, uint8_t payload_len, const uint8_t *payload) {
  ESP_LOGVV(TAG,
            "ModbusClient::send address=%d function_code=0x%X start_address=%d number_of_entities=%d "
            "payload_len=%d %s",
            address, function_code, start_address, number_of_entities, payload_len,
            payload != nullptr ? format_hex_pretty(std::vector<uint8_t>(payload, payload + payload_len)).c_str()
                               : "no payload");

  // Only check max number of registers for standard function codes
  // Some devices use non standard codes like 0x43
  if (number_of_entities > MAX_NUM_OF_REGISTERS_TO_WRITE &&
      function_code <= ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
    ESP_LOGE(TAG, "send too many values %d max=%zu", number_of_entities, MAX_NUM_OF_REGISTERS_TO_READ);
    return;
  }

  std::vector<uint8_t> data;
  data.push_back(address);
  data.push_back(function_code);

  data.push_back(start_address >> 8);
  data.push_back(start_address >> 0);
  if (function_code != ModbusFunctionCode::WRITE_SINGLE_COIL &&
      function_code != ModbusFunctionCode::WRITE_SINGLE_REGISTER) {
    data.push_back(number_of_entities >> 8);
    data.push_back(number_of_entities >> 0);
  }

  if (payload != nullptr) {
    if (function_code == ModbusFunctionCode::WRITE_MULTIPLE_COILS ||
        function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {  // Write multiple
      data.push_back(payload_len);                                        // Byte count is required for write
    } else {
      payload_len = 2;  // Write single register or coil
    }
    for (int i = 0; i < payload_len; i++) {
      data.push_back(payload[i]);
    }
  }

  this->send_raw(device, data);
}

void ModbusServer::send(uint8_t address, uint8_t function_code, std::vector<uint8_t> &&payload) {
  payload.insert(payload.begin(), std::initializer_list<uint8_t>{address, function_code});
  this->send_raw(payload);
}

// Helper function for lambdas
// Send raw command for client pushes to queue. Except CRC everything must be contained in payload
void ModbusClient::send_raw(ModbusClientDevice *device, const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }
  std::vector<uint8_t> frame = this->add_crc_to_payload_(payload);

  for (auto item : this->tx_buffer_) {
    if (item.frame == frame) {
      ESP_LOGW(TAG, "Frame already in tx queue, dropping: %s", format_hex_pretty(frame).c_str());
      return;
    }
  }

  if (this->tx_buffer_.size() < MODBUS_TX_BUFFER_SIZE) {
    ESP_LOGV(TAG, "Adding frame to tx queue: %s", format_hex_pretty(frame).c_str());
    this->tx_buffer_.push_back({device, frame});
  } else {
    ESP_LOGE(TAG, "Write buffer full, dropped: %s", format_hex_pretty(frame).c_str());
  }
}

void ModbusClient::clear_tx_queue_for_address(uint8_t address, bool clear_sent) {
  // Remove any pending commands for this address from the tx buffer
  auto &tx_buffer = this->tx_buffer_;
  tx_buffer.erase(std::remove_if(tx_buffer.begin(), tx_buffer.end(),
                                 [this, address](const ModbusDeviceCommand &cmd) { return cmd.frame[0] == address; }),
                  tx_buffer.end());

  if (clear_sent && this->waiting_for_response_.has_value() && this->waiting_for_response_.value().device) {
    if (this->waiting_for_response_.value().frame[0] == address) {
      ESP_LOGV(TAG, "Clearing waiting for response for address %d", address);
      // Invalidate the waiting device so it won't process a response.
      this->waiting_for_response_.value().device = nullptr;
    }
  }
}

// Send raw command for server replies immediately. Except CRC everything must be contained in payload
void ModbusServer::send_raw(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }
  std::vector<uint8_t> frame = this->add_crc_to_payload_(payload);

  // TODO: Make sure this is delayed until tx is unblocked
  delay(5);
  this->send_frame_(frame);
}

const std::vector<uint8_t> Modbus::add_crc_to_payload_(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> data = payload;
  auto crc = crc16(data.data(), data.size());
  data.push_back(crc >> 0);
  data.push_back(crc >> 8);
  return data;
}

void Modbus::clear_rx_buffer_(const std::string &reason, bool warn) {
  size_t at = this->rx_buffer_.size();
  if (at > 0) {
    if (warn) {
      ESP_LOGW(TAG, "Clearing buffer of %d bytes - %s %dms after last send", at, reason.c_str(),
               millis() - this->last_send_);
    } else {
      ESP_LOGV(TAG, "Clearing buffer of %d bytes - %s %dms after last send", at, reason.c_str(),
               millis() - this->last_send_);
    }
    this->rx_buffer_.clear();
  }
}

}  // namespace modbus
}  // namespace esphome
