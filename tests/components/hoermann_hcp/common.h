#pragma once
#include <chrono>
#include <cstring>
#include <initializer_list>
#include <span>
#include <thread>
#include <utility>
#include <vector>
#include <gtest/gtest.h>
#include "esphome/components/hoermann_hcp/hoermann_hcp.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::hoermann_hcp::testing {

using modbus::RegisterValues;

// Register block addresses the Hoermann bus controller polls (see hoermann_hcp.cpp).
constexpr uint16_t COMMAND_REG = 0x9C41;
constexpr uint16_t STATE_REG = 0x9CB9;
constexpr uint16_t BROADCAST_REG = 0x9D31;

// Answer codes mirrored from hoermann_hcp.cpp, so the tests below can name what they assert.
constexpr uint16_t RESPONSE_STATUS = 0x0001;
constexpr uint16_t RESPONSE_PAUSE = 0x0029;
constexpr uint16_t TRANSFER_ACK_ANSWER = 0x04FD;
constexpr uint16_t TRANSFER_NAK_ANSWER = 0x04FE;

// The tests shorten the key-press delay to zero, so the release only needs the millis() clock to tick on.
constexpr auto KEY_PRESS_ELAPSED = std::chrono::milliseconds(2);

inline RegisterValues make_registers(std::initializer_list<uint16_t> values) {
  RegisterValues registers;
  for (uint16_t value : values)
    registers.push_back(value);
  return registers;
}

// A status broadcast carrying the lamp register, which the door reports at index 6.
inline RegisterValues lamp_broadcast(uint16_t lamp_reg) {
  return make_registers({0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, lamp_reg});
}

// The door only accepts commands once the bus controller has actually talked to it.
inline void connect_controller(HoermannHcp &door) {
  door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000}));
}

// Runs one command poll (write 2 / read 8) and returns both key-press registers.
inline std::pair<uint16_t, uint16_t> poll_command(HoermannHcp &door) {
  door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000}));
  RegisterValues response;
  door.on_read_holding_registers(STATE_REG, 8, response);
  EXPECT_EQ(response.size(), 8u);
  if (response.size() != 8u)
    return {0xFFFF, 0xFFFF};
  return {response[2], response[3]};
}

// Presents and then releases the queued command, leaving the slot free.
inline void consume_command(HoermannHcp &door) {
  poll_command(door);
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  poll_command(door);
}

// Exposes the internal timings and the connection bookkeeping, so no test has to wait out a real delay.
class TestableHoermannHcp : public HoermannHcp {
 public:
  TestableHoermannHcp() { this->key_press_delay_ms_ = 0; }

  using HoermannHcp::connection_timeout_ms_;
  using HoermannHcp::is_light_toggle_pending_;
  using HoermannHcp::light_toggle_released_at_;
  using HoermannHcp::light_toggles_in_flight_;
  using HoermannHcp::announcing_;
  using HoermannHcp::pause_ack_timeout_ms_;
  using HoermannHcp::pause_confirmed_;
  using HoermannHcp::pause_quiet_ms_;
  using HoermannHcp::pause_settle_ms_;
  using HoermannHcp::pause_state_;
  using HoermannHcp::pause_total_timeout_ms_;
  using HoermannHcp::set_valid_;
  using HoermannHcp::transfer_answer_pending_;
};

// A UART the test can inject received bytes into and read written bytes back from.
class FakeWire : public uart::UARTComponent {
 public:
  FakeWire() {
    this->set_baud_rate(57600);
    this->set_data_bits(8);
    this->set_stop_bits(1);
    this->set_parity(uart::UART_CONFIG_PARITY_EVEN);
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
    std::memcpy(data, this->rx_.data(), len);
    this->rx_.erase(this->rx_.begin(), this->rx_.begin() + len);
    return true;
  }
  size_t available() override { return this->rx_.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
  void check_logger_conflict() override {}

  // A 0x17 request the way the bus controller sends it: write the command block, read the state block back.
  void inject_poll(uint8_t address, std::initializer_list<uint16_t> command_registers, uint8_t read_count) {
    std::vector<uint8_t> pdu = {0x17,
                                0x9C,
                                0xB9,
                                0x00,
                                read_count,
                                0x9C,
                                0x41,
                                0x00,
                                static_cast<uint8_t>(command_registers.size()),
                                static_cast<uint8_t>(command_registers.size() * 2)};
    for (uint16_t value : command_registers) {
      pdu.push_back(static_cast<uint8_t>(value >> 8));
      pdu.push_back(static_cast<uint8_t>(value & 0xFF));
    }
    const size_t start = this->rx_.size();
    this->rx_.push_back(address);
    this->rx_.insert(this->rx_.end(), pdu.begin(), pdu.end());
    const uint16_t crc = crc16(this->rx_.data() + start, this->rx_.size() - start);
    this->rx_.push_back(crc & 0xFF);
    this->rx_.push_back(crc >> 8);
  }

  // Register i of the first reply since the last clear: address, function code, byte count, then registers.
  uint16_t reply_register(size_t i) const {
    const size_t offset = 3 + 2 * i;
    EXPECT_GE(this->written.size(), offset + 2) << "no register " << i << " on the wire";
    if (this->written.size() < offset + 2)
      return 0xFFFF;
    EXPECT_EQ(this->written[1], 0x17) << "reply is not a read/write response";
    return static_cast<uint16_t>((this->written[offset] << 8) | this->written[offset + 1]);
  }

  std::vector<uint8_t> written;

 private:
  std::vector<uint8_t> rx_;
};

// The wire is never busy, so no reply is held back through the scheduler, which the host test binary has
// no working scheduler for.
class FreeWireHub : public modbus::ModbusServerHub {
 public:
  bool tx_blocked() override { return false; }
  void prime_send_timestamps_for_test() {
    const uint32_t now = millis();
    this->last_modbus_byte_ = now;
    this->last_send_ = now;
  }
};

struct BusFixture {
  BusFixture() {
    hub.set_uart_parent(&wire);
    hub.setup();
    hub.prime_send_timestamps_for_test();
    door.set_address(0x02);
    door.pause_ack_timeout_ms_ = 200;
    door.pause_quiet_ms_ = 1;
    door.pause_settle_ms_ = 5;
    door.pause_total_timeout_ms_ = 500;
    hub.register_device(&door);
    // The controller has to have talked to us, or there is nobody to announce anything to.
    wire.inject_poll(0x02, {0x0000, 0x0000}, 8);
    hub.loop();
    wire.written.clear();
  }

  FakeWire wire;
  FreeWireHub hub;
  TestableHoermannHcp door;
};

}  // namespace esphome::hoermann_hcp::testing
