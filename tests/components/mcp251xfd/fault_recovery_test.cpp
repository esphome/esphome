#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/mcp251xfd/mcp251xfd.h"
#include "esphome/core/application.h"

namespace esphome::mcp251xfd::testing {

namespace {

// Compute the controller's SPI CRC independently of the driver.
uint16_t spi_crc(const uint8_t *data, size_t length, uint16_t crc = 0xFFFF) {
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

class RegisterDelegate : public spi::SPIDelegate {
 public:
  std::array<uint8_t, 0x1000> registers{};
  std::array<unsigned, 0x1000> reads{};
  std::vector<Write> writes;
  uint16_t corrupt_address{0xFFFF};
  unsigned corrupt_reads{0};

  void begin_transaction() override {}
  void end_transaction() override {}
  uint8_t transfer(uint8_t data) override { return 0; }

  void transfer(uint8_t *data, size_t length) override {
    ASSERT_GE(length, 5u);
    ASSERT_EQ(data[0] & 0xF0, INSTRUCTION_READ_CRC);
    const uint16_t address = ((data[0] & 0x0F) << 8) | data[1];
    const size_t data_length = length - 5;
    ASSERT_LE(address + data_length, this->registers.size());
    EXPECT_EQ(data[2], address >= RAM_START && address < RAM_START + RAM_SIZE ? data_length / 4 : data_length);
    this->reads[address]++;

    uint16_t crc = spi_crc(data, 3);
    std::copy_n(this->registers.data() + address, data_length, data + 3);
    if (address == REG_INT && data_length >= 2) {
      if (this->registers[REG_RX_FIFO_STA] & FIFO_RX_OVERFLOW)
        data[4] |= INT_RX_OVERFLOW;
      if (this->registers[REG_CRC + 2] & (CRC_CRC_ERROR | CRC_FORMAT_ERROR))
        data[4] |= INT_SPI_CRC_ERROR;
    }
    crc = spi_crc(data + 3, data_length, crc);
    if (address == this->corrupt_address && this->corrupt_reads > 0) {
      this->corrupt_reads--;
      crc ^= 1;
    }
    data[3 + data_length] = crc >> 8;
    data[4 + data_length] = crc;
  }

  void write_array(const uint8_t *data, size_t length) override {
    ASSERT_GE(length, 2u);
    ASSERT_EQ(data[0] & 0xF0, INSTRUCTION_WRITE);
    const uint16_t address = ((data[0] & 0x0F) << 8) | data[1];
    ASSERT_LE(address + length - 2, this->registers.size());
    this->writes.push_back({address, {data + 2, data + length}});
    for (size_t i = 2; i < length; i++) {
      const uint16_t reg = address + i - 2;
      if (reg == REG_INT || reg == REG_INT + 1 || reg == REG_RX_FIFO_STA || reg == REG_CRC + 2) {
        this->registers[reg] &= data[i];
      } else {
        this->registers[reg] = data[i];
      }
    }
  }

  void corrupt_next_reads(uint16_t address, unsigned count = 3) {
    this->corrupt_address = address;
    this->corrupt_reads = count;
  }

  void set_mode(CanMode mode) { this->registers[REG_CON + 2] = static_cast<uint8_t>(mode) << CON_MODE_SHIFT; }
  void set_error_flags(uint8_t flags) { this->registers[REG_TREC + 2] = flags; }

  size_t writes_to(uint16_t address) const {
    return std::count_if(this->writes.begin(), this->writes.end(),
                         [address](const Write &write) { return write.address == address; });
  }
};

class TestMCP251XFD : public MCP251XFD {
 public:
  explicit TestMCP251XFD(RegisterDelegate *delegate)
      : MCP251XFD(CanClock::CAN_CLOCK_40MHZ, CanMode::CAN_MODE_NORMAL, 0, 0, 0) {
    this->delegate_ = delegate;
  }

  canbus::CanEventFlags poll(uint32_t now) {
    LoopBlockingGuard{nullptr, nullptr, now};
    return this->get_events();
  }

  void log_events(canbus::CanEventFlags events, uint32_t now) { this->log_events_(events, now); }
  bool mode_fault_active() const { return this->mode_fault_active_; }
  using MCP251XFD::read_register_;
};

}  // namespace

TEST(MCP251XFDFaults, RetriesCrcFailuresAndPreservesLastStatusWhenReadsFail) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_TREC] = 4;
  spi.registers[REG_TREC + 1] = 9;
  spi.corrupt_next_reads(REG_TREC, 2);

  auto status = controller.get_status();
  EXPECT_EQ(status.rx_error_counter, 4);
  EXPECT_EQ(status.tx_error_counter, 9);
  EXPECT_EQ(spi.reads[REG_TREC], 3u);
  EXPECT_FALSE(controller.status_has_warning());

  spi.registers[REG_TREC] = 7;
  spi.corrupt_next_reads(REG_TREC);
  status = controller.get_status();
  EXPECT_EQ(status.rx_error_counter, 4);
  EXPECT_EQ(status.tx_error_counter, 9);
  EXPECT_EQ(spi.reads[REG_TREC], 6u);
  EXPECT_TRUE(controller.status_has_warning());

