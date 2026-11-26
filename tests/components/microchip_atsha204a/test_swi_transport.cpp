#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "esphome/components/microchip_atsha204a/transport.h"
#include "esphome/components/uart/uart_virtual.h"

namespace esphome::microchip_atsha204a::testing {

class SwiTestBus : public uart::VirtualUARTComponent<(1 + MAX_RESPONSE) * 8> {
 public:
  void write_array(const uint8_t *data, size_t len) override { this->transmitted.assign(data, data + len); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  std::vector<uint8_t> transmitted;
};

class SwiTestTransport : public SWITransport {
 public:
  bool setup() override { return true; }
  void dump_config() override {}
  bool wake() override { return true; }

 protected:
  bool send(const uint8_t *bytes, size_t size) override {
    this->write_array(bytes, size);
    return true;
  }
};

static void append_tokens(std::vector<uint8_t> &tokens, uint8_t byte) {
  for (unsigned int bit = 0; bit < 8; bit++) {
    // Both observed representations of the one token decode to the same bit.
    tokens.push_back((byte & (1U << bit)) == 0 ? 0x7D : (bit % 2 != 0 ? 0x7E : 0x7F));
  }
}

TEST(ATSHA204ASWI, ReceivesMaximumFrameAcrossFragmentBoundaries) {
  SwiTestBus bus;
  SwiTestTransport transport;
  transport.set_uart_parent(&bus);
  ASSERT_TRUE(transport.request_response());
  EXPECT_EQ(bus.transmitted, (std::vector<uint8_t>{0x88}));

  std::array<uint8_t, MAX_RESPONSE> expected{};
  expected[0] = expected.size();
  for (size_t i = 1; i < expected.size(); i++)
    expected[i] = static_cast<uint8_t>((i * 37U) % 256U);
  std::vector<uint8_t> tokens;
  append_tokens(tokens, 0x88);
  for (uint8_t byte : expected)
    append_tokens(tokens, byte);

  std::array<uint8_t, MAX_RESPONSE> received{};
  size_t offset = 0;
  for (size_t count : {7U, 8U, 131U}) {
    ASSERT_TRUE(bus.inject_rx(tokens.data() + offset, count));
    EXPECT_EQ(transport.receive(received.data()), Receive::RECEIVE_PENDING);
    offset += count;
  }
  ASSERT_TRUE(bus.inject_rx(tokens.data() + offset, tokens.size() - offset));
  ASSERT_EQ(transport.receive(received.data()), Receive::RECEIVE_COMPLETE);
  EXPECT_EQ(received, expected);
}

TEST(ATSHA204ASWI, RejectsBadEchoAndOutOfRangePacketLengths) {
  SwiTestBus bus;
  SwiTestTransport transport;
  transport.set_uart_parent(&bus);
  std::array<uint8_t, MAX_RESPONSE> received{};
  for (const auto &header : {std::array<uint8_t, 2>{0x89, 4}, {0x88, 3}, {0x88, MAX_RESPONSE + 1}}) {
    ASSERT_TRUE(transport.request_response());
    std::vector<uint8_t> tokens;
    append_tokens(tokens, header[0]);
    append_tokens(tokens, header[1]);
    ASSERT_TRUE(bus.inject_rx(tokens.data(), tokens.size()));
    EXPECT_EQ(transport.receive(received.data()), Receive::RECEIVE_INVALID_LENGTH);
  }
}

}  // namespace esphome::microchip_atsha204a::testing
