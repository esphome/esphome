#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "esphome/components/uart/uart_component.h"

namespace esphome::uart::testing {

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::SetArgPointee;

// Derive a mock from UARTComponent to test the wrapper implementations.
class MockUARTComponent : public UARTComponent {
 public:
  using UARTComponent::write_array;
  using UARTComponent::write_byte;

  // NOTE: std::vector is used here for test convenience. For production code,
  // consider using StaticVector or FixedVector from esphome/core/helpers.h instead.
  std::vector<uint8_t> written_data;

  void write_array(const uint8_t *data, size_t len) override { written_data.assign(data, data + len); }

  MOCK_METHOD(bool, read_array, (uint8_t * data, size_t len), (override));
  MOCK_METHOD(bool, peek_byte, (uint8_t * data), (override));
  MOCK_METHOD(int, available, (), (override));
  MOCK_METHOD(void, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
};

}  // namespace esphome::uart::testing
