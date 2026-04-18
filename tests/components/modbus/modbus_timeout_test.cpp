#include "common.h"

namespace esphome::modbus::testing {

class ModbusTimeoutTest : public ::testing::Test {
 protected:
  TestUARTComponent uart_;
  Modbus modbus_;
  TestModbusDevice device_;

  void SetUp() override {
    modbus_.set_uart_parent(&uart_);
    modbus_.set_role(ModbusRole::CLIENT);
    modbus_.set_send_wait_time(20);
    modbus_.set_turnaround_time(0);
    modbus_.set_disable_crc(false);

    device_.set_parent(&modbus_);
    device_.set_address(0x01);
    modbus_.register_device(&device_);

    modbus_.setup();
  }

  void queue_read_(uint16_t reg_addr, uint16_t count = 1) { device_.send(0x04, reg_addr, count); }

  // Returns true if a frame was sent before max_loops was hit. Callers should ASSERT_TRUE
  // on the return value so a silently-stuck queue is treated as a test failure, not a
  // spurious downstream check failure.
  bool pump_until_sent_(int max_loops = 100) {
    uart_.clear_written();
    for (int i = 0; i < max_loops; i++) {
      modbus_.loop();
      if (!uart_.written_data.empty()) {
        return true;
      }
      sleep_ms(1);
    }
    return false;
  }

  void pump_loops_(int count, uint32_t delay_ms = 1) {
    for (int i = 0; i < count; i++) {
      modbus_.loop();
      if (delay_ms > 0) {
        sleep_ms(delay_ms);
      }
    }
  }
};

TEST_F(ModbusTimeoutTest, NormalResponseWithinTimeout) {
  queue_read_(0x0016, 1);
  ASSERT_TRUE(pump_until_sent_());

  uart_.inject_rx(make_response(0x01, 0x04, {0x00, 0x04, 0xD2, 0x00}));
  modbus_.loop();

  ASSERT_EQ(device_.received_data.size(), 1);
  EXPECT_EQ(device_.received_data[0].size(), 4);
}

TEST_F(ModbusTimeoutTest, LateResponseDoesNotGetMisattributedToNextCommand) {
  queue_read_(0x000C, 1);
  queue_read_(0x0079, 1);

  ASSERT_TRUE(pump_until_sent_());

  // 50ms > threshold (10ms tx_offset + 20ms send_wait_time = 30ms). The extra 20ms
  // margin is there so a scheduler-aligned sleep_for(30) that returns at exactly 30ms
  // elapsed doesn't leave the timeout condition on an unstable equality.
  sleep_ms(50);
  modbus_.loop();

  uart_.inject_rx(make_response(0x01, 0x04, {0x00, 0x00, 0x1A, 0xF4}));
  modbus_.loop();

  sleep_ms(30);
  ASSERT_TRUE(pump_until_sent_(50));

  uart_.inject_rx(make_response(0x01, 0x04, {0x00, 0x00, 0x01, 0xFB}));
  modbus_.loop();

  bool found_contract_value = false;
  for (const auto &data : device_.received_data) {
    if (data.size() >= 4) {
      uint32_t value = (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
                       (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
      if (value == 0x00001AF4) {
        found_contract_value = true;
      }
    }
  }

  EXPECT_FALSE(found_contract_value);
}

TEST_F(ModbusTimeoutTest, SplitLateResponseIsIgnoredDuringQuarantine) {
  queue_read_(0x000C, 1);
  queue_read_(0x0079, 1);

  ASSERT_TRUE(pump_until_sent_());

  // 50ms for clear margin above the 30ms timeout threshold (see comment in the
  // LateResponseDoesNotGetMisattributedToNextCommand test).
  sleep_ms(50);
  modbus_.loop();

  const std::vector<uint8_t> full_response = make_response(0x01, 0x04, {0x00, 0x00, 0x1A, 0xF4});
  uart_.inject_rx(std::vector<uint8_t>(full_response.begin(), full_response.begin() + 4));
  modbus_.loop();
  sleep_ms(2);

  uart_.inject_rx(std::vector<uint8_t>(full_response.begin() + 4, full_response.end()));
  modbus_.loop();

  sleep_ms(30);
  ASSERT_TRUE(pump_until_sent_(50));

  uart_.inject_rx(make_response(0x01, 0x04, {0x00, 0x00, 0x01, 0xFB}));
  modbus_.loop();

  bool found_contract_value = false;
  for (const auto &data : device_.received_data) {
    if (data.size() >= 4) {
      uint32_t value = (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
                       (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
      if (value == 0x00001AF4) {
        found_contract_value = true;
      }
    }
  }

  EXPECT_FALSE(found_contract_value);
}

TEST_F(ModbusTimeoutTest, RepeatedTimeoutsDoNotDeadlockQueue) {
  for (int i = 0; i < 5; i++) {
    queue_read_(0x0016 + i, 1);
  }

  // No responses are injected - each queued command must eventually be popped via the
  // wait timeout followed by quarantine. A full cycle is ~(send_wait_time + tx_offset +
  // quarantine_window) ~= 40ms per command, so pump up to 500ms and require the queue
  // to drain completely. If the queue deadlocks, we'd still have frames pending here.
  const uint32_t deadline = millis() + 500;
  while (!modbus_.tx_buffer_empty() && millis() < deadline) {
    modbus_.loop();
    sleep_ms(1);
  }

  EXPECT_TRUE(modbus_.tx_buffer_empty());
}

TEST_F(ModbusTimeoutTest, ResponseJustUnderTimeoutStillDispatchesNormally) {
  queue_read_(0x0016, 1);

  ASSERT_TRUE(pump_until_sent_());
  sleep_ms(15);

  uart_.inject_rx(make_response(0x01, 0x04, {0x00, 0x04, 0xD2, 0x00}));
  modbus_.loop();

  EXPECT_EQ(device_.received_data.size(), 1);
}

}  // namespace esphome::modbus::testing
