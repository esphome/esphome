#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/mcp251xfd/mcp251xfd.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome::mcp251xfd::testing {

// Compute the controller's SPI CRC independently of the driver.
inline uint16_t spi_crc(const uint8_t *data, size_t length, uint16_t crc = 0xFFFF) {
  for (size_t i = 0; i < length; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = crc & 0x8000 ? (crc << 1) ^ 0x8005 : crc << 1;
  }
  return crc;
}

struct Write {
  uint16_t address;
  std::vector<uint8_t> data;
};

struct Read {
  uint16_t address;
  size_t length;
  bool operator==(const Read &other) const { return this->address == other.address && this->length == other.length; }
};

class RegisterDelegate : public spi::SPIDelegate {
 public:
  std::array<uint8_t, 0x1000> registers{};
  std::array<unsigned, 0x1000> reads{};
  std::vector<Write> writes;
  std::vector<Read> ram_reads;
  uint16_t corrupt_address{0xFFFF};
  unsigned corrupt_reads{0};
  // With a 4 MHz oscillator, reads above 1 MHz fail CRC while the PLL is off
  bool oscillator_4mhz{false};
  uint32_t data_rate{0};

  void begin_transaction() override {}
  void end_transaction() override {}
  uint8_t transfer(uint8_t data) override { return 0; }

  void transfer(uint8_t *data, size_t length) override {
    ASSERT_GE(length, 5u);
    ASSERT_EQ(data[0] & 0xF0, INSTRUCTION_READ_CRC);
    const uint16_t address = encode_uint16(data[0] & 0x0F, data[1]);
    const size_t data_length = length - 5;
    ASSERT_LE(address + data_length, this->registers.size());
    EXPECT_EQ(data[2], address >= RAM_START && address < RAM_START + RAM_SIZE ? data_length / 4 : data_length);
    this->reads[address]++;
    if (address >= RAM_START)
      this->ram_reads.push_back({address, data_length});

    uint16_t crc = spi_crc(data, 3);
    std::copy_n(this->registers.data() + address, data_length, data + 3);
    if (address == REG_INT && data_length >= 2) {
      if (this->registers[REG_RX_FIFO_STA] & FIFO_RX_OVERFLOW)
        data[4] |= INT_RX_OVERFLOW;
    }
    crc = spi_crc(data + 3, data_length, crc);
    if (address == this->corrupt_address && this->corrupt_reads > 0) {
      this->corrupt_reads--;
      crc ^= 1;
    }
    if (this->oscillator_4mhz && (this->registers[REG_OSC] & OSC_PLL_ENABLE) == 0 && this->data_rate > 1000000)
      crc ^= 1;
    put_be16(data + 3 + data_length, crc);
  }

  void write_array(const uint8_t *data, size_t length) override {
    ASSERT_GE(length, 2u);
    if (data[0] == INSTRUCTION_RESET) {
      this->reset();
      return;
    }
    ASSERT_EQ(data[0] & 0xF0, INSTRUCTION_WRITE);
    const uint16_t address = encode_uint16(data[0] & 0x0F, data[1]);
    ASSERT_LE(address + length - 2, this->registers.size());
    this->writes.push_back({address, {data + 2, data + length}});
    for (size_t i = 2; i < length; i++) {
      const uint16_t reg = address + i - 2;
      if (reg == REG_INT || reg == REG_INT + 1 || reg == REG_RX_FIFO_STA) {
        this->registers[reg] &= data[i];
      } else {
        this->registers[reg] = data[i];
      }
      if (reg == REG_CON + 3)
        this->request_mode(static_cast<CanMode>(data[i] & CON_MODE_MASK));
      // The PLL locks at once
      if (reg == REG_OSC)
        assign_bits(this->registers[REG_OSC + 1], OSC_PLL_READY, data[i] & OSC_PLL_ENABLE);
    }
  }

  void corrupt_next_reads(uint16_t address, unsigned count = 3) {
    this->corrupt_address = address;
    this->corrupt_reads = count;
  }

