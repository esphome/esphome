#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus::testing {

// Lets the test queue received bytes, so frames travel the full receive path through hub.loop().
//
// The sniffer never transmits, so unlike the server tests there is no send path to unblock: no
// tx_blocked() override or primed timestamps, and `written` doubles as the assertion that the
// hub stayed passive.
class SnifferUART : public uart::UARTComponent {
 public:
  SnifferUART() {
    this->set_baud_rate(9600);
    this->set_data_bits(8);
    this->set_stop_bits(1);
    this->set_parity(uart::UART_CONFIG_PARITY_NONE);
  }

  void write_array(const uint8_t *data, size_t len) override {
    this->written.insert(this->written.end(), data, data + len);
  }
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
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
  void check_logger_conflict() override {}

  /// Queues a complete wire frame: address + PDU + CRC16 (low byte first).
  void inject_frame(uint8_t address, std::span<const uint8_t> pdu) {
    size_t start = this->rx_.size();
    this->rx_.push_back(address);
    this->rx_.insert(this->rx_.end(), pdu.begin(), pdu.end());
    uint16_t crc = crc16(this->rx_.data() + start, this->rx_.size() - start);
    this->rx_.push_back(crc & 0xFF);
    this->rx_.push_back(crc >> 8);
  }

  std::vector<uint8_t> written;

 private:
  std::vector<uint8_t> rx_;
};

}  // namespace esphome::modbus::testing
