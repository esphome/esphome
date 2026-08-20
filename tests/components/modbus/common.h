#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus::testing {

// A UART that discards all writes, for tests that never inspect the wire.
class NullUART : public uart::UARTComponent {
 public:
  NullUART() { this->set_baud_rate(115200); }
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override { return false; }
  size_t available() override { return 0; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
  void check_logger_conflict() override {}
};

// A UART that records every byte written so tests can assert on the exact wire response.
class RecordingUART : public NullUART {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    this->written.insert(this->written.end(), data, data + len);
  }

  std::vector<uint8_t> written;
};

// A UART the test can inject received bytes into, so frames travel the full receive path
// (receive_modbus_frames -> parse -> dispatch) through hub.loop(). Writes are recorded.
class InjectableUART : public RecordingUART {
 public:
  bool peek_byte(uint8_t *data) override {
    if (this->rx_.empty())
      return false;
    *data = this->rx_.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx_.size())
      return false;
    memcpy(data, this->rx_.data(), len);
    this->rx_.erase(this->rx_.begin(), this->rx_.begin() + len);
    return true;
  }
  size_t available() override { return this->rx_.size(); }

  // Queues a complete wire frame: address + PDU + CRC16 (low byte first).
  void inject_frame(uint8_t address, std::span<const uint8_t> pdu) {
    size_t start = this->rx_.size();
    this->rx_.push_back(address);
    this->rx_.insert(this->rx_.end(), pdu.begin(), pdu.end());
    uint16_t crc = crc16(this->rx_.data() + start, this->rx_.size() - start);
    this->rx_.push_back(crc & 0xFF);
    this->rx_.push_back(crc >> 8);
  }

 private:
  std::vector<uint8_t> rx_;
};

}  // namespace esphome::modbus::testing