  CanMode mode() const { return static_cast<CanMode>(this->registers[REG_CON + 2] >> CON_MODE_SHIFT); }
  // Sets the operating and requested modes, as after a completed change.
  void set_mode(CanMode mode) {
    this->registers[REG_CON + 2] = static_cast<uint8_t>(mode) << CON_MODE_SHIFT;
    this->registers[REG_CON + 3] = static_cast<uint8_t>(mode);
  }
  // A request completes at once, but only into or out of Configuration mode or between Listen Only and a Normal mode,
  // like the controller for some changes.
  void request_mode(CanMode mode) {
    const CanMode current = this->mode();
    const auto normal = [](CanMode m) { return m == CanMode::CAN_MODE_NORMAL || m == CanMode::CAN_MODE_CLASSIC; };
    if (mode == CanMode::CAN_MODE_CONFIGURATION || current == CanMode::CAN_MODE_CONFIGURATION ||
        (current == CanMode::CAN_MODE_LISTENONLY && normal(mode)) ||
        (mode == CanMode::CAN_MODE_LISTENONLY && normal(current)))
      this->registers[REG_CON + 2] = static_cast<uint8_t>(mode) << CON_MODE_SHIFT;
  }
  // Like a chip reset: datasheet reset values, Configuration mode, PLL off, oscillator running.
  void reset() {
    this->registers.fill(0);
    put_le32(this->registers.data() + REG_CON, 0x04980760);
    put_le32(this->registers.data() + REG_NBTCFG, 0x003E0F0F);
    put_le32(this->registers.data() + REG_OSC, 0x00000460);
    put_le32(this->registers.data() + REG_TREC, 0x00200000);  // TXBO is set while in Configuration mode
  }
  void set_error_flags(uint8_t flags) { this->registers[REG_TREC + 2] = flags; }

  // Puts a message at the RX FIFO head; payload byte i holds i + 1.
  void queue_rx_message(uint32_t offset, uint8_t flags, size_t payload_length) {
    this->registers[REG_RX_FIFO_STA] = FIFO_NOT_FULL_NOT_EMPTY;
    put_le32(this->registers.data() + REG_RX_FIFO_STA + 4, offset);
    uint8_t *message = this->registers.data() + RAM_START + offset;
    put_le32(message, 0x123);
    message[4] = flags;
    for (size_t i = 0; i < payload_length; i++)
      message[MESSAGE_HEADER_SIZE + i] = i + 1;
  }

  size_t writes_to(uint16_t address) const {
    return std::count_if(this->writes.begin(), this->writes.end(),
                         [address](const Write &write) { return write.address == address; });
  }
};

// 500 kbps at 40 MHz, which is also the NBTCFG reset value
static constexpr uint32_t TEST_NOMINAL_BIT_TIMING = 0x003E0F0F;

class TestMCP251XFD : public MCP251XFD {
 public:
  explicit TestMCP251XFD(RegisterDelegate *delegate, CanMode mode = CanMode::CAN_MODE_NORMAL,
                         CanClock clock = CanClock::CAN_CLOCK_40MHZ)
      : MCP251XFD(clock, mode, TEST_NOMINAL_BIT_TIMING, 0, 0) {
    this->delegate_ = delegate;
    delegate->data_rate = this->data_rate_;
  }

  // There is no SPI bus component, so the fake delegate stays registered
  void spi_setup() override {
    this->spi_setups++;
    static_cast<RegisterDelegate *>(this->delegate_)->data_rate = this->data_rate_;
  }
  void spi_teardown() override { this->spi_teardowns++; }

  canbus::CanEventFlags poll(uint32_t now) {
    LoopBlockingGuard{nullptr, nullptr, now};
    return this->get_events();
  }

  void log_events(canbus::CanEventFlags events, uint32_t now) { this->log_events_(events, now); }
  bool mode_fault_active() const { return this->mode_fault_active_; }
  using MCP251XFD::read_message;
  using MCP251XFD::send_message;

  unsigned spi_setups{0};
  unsigned spi_teardowns{0};
};

}  // namespace esphome::mcp251xfd::testing
