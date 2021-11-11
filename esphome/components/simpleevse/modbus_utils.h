#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace simpleevse {

// modbus constants
const uint32_t BAUD_RATE{9600};
const uint8_t STOP_BITS{1};
const uint8_t PARITY_BITS{8};
const size_t MIN_FRAME_LEN{4};    // min. size for evaluation (address + function + 2* CRC)
const uint8_t MODBUS_ADDRESS{1};  // default SimpleEVSE Modbus address
const uint8_t EXCEPTION_MASK{0x7F};
const uint32_t RESPONSE_TIMEOUT{1000};  // timeout in ms

const uint8_t MAX_REQUEST_ATTEMPTS{3};

// Utility class to read data from a received frame
class BufferStreamReader {
 public:
  BufferStreamReader(std::vector<uint8_t>::const_iterator begin, std::vector<uint8_t>::const_iterator end)
      : it_(begin), end_(end) {}

  BufferStreamReader &operator>>(uint8_t &value) {
    if (this->it_ != this->end_) {
      value = *this->it_++;
    } else {
      value = 0;
      this->error_ = true;
    }

    return *this;
  }

  BufferStreamReader &operator>>(uint16_t &value) {
    if ((this->end_ - this->it_) >= 2) {
      uint8_t msb = *this->it_++;
      uint8_t lsb = *this->it_++;
      value = encode_uint16(msb, lsb);
    } else {
      value = 0;
      this->error_ = true;
    }

    return *this;
  }

  uint8_t remaining_size() const { return this->end_ - this->it_; }
  bool error() const { return this->error_; }

 protected:
  std::vector<uint8_t>::const_iterator it_;
  const std::vector<uint8_t>::const_iterator end_;
  bool error_{false};
};

// Utility class to write data to a frame
class BufferStreamWriter {
 public:
  explicit BufferStreamWriter(std::back_insert_iterator<std::vector<uint8_t>> inserter) : inserter_(inserter) {}

  BufferStreamWriter &operator<<(uint8_t value) {
    this->inserter_ = value;
    return *this;
  }

  BufferStreamWriter &operator<<(uint16_t value) {
    auto decoded = decode_value(value);
    this->inserter_ = decoded[0];
    this->inserter_ = decoded[1];
    return *this;
  }

 protected:
  std::back_insert_iterator<std::vector<uint8_t>> inserter_;
};

/// Enumeration of possible modbus transaction results.
enum class ModbusTransactionResult {
  SUCCESS,    // transaction was successfully executed
  EXCEPTION,  // modbus device returned an exception
  TIMEOUT,    // no response from modbus device
  CANCELLED,  // transaction was cancelled by controller
};

/// Base class for all Modbus transactions.
class ModbusTransaction {
 public:
  virtual ~ModbusTransaction() = default;  // important to avoid memory leaks when bases clases are destroyed
  /// returns the complete frame for this request
  std::vector<uint8_t> encode_request();
  /// tries to decode a frame, returns true on success
  bool decode_response(const std::vector<uint8_t> &buffer);
  /// returns true if the timeout for this request is expired
  bool check_timeout() { return (millis() - this->send_time_) >= RESPONSE_TIMEOUT; }
  /// cancels this request
  void cancel() { this->on_error(ModbusTransactionResult::CANCELLED); }
  /// times out this request
  void timeout() { this->on_error(ModbusTransactionResult::TIMEOUT); }
  /// Returns if the request should be retried after a timeout
  bool should_retry() const { return this->attempt_ > 0; }

 protected:
  ModbusTransaction(uint8_t function_code, uint8_t request_size, uint8_t response_size)
      : function_code_(function_code), request_size_(request_size), response_size_(response_size) {}
  // subclasses must insert their request data without header and crc
  virtual void on_get_data(BufferStreamWriter &writer) = 0;
  // subclasses must handle their response data without header and crc
  virtual void on_handle_data(BufferStreamReader &reader) = 0;
  // subclasses must handle exception responses
  virtual void on_error(ModbusTransactionResult result) = 0;

  const uint8_t frame_size_{MIN_FRAME_LEN};
  const uint8_t function_code_;
  const uint8_t request_size_;
  const uint8_t response_size_;

  uint32_t send_time_{0};
  uint8_t attempt_{MAX_REQUEST_ATTEMPTS};
};

/// Implements a Modbus transaction for reading multiple holding registers.
class ModbusReadHoldingRegistersTransaction : public ModbusTransaction {
 public:
  using callback_t = std::function<void(ModbusTransactionResult, const std::vector<uint16_t> &)>;

  ModbusReadHoldingRegistersTransaction(uint16_t address, uint16_t count, callback_t &&callback)
      : ModbusTransaction(0x03, 4, count * 2 + 1), address_(address), count_(count), callback_(std::move(callback)) {}
  ~ModbusReadHoldingRegistersTransaction() override = default;

 protected:
  void on_get_data(BufferStreamWriter &writer) override;
  void on_handle_data(BufferStreamReader &reader) override;
  void on_error(ModbusTransactionResult result) override;

  const uint16_t address_;
  const uint16_t count_;
  const callback_t callback_;
};

/// Implements a Modbus transaction for writing a single holding register.
class ModbusWriteHoldingRegistersTransaction : public ModbusTransaction {
 public:
  using callback_t = std::function<void(ModbusTransactionResult)>;

  // for simplification only one register is implemented even this function provides multiple
  ModbusWriteHoldingRegistersTransaction(uint16_t address, uint16_t value, callback_t &&callback)
      : ModbusTransaction(0x10, 7, 4), address_(address), value_(value), callback_(std::move(callback)) {}
  ~ModbusWriteHoldingRegistersTransaction() override = default;

 protected:
  void on_get_data(BufferStreamWriter &writer) override;
  void on_handle_data(BufferStreamReader &reader) override;
  void on_error(ModbusTransactionResult result) override;

  const uint16_t address_;
  const uint16_t value_;
  const callback_t callback_;
};

/** This class implements a generic Modbus interface based on the UARTDevice.
 *
 * This implementation supports Modbus transactions which are implementations
 * of the ModbusTransaction class and handles all specific parts of the
 * communication. Transaction can be processed by calling the execute() method.
 */
class ModbusDeviceComponent : public uart::UARTDevice, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS - 1.0f; /* after UART */ }
  void loop() override;

 protected:
  /// Executes the transaction, that is sends a request and waits for a response.
  void execute_(std::unique_ptr<ModbusTransaction> &&transaction);
  /// Called when no modbus activity is running and a new transaction can be started
  virtual void idle(){};
  // sends the request from the current active transaction
  void send_request_();

  /// Time in us after a frame is consideres as complete.
  const uint32_t inter_char_timeout_{1563};  // (1 + 8 + 1) * 1.5 / 9600
  /// Time in us of silence before a new frame can be sent.
  const uint32_t inter_frame_timeout_{3646};  // (1 + 8 + 1) * 3.5 / 9600

  /// Buffer for the received data from the UART.
  std::vector<uint8_t> rx_buffer_;
  /// Relative time of the last received byte in us.
  uint32_t last_modbus_byte_{0};
  /// True if the last serial activity is less than the inter frame timeout.
  bool after_char_delay_{false};
  /// Contains the current active transaction.
  std::unique_ptr<ModbusTransaction> active_transaction_{nullptr};
};

}  // namespace simpleevse
}  // namespace esphome
