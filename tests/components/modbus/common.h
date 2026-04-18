#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::modbus::testing {

class TestUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> written_data;
  std::vector<uint8_t> rx_fifo;

  // Modbus::setup() divides by get_baud_rate() when computing frame_delay_ms_, so the
  // mock must report a non-zero baud rate. 9600 is the typical RS485 Modbus default.
  TestUARTComponent() { this->set_baud_rate(9600); }

  void write_array(const uint8_t *data, size_t len) override {
    this->written_data.insert(this->written_data.end(), data, data + len);
  }

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx_fifo.size() < len) {
      return false;
    }
    std::memcpy(data, this->rx_fifo.data(), len);
    this->rx_fifo.erase(this->rx_fifo.begin(), this->rx_fifo.begin() + len);
    return true;
  }

  bool peek_byte(uint8_t *data) override {
    if (this->rx_fifo.empty()) {
      return false;
    }
    *data = this->rx_fifo[0];
    return true;
  }

  size_t available() override { return this->rx_fifo.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

  void inject_rx(const std::vector<uint8_t> &data) {
    this->rx_fifo.insert(this->rx_fifo.end(), data.begin(), data.end());
  }

  void clear_written() { this->written_data.clear(); }
  void clear_rx() { this->rx_fifo.clear(); }
};

class TestModbusDevice : public ModbusDevice {
 public:
  std::vector<std::vector<uint8_t>> received_data;
  int error_count{0};

  void on_modbus_data(const std::vector<uint8_t> &data) override { this->received_data.push_back(data); }

  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override { this->error_count++; }
};

inline std::vector<uint8_t> make_response(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame;
  frame.push_back(address);
  frame.push_back(function_code);
  frame.push_back(static_cast<uint8_t>(payload.size()));
  frame.insert(frame.end(), payload.begin(), payload.end());

  uint16_t crc = crc16(frame.data(), frame.size());
  frame.push_back(crc & 0xFF);
  frame.push_back((crc >> 8) & 0xFF);
  return frame;
}

inline void sleep_ms(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

}  // namespace esphome::modbus::testing
