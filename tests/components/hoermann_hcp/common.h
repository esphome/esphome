#pragma once
#include <chrono>
#include <initializer_list>
#include <thread>
#include <utility>
#include <gtest/gtest.h>
#include "esphome/components/hoermann_hcp/hoermann_hcp.h"

namespace esphome::hoermann_hcp::testing {

using modbus::RegisterValues;

// Register block addresses the Hoermann bus controller polls (see hoermann_hcp.cpp).
constexpr uint16_t COMMAND_REG = 0x9C41;
constexpr uint16_t STATE_REG = 0x9CB9;
constexpr uint16_t BROADCAST_REG = 0x9D31;

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
  using HoermannHcp::set_valid_;
};

}  // namespace esphome::hoermann_hcp::testing
