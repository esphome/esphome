#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "esphome/components/bmp581_base/bmp581_base.h"
#include "esphome/components/bmp581_spi/bmp581_spi.h"
#include "esphome/components/spi/spi.h"

namespace esphome::bmp581_spi::testing {

// SPI protocol constants for BMP581 (from datasheet section 6.3)
// In SPI mode, the MSB of the register address selects read (1) or write (0).
inline constexpr uint8_t SPI_READ_BIT = 0x80;    // OR with register address for read access
inline constexpr uint8_t SPI_WRITE_MASK = 0x7F;  // AND with register address for write access

// A dummy zero byte sent during reads to clock out the response from the device
inline constexpr uint8_t SPI_DUMMY_BYTE = 0x00;

// Mock SPIDelegate that records the sequence of transfers and returns preset response bytes.
// This allows tests to verify the SPI wire protocol without real hardware.
class MockSPIDelegate : public spi::SPIDelegate {
 public:
  MockSPIDelegate() = default;

  // Preset bytes to return from successive transfer() calls (in order).
  // NOTE: std::vector is used here for test convenience.
  std::vector<uint8_t> response_bytes;
  size_t response_index{0};

  // All bytes passed to transfer(), in order.
  std::vector<uint8_t> sent_bytes;

  // Number of times begin_transaction() / end_transaction() were called.
  int begin_count{0};
  int end_count{0};

  void push_response(uint8_t byte) { response_bytes.push_back(byte); }

  uint8_t transfer(uint8_t data) override {
    sent_bytes.push_back(data);
    if (response_index < response_bytes.size()) {
      return response_bytes[response_index++];
    }
    return SPI_DUMMY_BYTE;
  }

  void begin_transaction() override { begin_count++; }
  void end_transaction() override { end_count++; }

  void reset() {
    response_bytes.clear();
    response_index = 0;
    sent_bytes.clear();
    begin_count = 0;
    end_count = 0;
  }
};

// Subclass of BMP581SPIComponent that exposes protected members for injection in tests.
// delegate_ is protected in SPIClient, so a derived class can set it directly.
// activate_interface() is protected in BMP581SPIComponent; call_activate_interface() is a
// public wrapper so tests can invoke it directly without friend declarations.
class TestBMP581SPIComponent : public BMP581SPIComponent {
 public:
  void set_test_delegate(spi::SPIDelegate *delegate) { this->delegate_ = delegate; }
  void call_activate_interface() { esphome::bmp581_spi::BMP581SPIComponent::activate_interface(); }
};

}  // namespace esphome::bmp581_spi::testing