  status = controller.get_status();
  EXPECT_EQ(status.rx_error_counter, 7);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDFaults, ClearsFifoAndSpiFaultsWithoutClearingOtherBits) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_RX_FIFO_STA] = FIFO_RX_OVERFLOW | FIFO_NOT_FULL_NOT_EMPTY;
  spi.registers[REG_CRC + 2] = CRC_CRC_ERROR | CRC_FORMAT_ERROR | 0x80;

  EXPECT_EQ(controller.poll(100), canbus::CAN_EVENT_RX_QUEUE_FULL);
  EXPECT_EQ(spi.registers[REG_RX_FIFO_STA], FIFO_NOT_FULL_NOT_EMPTY);
  EXPECT_EQ(spi.registers[REG_CRC + 2], 0x80);
  ASSERT_EQ(spi.writes.size(), 2u);
  EXPECT_EQ(spi.writes[0].address, REG_RX_FIFO_STA);
  EXPECT_EQ(spi.writes[0].data, (std::vector<uint8_t>{static_cast<uint8_t>(~FIFO_RX_OVERFLOW)}));
  EXPECT_EQ(spi.writes[1].address, REG_CRC + 2);
  EXPECT_EQ(spi.writes[1].data, (std::vector<uint8_t>{static_cast<uint8_t>(~(CRC_CRC_ERROR | CRC_FORMAT_ERROR))}));
  EXPECT_EQ(controller.get_status().rx_overrun_count, 1u);
  EXPECT_EQ(controller.poll(200), 0);
  EXPECT_EQ(controller.get_status().rx_overrun_count, 1u);
}

TEST(MCP251XFDFaults, FailedFifoStatusReadLeavesOverflowPendingForRetry) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_RX_FIFO_STA] = FIFO_RX_OVERFLOW;
  spi.corrupt_next_reads(REG_RX_FIFO_STA);

  EXPECT_EQ(controller.poll(100), canbus::CAN_EVENT_RX_QUEUE_FULL);
  EXPECT_EQ(spi.registers[REG_RX_FIFO_STA], FIFO_RX_OVERFLOW);
  EXPECT_EQ(spi.writes_to(REG_RX_FIFO_STA), 0u);
  EXPECT_TRUE(controller.status_has_warning());

  EXPECT_EQ(controller.poll(200), canbus::CAN_EVENT_RX_QUEUE_FULL);
  EXPECT_EQ(spi.registers[REG_RX_FIFO_STA], 0);
  EXPECT_EQ(spi.writes_to(REG_RX_FIFO_STA), 1u);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDFaults, FailedCrcStatusReadLeavesSpiFaultPendingForRetry) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_CRC + 2] = CRC_CRC_ERROR;
  spi.corrupt_next_reads(REG_CRC + 2);

  EXPECT_EQ(controller.poll(100), 0);
  EXPECT_EQ(spi.registers[REG_CRC + 2], CRC_CRC_ERROR);
  EXPECT_EQ(spi.writes_to(REG_CRC + 2), 0u);
  EXPECT_TRUE(controller.status_has_warning());

  EXPECT_EQ(controller.poll(200), 0);
  EXPECT_EQ(spi.registers[REG_CRC + 2], 0);
  EXPECT_EQ(spi.writes_to(REG_CRC + 2), 1u);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDFaults, ReportsErrorStateTransitionsAndPreservesUnrelatedInterrupts) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.registers[REG_TREC] = 3;
  spi.registers[REG_TREC + 1] = 128;
  spi.set_error_flags(ERROR_TX_PASSIVE);
  constexpr uint8_t unrelated_interrupt = 0x40;
  spi.registers[REG_INT + 1] = INT_CAN_ERROR | INT_INVALID_MESSAGE | unrelated_interrupt;

  EXPECT_EQ(controller.poll(100), canbus::CAN_EVENT_PASSIVE);
  EXPECT_EQ(spi.registers[REG_INT + 1], unrelated_interrupt);
  const auto status = controller.get_status();
  EXPECT_EQ(status.rx_error_counter, 3);
  EXPECT_EQ(status.tx_error_counter, 128);
  EXPECT_EQ(status.bus_error_count, 1u);

  spi.set_error_flags(0);
  EXPECT_EQ(controller.poll(200), canbus::CAN_EVENT_ACTIVE);
  EXPECT_EQ(controller.get_status().bus_error_count, 1u);
}

TEST(MCP251XFDRecovery, RetriesModeRequestAfterIntervalAndStopsWhenRecovered) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTEN_ONLY);
  spi.registers[REG_CON + 3] = 0xA6;
  spi.registers[REG_INT] = INT_MODE_CHANGE;

  EXPECT_EQ(controller.poll(100), 0);
  ASSERT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.registers[REG_CON + 3], 0xA0);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, 0);
  EXPECT_TRUE(controller.mode_fault_active());

  controller.poll(5099);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  controller.poll(5100);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 2u);

  spi.set_mode(CanMode::CAN_MODE_NORMAL);
  controller.poll(5200);
  EXPECT_FALSE(controller.mode_fault_active());
  const auto mode_reads = spi.reads[REG_CON + 2];
  controller.poll(10300);
  EXPECT_EQ(spi.reads[REG_CON + 2], mode_reads);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 2u);
}

