#include "modbus_utils.h"
#include "esphome/core/log.h"

namespace esphome {
namespace simpleevse {

extern const char * const TAG;

static uint16_t crc16(std::vector<uint8_t>::const_iterator start, std::vector<uint8_t>::const_iterator end) {
  const uint16_t initial{0xFFFF};
  const uint16_t polynom{0xA001};

  uint16_t crc = std::accumulate(start, end, initial, [](uint16_t crc, uint8_t value) {
    crc ^= value;
    for (uint8_t i = 0; i < 8; i++) {
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= polynom;
      } else {
        crc >>= 1;
      }
    }
    return crc;
  });

  // swap bytes
  return ((crc << 8) & 0xff00) | ((crc >> 8) & 0x00ff);
}

std::vector<uint8_t> ModbusTransaction::encode_request() {
  std::vector<uint8_t> buffer;

  // Reserve space to avoid reallocations
  buffer.reserve(this->request_size_ + this->frame_size_);

  BufferStreamWriter writer(std::back_inserter(buffer));

  // Address | Fuction
  writer << MODBUS_ADDRESS << this->function_code_;

  // Data
  this->on_get_data(writer);

  // CRC
  uint16_t crc = crc16(buffer.cbegin(), buffer.cend());
  writer << crc;

  ESP_LOGD(TAG, "Sending request with function=0x%02X.", this->function_code_);
  this->send_time_ = millis();
  this->attempt_--;

  return buffer;
}

bool ModbusTransaction::decode_response(const std::vector<uint8_t> &buffer) {
  size_t len = buffer.size();
  if (len < MIN_FRAME_LEN) {
    ESP_LOGW(TAG, "Incomplete data received with length %zu.", len);
    return false;
  }

  // check CRC
  uint16_t remote_crc = encode_uint16(buffer[len - 2], buffer[len - 1]);
  uint16_t computed_crc = crc16(buffer.cbegin(), buffer.cend() - 2);
  if (remote_crc != computed_crc) {
    ESP_LOGW(TAG, "Modbus CRC Check failed! Expected 0x%04X, got 0x%04X.", computed_crc, remote_crc);
    return false;
  }

  // Frame should be correct because CRC matched
  BufferStreamReader reader(buffer.cbegin(), buffer.cend() - 2);

  uint8_t address, function;
  reader >> address >> function;
  if (address != MODBUS_ADDRESS) {
    ESP_LOGE(TAG, "Response has invalid address: %d.", address);
    return false;
  }

  if ((function & EXCEPTION_MASK) != this->function_code_) {
    ESP_LOGE(TAG, "Response has invalid function code! Expected 0x%02X, got 0x%02X.", this->function_code_, function);
    return false;
  }

  if (function & (~EXCEPTION_MASK)) {
    ESP_LOGE(TAG, "Exception response: 0x%02X.", buffer[2]);
    this->on_error(ModbusTransactionResult::EXCEPTION);
    return true;
  }

  if (reader.remaining_size() != this->response_size_) {
    ESP_LOGE(TAG, "Invalid response size. Expected %d, got %d.", reader.remaining_size(), this->response_size_);
    return false;
  }

  // handle data
  ESP_LOGD(TAG, "Receive response with function=0x%02X.", function);
  this->on_handle_data(reader);

  // check if to much data was read
  if (reader.error()) {
    ESP_LOGE(TAG, "Too much data was read from the frame.");
  }

  return true;
}

void ModbusReadHoldingRegistersTransaction::on_get_data(BufferStreamWriter &writer) {
  // address | register count
  writer << this->address_ << this->count_;
}

void ModbusReadHoldingRegistersTransaction::on_handle_data(BufferStreamReader &reader) {
  // check received size with data size
  uint8_t count;
  reader >> count;
  if (count != reader.remaining_size()) {
    ESP_LOGE(TAG, "Message length does not match received data. Expected %d, got %d.", count, reader.remaining_size());
    return;
  }

  // decode 16-bit registers
  std::vector<uint16_t> regs;
  regs.reserve(this->count_);
  uint16_t reg;
  for (int i = 0; i < this->count_; i++) {
    reader >> reg;
    regs.push_back(reg);
  }

  this->callback_(ModbusTransactionResult::SUCCESS, regs);
}

void ModbusReadHoldingRegistersTransaction::on_error(ModbusTransactionResult result) {
  this->callback_(result, std::vector<uint16_t>());
}

void ModbusWriteHoldingRegistersTransaction::on_get_data(BufferStreamWriter &writer) {
  // address | register count | byte count | value(s)
  writer << this->address_ << uint16_t(1) << uint8_t(2) << this->value_;
}

void ModbusWriteHoldingRegistersTransaction::on_handle_data(BufferStreamReader &reader) {
  this->callback_(ModbusTransactionResult::SUCCESS);
}

void ModbusWriteHoldingRegistersTransaction::on_error(ModbusTransactionResult result) { this->callback_(result); }

void ModbusDeviceComponent::loop() {
  const uint32_t now = micros();

  // collect received bytes into buffer
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    this->rx_buffer_.push_back(byte);
    this->last_modbus_byte_ = now;
    this->after_char_delay_ = true;
  }

  // decode receive buffer after t1.5 silence
  if (!this->rx_buffer_.empty()) {
    if ((now - this->last_modbus_byte_) > this->inter_char_timeout_) {
      if (this->active_transaction_ != nullptr) {
        if (this->active_transaction_->decode_response(this->rx_buffer_)) {
          this->active_transaction_ = nullptr;
        }
      } else {
        ESP_LOGW(TAG, "Receive data but no one is waiting for it.");
      }
      this->rx_buffer_.clear();
    }
  }

  // wait time after last activity
  if (this->after_char_delay_) {
    if ((now - this->last_modbus_byte_) > this->inter_frame_timeout_) {
      this->after_char_delay_ = false;
    }
  }

  // check for response timeout
  if (this->active_transaction_ != nullptr && this->active_transaction_->check_timeout()) {
    ESP_LOGE(TAG, "Timeout waiting for response.");
    if (this->active_transaction_->should_retry()) {
      // most errors result eventually in a timeout so actualy retry handles
      // more cases than just timeouts
      ESP_LOGI(TAG, "Retry request.");
      this->send_request_();
    } else {
      this->active_transaction_->timeout();
      this->active_transaction_ = nullptr;
    }
  }

  // no modbus activity and ready for a new request
  if (!this->after_char_delay_ && !this->active_transaction_) {
    this->idle();
  }
}

void ModbusDeviceComponent::execute_(std::unique_ptr<ModbusTransaction> &&transaction) {
  if (this->active_transaction_ != nullptr) {
    ESP_LOGW(TAG, "Trying to execute a modbus transaction although one is already running.");
    transaction->cancel();
    return;
  }

  this->active_transaction_ = std::move(transaction);
  this->send_request_();
}

void ModbusDeviceComponent::send_request_() {
  std::vector<uint8_t> buffer = this->active_transaction_->encode_request();
  this->write_array(buffer);
  this->after_char_delay_ = true;
  this->last_modbus_byte_ = micros();
}

}  // namespace simpleevse
}  // namespace esphome
