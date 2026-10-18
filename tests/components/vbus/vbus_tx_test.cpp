#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

#include "esphome/components/vbus/vbus.h"
#include "esphome/core/application.h"

namespace esphome::vbus::testing {
namespace {

class TransmitTestUART : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;
  std::vector<std::vector<uint8_t>> writes;
  size_t position{0};
  size_t room{16};
  bool connected{true};
  bool fail_read{false};
  void inject(const std::vector<uint8_t> &bytes) { this->rx.insert(this->rx.end(), bytes.begin(), bytes.end()); }
  size_t available() override { return this->rx.size() - this->position; }
  size_t available_for_write() override { return this->room; }
  bool is_connected() override { return this->connected; }
  bool read_array(uint8_t *data, size_t size) override {
    if (this->fail_read || size > this->available())
      return false;
    std::copy_n(this->rx.data() + this->position, size, data);
    this->position += size;
    return true;
  }
  bool peek_byte(uint8_t *data) override {
    if (!this->available())
      return false;
    *data = this->rx[this->position];
    return true;
  }
  void write_array(const uint8_t *data, size_t size) override { this->writes.emplace_back(data, data + size); }
  uart::UARTFlushResult flush() override {
    ADD_FAILURE() << "TX must not block flushing the UART";
    return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

std::vector<uint8_t> wire_datagram(uint16_t command = 0x0500, uint16_t source = 0x2211, uint16_t destination = 0,
                                   uint16_t value_id = 0, uint32_t value = 0) {
  std::vector<uint8_t> bytes{0xaa,
                             uint8_t(destination),
                             uint8_t(destination >> 8),
                             uint8_t(source),
                             uint8_t(source >> 8),
                             0x20,
                             uint8_t(command),
                             uint8_t(command >> 8),
                             uint8_t(value_id),
                             uint8_t(value_id >> 8),
                             uint8_t(value),
                             uint8_t(value >> 8),
                             uint8_t(value >> 16),
                             uint8_t(value >> 24),
                             0,
                             0};
  for (size_t i = 8; i < 14; ++i) {
    bytes[14] |= (bytes[i] >> 7) << (i - 8);
    bytes[i] &= 0x7f;
  }
  uint32_t sum = 0;
  for (size_t i = 1; i < 15; ++i)
    sum += bytes[i];
  bytes[15] = (0x7f - sum) & 0x7f;
  return bytes;
}

class VBusTransmitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.set_uart_parent(&this->uart_);
    this->bus_.set_transmitter(&this->tx_);
    this->tx_.set_uart_parent(&this->uart_);
    this->tx_.set_destination(0x2211);
    this->tx_.add_on_bus_grant_callback([this](uint32_t grant_id) {
      ++this->grants_;
      this->grant_id_ = grant_id;
    });
    this->cycle_();
    this->advance_(10);
  }
  void cycle_() {
    LoopBlockingGuard guard{nullptr, nullptr, this->now_};
    this->bus_.loop();
    this->tx_.loop();
  }
  void advance_(uint32_t elapsed) {
    while (elapsed > 0) {
      const uint32_t step = std::min(elapsed, uint32_t{10});
      this->now_ += step;
      elapsed -= step;
      this->cycle_();
    }
  }
  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.inject(bytes);
    this->cycle_();
  }
  void acquire_() {
    ASSERT_TRUE(this->bus_.request_bus());
    this->feed_(wire_datagram());
    this->advance_(100);
    ASSERT_TRUE(this->bus_.has_bus());
  }

  TransmitTestUART uart_;
  VBus bus_{};
  VBusTransmitter tx_{&this->bus_};
  uint32_t now_{0};
  size_t grants_{0};
  uint32_t grant_id_{0};
};

TEST_F(VBusTransmitTest, DoesNotTransmitOrClaimUnrequestedGrants) {
  this->feed_(wire_datagram());
  this->advance_(1000);
  EXPECT_EQ(this->grants_, 0u);
  EXPECT_FALSE(this->bus_.has_bus());
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusTransmitTest, WaitsForContentionAndExplicitlyReleasesOnce) {
  ASSERT_TRUE(this->bus_.request_bus());
  EXPECT_FALSE(this->bus_.request_bus());
  this->feed_(wire_datagram());
  this->advance_(99);
  EXPECT_FALSE(this->bus_.has_bus());
  this->advance_(1);
  EXPECT_TRUE(this->bus_.has_bus());
  EXPECT_EQ(this->grants_, 1u);
  EXPECT_TRUE(this->uart_.writes.empty());
  this->bus_.release_bus(this->grant_id_);
  EXPECT_FALSE(this->bus_.has_bus());
  this->cycle_();
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_EQ(this->uart_.writes[0], wire_datagram(0x0600, 0x0020, 0x2211));
  this->advance_(2000);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
}