TEST(MCP251XFDRecovery, DefersModeRequestWhileBusOff) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTEN_ONLY);
  spi.set_error_flags(ERROR_BUS_OFF);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  spi.registers[REG_INT + 1] = INT_CAN_ERROR;

  EXPECT_EQ(controller.poll(100), canbus::CAN_EVENT_BUS_OFF);
  EXPECT_TRUE(controller.mode_fault_active());
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 0u);
  EXPECT_EQ(controller.poll(200), canbus::CAN_EVENT_BUS_OFF);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 0u);

  spi.set_error_flags(0);
  EXPECT_EQ(controller.poll(300), 0);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
}

TEST(MCP251XFDRecovery, FailedModeReadLeavesFaultFlagsPending) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTEN_ONLY);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  spi.registers[REG_INT + 1] = INT_SYSTEM_ERROR;
  spi.corrupt_next_reads(REG_CON + 2);

  controller.poll(100);
  EXPECT_EQ(spi.reads[REG_CON + 2], 3u);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, INT_MODE_CHANGE);
  EXPECT_EQ(spi.registers[REG_INT + 1] & INT_SYSTEM_ERROR, INT_SYSTEM_ERROR);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 0u);
  EXPECT_TRUE(controller.status_has_warning());

  controller.poll(200);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, 0);
  EXPECT_EQ(spi.registers[REG_INT + 1] & INT_SYSTEM_ERROR, 0);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDRecovery, FailedErrorStateReadDefersModeRecoveryAndFlagClearing) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTEN_ONLY);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  spi.registers[REG_INT + 1] = INT_SYSTEM_ERROR | INT_CAN_ERROR;
  spi.corrupt_next_reads(REG_TREC);

  controller.poll(100);
  EXPECT_EQ(spi.reads[REG_CON + 2], 0u);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, INT_MODE_CHANGE);
  EXPECT_EQ(spi.registers[REG_INT + 1] & (INT_SYSTEM_ERROR | INT_CAN_ERROR), INT_SYSTEM_ERROR | INT_CAN_ERROR);
  EXPECT_TRUE(controller.status_has_warning());

  controller.poll(200);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 1u);
  EXPECT_EQ(spi.registers[REG_INT] & INT_MODE_CHANGE, 0);
  EXPECT_EQ(spi.registers[REG_INT + 1] & (INT_SYSTEM_ERROR | INT_CAN_ERROR), 0);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDRecovery, ContinuesModeRecoveryWhenInterruptReadFails) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_mode(CanMode::CAN_MODE_LISTEN_ONLY);
  spi.registers[REG_INT] = INT_MODE_CHANGE;
  controller.poll(100);
  ASSERT_TRUE(controller.mode_fault_active());
  ASSERT_EQ(spi.writes_to(REG_CON + 3), 1u);

  spi.corrupt_next_reads(REG_INT);
  controller.poll(5100);
  EXPECT_EQ(spi.writes_to(REG_CON + 3), 2u);
  EXPECT_TRUE(controller.mode_fault_active());
}

TEST(MCP251XFDRecovery, SpiRecoveryDoesNotClearBusOffWarning) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  spi.set_error_flags(ERROR_BUS_OFF);
  spi.registers[REG_INT + 1] = INT_CAN_ERROR;
  auto events = controller.poll(100);
  ASSERT_EQ(events, canbus::CAN_EVENT_BUS_OFF);
  controller.log_events(events, 100);
  ASSERT_TRUE(controller.status_has_warning());

  spi.corrupt_next_reads(REG_INT);
  EXPECT_EQ(controller.poll(200), canbus::CAN_EVENT_BUS_OFF);
  EXPECT_EQ(spi.reads[REG_INT], 4u);
  EXPECT_TRUE(controller.status_has_warning());
  EXPECT_EQ(controller.poll(300), canbus::CAN_EVENT_BUS_OFF);
  EXPECT_TRUE(controller.status_has_warning());

  spi.set_error_flags(0);
  events = controller.poll(1400);
  EXPECT_EQ(events, 0);
  controller.log_events(events, 1400);
  EXPECT_FALSE(controller.status_has_warning());
}

TEST(MCP251XFDFaults, FailedComponentDoesNotAccessSpi) {
  RegisterDelegate spi;
  TestMCP251XFD controller(&spi);
  controller.mark_failed();
  EXPECT_EQ(controller.poll(100), 0);
  EXPECT_EQ(spi.reads[REG_INT], 0u);
  EXPECT_TRUE(spi.writes.empty());
}

}  // namespace esphome::mcp251xfd::testing
