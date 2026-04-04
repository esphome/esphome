#include "../common.h"

namespace esphome::mitsubishi_cn105::testing {

struct TestContext {
  MockUARTComponent uart;
  uart::UARTDevice device{&uart};
  TestableMitsubishiCN105 sut{device};

  TestContext() { this->sut.set_current_time(0); }
};

TEST(MitsubishiCN105Tests, InitSendsConnectPacket) {
  auto ctx = TestContext{};

  ctx.sut.set_current_time(123);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::NOT_CONNECTED);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_FALSE(ctx.sut.write_timeout_start_ms_.has_value());

  ctx.sut.initialize();

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8));
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{123});
}

TEST(MitsubishiCN105Tests, SuccessfullyConnects) {
  auto ctx = TestContext{};

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.sut.write_timeout_start_ms_.has_value());

  // Connect response
  ctx.uart.push_rx({0xFC, 0x7A, 0x01, 0x30, 0x00, 0x55});

  ctx.sut.update();

  // All bytes from UART should be consumed and state = CONNECTED
  EXPECT_TRUE(ctx.uart.rx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTED);
  EXPECT_FALSE(ctx.sut.write_timeout_start_ms_.has_value());

  // Nothing should be send to UART
  EXPECT_TRUE(ctx.uart.tx.empty());
}

TEST(MitsubishiCN105Tests, NoResponseTriggersReconnect) {
  auto ctx = TestContext{};

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  // No response (no RX data), no retry yet
  ctx.sut.update();
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{0});

  // Still no response after 1999ms, no retry yet
  ctx.sut.set_current_time(1999);
  ctx.sut.update();
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{0});

  // Stop waiting after 2s and retry connect
  ctx.sut.set_current_time(2000);
  ctx.sut.update();
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8));
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{2000});
}

TEST(MitsubishiCN105Tests, RxWatchdogLimitsProcessingPerUpdate) {
  auto ctx = TestContext{};

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  // RX noise/unexpected traffic
  ctx.uart.push_rx({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
                    0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
                    0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                    0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46});

  // Make sure we have enough bytes in buffer.
  ASSERT_GT(ctx.uart.rx.size(), 64);

  // No valid response, no state change expected
  ctx.sut.update();
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());

  // Watchdog interrupts reading (max. 64 bytes at once) so we do not spend the whole loop draining UART
  EXPECT_FALSE(ctx.uart.rx.empty());

  // Next update will read remaining bytes, no state change expected
  ctx.sut.update();
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_TRUE(ctx.uart.rx.empty());
}

TEST(MitsubishiCN105Tests, ParserHandlesMixedRxStream) {
  auto ctx = TestContext{};

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  // Mixed RX stream with partial, malformed, and oversized frames to test parser robustness
  ctx.uart.push_rx({// ─────────────────────────────
                    // Noise (no 0xFC) -> should be ignored via preamble reset
                    // ────────────────────────────
                    0x01, 0x02, 0x03, 0x04, 0x05,

                    // ─────────────────────────────
                    // Partial frame (declares payload len=5, but we cut it short)
                    // Later bytes will eventually force checksum mismatch and reset
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0x05, 0xAA, 0xBB,

                    // ─────────────────────────────
                    // Invalid header (header byte 3 should be 0x01, header byte 4 should be 0x30)
                    // Should reset quickly on header mismatch
                    // ─────────────────────────────
                    0xFC, 0x62, 0xFF, 0xFF, 0x02, 0x01, 0x02, 0x00,

                    // ─────────────────────────────
                    // Oversized length field (rejected by payload-too-large check at HEADER_LEN)
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0xFE, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
                    0x1B, 0x1C, 0x1D, 0x1E, 0x1F,

                    // ─────────────────────────────
                    // Valid unknown-type frame (type=0x62), should be parsed successfully then ignored
                    // Frame: FC 62 01 30 02 AA BB 30
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0x02, 0xAA, 0xBB, 0x30,

                    // ─────────────────────────────
                    // Invalid checksum (should be rejected at checksum check)
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0x02, 0x10, 0x20, 0xFF,

                    // ─────────────────────────────
                    // Back-to-back VALID frames (unknown type=0x62) to stress boundary handling.
                    // Frame A: FC 62 01 30 01 02 6C
                    // Frame B: FC 62 01 30 01 03 6B
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0x01, 0x02, 0x6C, 0xFC, 0x62, 0x01, 0x30, 0x01, 0x03, 0x6B,

                    // ─────────────────────────────
                    // Trailing noise
                    // ─────────────────────────────
                    0x55, 0x66, 0x77, 0x88});

  // Drain RX - no valid response, no state change expected
  int iterations = 0;
  while (!ctx.uart.rx.empty() && iterations++ < 10) {
    ctx.sut.update();
    EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
    EXPECT_TRUE(ctx.uart.tx.empty());
  }

  EXPECT_TRUE(ctx.uart.rx.empty());
}

}  // namespace esphome::mitsubishi_cn105::testing