TEST_F(VBusTransmitTest, EncodesFullPayloadAndDoesNotRetry) {
  this->acquire_();
  EXPECT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0900, 0x9234, 0x89abcdef));
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_EQ(this->uart_.writes[0], wire_datagram(0x0900, 0x0020, 0x2211, 0x9234, 0x89abcdef));
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->advance_(500);
  ASSERT_EQ(this->uart_.writes.size(), 2u);
  EXPECT_EQ(this->uart_.writes[1], wire_datagram(0x0600, 0x0020, 0x2211));
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
}

TEST_F(VBusTransmitTest, SupportsMatchingEchoAndReplyForFurtherRequests) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->feed_(this->uart_.writes[0]);
  EXPECT_TRUE(this->bus_.has_bus());
  this->advance_(20);
  this->feed_(wire_datagram(0x0100, 0x2211, 0x0020, 2, 1));
  EXPECT_TRUE(this->bus_.has_bus());
  EXPECT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 3, 0));
}

TEST_F(VBusTransmitTest, RejectsReservedAndInvalidCommandsAtRuntime) {
  this->acquire_();
  for (uint16_t command : {0x0500, 0x0600, 0x0080, 0x8000, 0xffff})
    EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, command, 0, 0));
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusTransmitTest, BackpressureAndDisconnectedUartCannotQueueALateWrite) {
  this->acquire_();
  this->uart_.room = 15;
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0200, 2, 1));
  this->uart_.room = 16;
  this->uart_.connected = false;
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0200, 2, 1));
  this->advance_(500);
  this->uart_.connected = true;
  this->advance_(1000);
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusTransmitTest, TimesOutWaitingAndCancellationDoesNotReleaseUnownedBus) {
  ASSERT_TRUE(this->bus_.request_bus());
  this->advance_(10000);
  this->feed_(wire_datagram());
  this->advance_(100);
  EXPECT_EQ(this->grants_, 0u);
  EXPECT_TRUE(this->bus_.request_bus());
  this->feed_(wire_datagram());
  this->bus_.release_bus(this->grant_id_);
  this->advance_(1000);
  EXPECT_EQ(this->grants_, 0u);
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusTransmitTest, CancelsOnForeignTrafficDuringContention) {
  ASSERT_TRUE(this->bus_.request_bus());
  this->feed_(wire_datagram());
  this->advance_(50);
  this->feed_(wire_datagram(0x0300, 0x0030, 0x2211, 2, 0));
  this->advance_(1000);
  EXPECT_EQ(this->grants_, 0u);
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusTransmitTest, RejectsGrantAfterLoopStallOrBacklog) {
  ASSERT_TRUE(this->bus_.request_bus());
  this->now_ += 100;
  this->feed_(wire_datagram());
  this->advance_(1000);
  EXPECT_EQ(this->grants_, 0u);
  std::vector<uint8_t> backlog(40, 0);
  const auto grant = wire_datagram();
  backlog.insert(backlog.end(), grant.begin(), grant.end());
  this->feed_(backlog);
  this->advance_(1000);
  EXPECT_EQ(this->grants_, 0u);
  this->feed_(grant);
  this->advance_(100);
  EXPECT_EQ(this->grants_, 1u);
}

TEST_F(VBusTransmitTest, DoesNotTransmitAfterControllerResumesOrAnotherPeerAppears) {
  this->acquire_();
  this->feed_(wire_datagram(0x0100, 0x2211, 0x0010));
  EXPECT_FALSE(this->bus_.has_bus());
  this->bus_.release_bus(this->grant_id_);
  this->advance_(1000);
  EXPECT_TRUE(this->uart_.writes.empty());
  this->acquire_();
  this->feed_(wire_datagram(0x0300, 0x0030, 0x2211, 2, 0));
  EXPECT_FALSE(this->bus_.has_bus());
  this->advance_(1000);
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusTransmitTest, DuplicateLocalAddressCannotClaimBus) {
  ASSERT_TRUE(this->bus_.request_bus());
  this->feed_(wire_datagram(0x0300, 0x0020, 0x2211, 2, 0));
  this->feed_(wire_datagram());
  this->advance_(1000);
  EXPECT_EQ(this->grants_, 0u);
}

TEST_F(VBusTransmitTest, DelayedCallbackAndShutdownCannotTransmit) {
  this->acquire_();
  this->now_ += 1000;
  {
    LoopBlockingGuard guard{nullptr, nullptr, this->now_};
    EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0200, 2, 1));
  }
  this->cycle_();
  EXPECT_TRUE(this->uart_.writes.empty());
  this->advance_(10);
  this->acquire_();
  this->tx_.on_shutdown();
  EXPECT_FALSE(this->bus_.request_bus());
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0200, 2, 1));
  this->advance_(1000);
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusTransmitTest, ReleaseWaitsForWireTime) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->bus_.release_bus(this->grant_id_);
  this->advance_(19);
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  this->advance_(1);
  ASSERT_EQ(this->uart_.writes.size(), 2u);
}

TEST_F(VBusTransmitTest, CanUseSeparateTransmitUartAndLeavesOtherHubRxOnly) {
  TransmitTestUART tx_uart;
  this->tx_.set_uart_parent(&tx_uart);
  VBus other{};
  TransmitTestUART other_uart;
  other.set_uart_parent(&other_uart);
  EXPECT_FALSE(other.request_bus());
  EXPECT_FALSE(other.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->acquire_();
  EXPECT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  EXPECT_TRUE(this->uart_.writes.empty());
  EXPECT_TRUE(other_uart.writes.empty());
  ASSERT_EQ(tx_uart.writes.size(), 1u);
}

TEST_F(VBusTransmitTest, AGrantAlreadyStartedBeforeRequestCannotBeClaimed) {
  const auto bytes = wire_datagram();
  this->feed_({bytes.begin(), bytes.begin() + 5});
  ASSERT_TRUE(this->bus_.request_bus());
  this->feed_({bytes.begin() + 5, bytes.end()});
  this->advance_(150);
  EXPECT_EQ(this->grants_, 0u);
  this->feed_(bytes);
  this->advance_(100);
  EXPECT_EQ(this->grants_, 1u);
}

TEST_F(VBusTransmitTest, ReleaseIsSuppressedWhenAResponseIsIncomplete) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->advance_(20);
  const auto reply = wire_datagram(0x0100, 0x2211, 0x0020, 2, 1);
  this->feed_({reply.begin(), reply.begin() + 10});
  this->bus_.release_bus(this->grant_id_);
  this->advance_(500);
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_FALSE(this->bus_.has_bus());
}

TEST_F(VBusTransmitTest, LeaseAndWaitTimeoutWorkAcrossClockWraparound) {
  this->now_ = 0xffffff00;
  this->cycle_();
  this->advance_(10);
  this->acquire_();
  this->advance_(500);
  EXPECT_FALSE(this->bus_.has_bus());
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_EQ(this->uart_.writes[0], wire_datagram(0x0600, 0x0020, 0x2211));
  this->now_ = 0xfffffff0;
  this->cycle_();
  this->advance_(10);
  EXPECT_TRUE(this->bus_.request_bus());
  this->advance_(10000);
  EXPECT_TRUE(this->bus_.request_bus());
}

TEST_F(VBusTransmitTest, AnUnexpectedLocalEchoRevokesOwnership) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->feed_(wire_datagram(0x0300, 0x0020, 0x2211, 3, 0));
  this->advance_(1000);
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_FALSE(this->bus_.has_bus());
}

TEST_F(VBusTransmitTest, EveryPayloadHighBitCombinationIsPreserved) {
  for (uint32_t septet = 0; septet < 64; ++septet) {
    this->acquire_();
    const auto byte = [septet](size_t i) -> uint32_t { return (septet & (1u << i)) ? 0xff : 0x7f; };
    const uint16_t id = byte(0) | (byte(1) << 8);
    const uint32_t value = byte(2) | (byte(3) << 8) | (byte(4) << 16) | (byte(5) << 24);
    ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, id, value));
    const auto &bytes = this->uart_.writes.back();
    ASSERT_EQ(bytes.size(), 16u);
    EXPECT_EQ(bytes[14], septet);
    for (size_t i = 8; i < 14; ++i)
      EXPECT_EQ(bytes[i], 0x7f);
    unsigned sum = 0;
    for (size_t i = 1; i < 16; ++i)
      sum += bytes[i];
    EXPECT_EQ(sum & 0x7f, 0x7fu);
    this->bus_.release_bus(this->grant_id_);
    this->advance_(30);
  }
}

TEST_F(VBusTransmitTest, AReadFailureRevokesOwnershipWithoutTransmitting) {
  this->acquire_();
  this->uart_.inject(wire_datagram(0x0100, 0x2211, 0x0020, 2, 1));
  this->uart_.fail_read = true;
  this->cycle_();
  EXPECT_FALSE(this->bus_.has_bus());
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0200, 2, 1));
  this->uart_.fail_read = false;
  this->cycle_();
  this->advance_(1000);
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusTransmitTest, OldCallbacksCannotUseOrReleaseANewerLease) {
  this->acquire_();
  const uint32_t old_grant = this->grant_id_;
  this->advance_(500);
  this->acquire_();
  ASSERT_NE(this->grant_id_, old_grant);
  const size_t writes = this->uart_.writes.size();
  EXPECT_FALSE(this->bus_.send_datagram(old_grant, 0x0200, 2, 1));
  this->bus_.release_bus(old_grant);
  EXPECT_TRUE(this->bus_.has_bus());
  EXPECT_EQ(this->uart_.writes.size(), writes);
  EXPECT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
}

TEST_F(VBusTransmitTest, AGrantQueuedBeforeTheRequestCannotBeClaimed) {
  this->uart_.inject(wire_datagram());
  EXPECT_TRUE(this->bus_.request_bus());
  this->cycle_();
  this->advance_(150);
  EXPECT_EQ(this->grants_, 0u);
  this->feed_(wire_datagram());
  this->advance_(100);
  EXPECT_EQ(this->grants_, 1u);
}

class VBusTxGrantTest : public VBusTransmitTest, public ::testing::WithParamInterface<int> {};

TEST_P(VBusTxGrantTest, RejectsUnauthenticatedGrantAndRecovers) {
  auto bytes = wire_datagram();
  switch (GetParam()) {
    case 0:
      bytes = wire_datagram(0x0500, 0x3311);
      break;
    case 1:
      bytes = wire_datagram(0x0500, 0x2211, 0x0030);
      break;
    case 2:
      bytes = wire_datagram(0x0500, 0x2211, 0, 1);
      break;
    case 3:
      bytes = wire_datagram(0x0500, 0x2211, 0, 0, 1);
      break;
    case 4:
      bytes[15] ^= 1;
      break;
    case 5:
      bytes[14] = 0x40;
      bytes[15] = (bytes[15] - 0x40) & 0x7f;
      break;
    case 6:
      bytes[5] = 0x21;
      bytes[15] = (bytes[15] - 1) & 0x7f;
      break;
  }
  ASSERT_TRUE(this->bus_.request_bus());
  this->feed_(bytes);
  this->advance_(150);
  EXPECT_EQ(this->grants_, 0u);
  this->feed_(wire_datagram());
  this->advance_(100);
  EXPECT_EQ(this->grants_, 1u);
}

INSTANTIATE_TEST_SUITE_P(EveryField, VBusTxGrantTest, ::testing::Range(0, 7));

class VBusTxSplitTest : public VBusTransmitTest, public ::testing::WithParamInterface<size_t> {};

TEST_P(VBusTxSplitTest, FreshSplitGrantWorksButAStalledSplitDoesNot) {
  const auto bytes = wire_datagram();
  const auto split = bytes.begin() + GetParam();
  ASSERT_TRUE(this->bus_.request_bus());
  this->feed_({bytes.begin(), split});
  this->advance_(60);
  this->feed_({split, bytes.end()});
  this->advance_(150);
  EXPECT_EQ(this->grants_, 0u);
  this->feed_({bytes.begin(), split});
  this->advance_(10);
  this->feed_({split, bytes.end()});
  this->advance_(100);
  EXPECT_EQ(this->grants_, 1u);
}

INSTANTIATE_TEST_SUITE_P(EverySplit, VBusTxSplitTest, ::testing::Range(size_t{1}, size_t{16}));

}  // namespace
}  // namespace esphome::vbus::testing
