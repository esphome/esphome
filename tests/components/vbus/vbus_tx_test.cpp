#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

#include "esphome/components/vbus/vbus.h"
#include "esphome/components/vbus/sensor/vbus_sensor.h"
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

TEST_F(VBusTransmitTest, EncodesFullPayloadAndDoesNotRetryOrReleaseWithoutAReply) {
  this->acquire_();
  EXPECT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0900, 0x9234, 0x89abcdef));
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_EQ(this->uart_.writes[0], (std::vector<uint8_t>{0xaa, 0x11, 0x22, 0x20, 0x00, 0x20, 0x00, 0x09, 0x34, 0x12,
                                                         0x6f, 0x4d, 0x2b, 0x09, 0x3e, 0x0f}));
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->advance_(500);
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->acquire_();
  EXPECT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
}

TEST_F(VBusTransmitTest, SupportsMatchingEchoAndReplyForFurtherRequests) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->feed_(this->uart_.writes[0]);
  EXPECT_TRUE(this->bus_.has_bus());
  this->feed_(wire_datagram(0x0100, 0x2211, 0x0020, 2, 1));
  this->advance_(20);
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0300, 3, 0));
  this->feed_(wire_datagram(0x0100, 0x2211, 0x0020, 3, 1));
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0300, 3, 0));
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
  ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0100, 2, 0, false));
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
    ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0100, id, value, false));
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

TEST_F(VBusTransmitTest, AGrantFollowedByQueuedTrafficCannotBeClaimed) {
  EXPECT_TRUE(this->bus_.request_bus());
  auto bytes = wire_datagram();
  bytes.push_back(0);
  this->feed_(bytes);
  this->advance_(150);
  EXPECT_EQ(this->grants_, 0u);
  EXPECT_TRUE(this->uart_.writes.empty());
  this->feed_(wire_datagram());
  this->advance_(100);
  EXPECT_EQ(this->grants_, 1u);
}

TEST_F(VBusTransmitTest, LastOrdinaryFrameLeavesEnoughWireTimeForRelease) {
  this->acquire_();
  this->advance_(359);
  ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0100, 2, 0, false));
  this->advance_(1);
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0200, 2, 1));
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  this->advance_(19);
  ASSERT_EQ(this->uart_.writes.size(), 2u);
  EXPECT_EQ(this->uart_.writes[1], wire_datagram(0x0600, 0x0020, 0x2211));
}

TEST_F(VBusTransmitTest, ReleaseWaitsForACompleteControllerReply) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->bus_.release_bus(this->grant_id_);
  this->advance_(40);
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  this->feed_(wire_datagram(0x0100, 0x2211, 0x0020, 3, 1));
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  const auto reply = wire_datagram(0x0100, 0x2211, 0x0020, 2, 1);
  this->feed_({reply.begin(), reply.begin() + 10});
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  this->feed_({reply.begin() + 10, reply.end()});
  ASSERT_EQ(this->uart_.writes.size(), 2u);
  EXPECT_EQ(this->uart_.writes[1], wire_datagram(0x0600, 0x0020, 0x2211));
}

TEST_F(VBusTransmitTest, StandardRequestsCannotDisableReplyWaitingAtRuntime) {
  this->acquire_();
  for (uint16_t command : {0x0200, 0x0300, 0x0400})
    EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, command, 2, 0, false));
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusTransmitTest, AReplyTooNearTheDeadlineCannotStartALateRelease) {
  this->acquire_();
  this->advance_(359);
  ASSERT_TRUE(this->bus_.send_datagram(this->grant_id_, 0x0300, 2, 0));
  this->advance_(35);
  this->feed_(wire_datagram(0x0100, 0x2211, 0x0020, 2, 1));
  this->advance_(100);
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_FALSE(this->bus_.has_bus());
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

#ifdef USE_VBUS_TRANSACTIONS
struct TransactionResult {
  uint32_t grant_id;
  uint16_t command;
  uint16_t value_id;
  uint32_t value;
  TransactionStatus status;
};

class VBusTransactionTest : public VBusTransmitTest {
 protected:
  void SetUp() override {
    VBusTransmitTest::SetUp();
    this->bus_.set_transactions(&this->transactions_);
    this->transactions_.setup();
    this->transactions_.add_on_result_callback(
        [this](uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value, TransactionStatus status) {
          this->results_.push_back({grant_id, command, value_id, value, status});
        });
  }
  void step_(uint32_t elapsed = 0) {
    do {
      const uint32_t step = std::min(elapsed, uint32_t{10});
      this->now_ += step;
      elapsed -= step;
      LoopBlockingGuard guard{nullptr, nullptr, this->now_};
      this->bus_.loop();
      this->tx_.loop();
      this->transactions_.loop();
    } while (elapsed > 0);
  }
  void receive_(const std::vector<uint8_t> &bytes) {
    this->uart_.inject(bytes);
    this->step_();
  }
  void reply_(uint32_t value, uint16_t value_id = 2) {
    this->receive_(wire_datagram(0x0100, 0x2211, 0x0020, value_id, value));
  }
  VBusTransactions transactions_{&this->bus_, &this->tx_};
  std::vector<TransactionResult> results_;
};

TEST_F(VBusTransactionTest, ReadRequiresAValidMatchingReplyAndDoesNotConsumeRawCallbacks) {
  size_t raw = 0;
  this->bus_.add_on_datagram_callback([&raw](uint16_t, uint16_t, uint16_t, uint16_t, uint32_t) { ++raw; });
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->receive_(this->uart_.writes[0]);
  this->reply_(111);
  this->step_(25);
  this->reply_(222, 3);
  this->receive_(wire_datagram(0x0900, 0x2211, 0x0020, 2, 333));
  EXPECT_TRUE(this->results_.empty());
  EXPECT_FALSE(this->bus_.send_datagram(this->grant_id_, 0x0200, 2, 1));
  EXPECT_FALSE(this->bus_.read_value(this->grant_id_, 3));
  EXPECT_FALSE(this->bus_.write_value(this->grant_id_, 3, 1));
  this->reply_(0x89abcdef);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
  EXPECT_EQ(this->results_[0].grant_id, this->grant_id_);
  EXPECT_EQ(this->results_[0].command, 0x0300);
  EXPECT_EQ(this->results_[0].value_id, 2);
  EXPECT_EQ(this->results_[0].value, 0x89abcdef);
  EXPECT_EQ(raw, 6u);
  EXPECT_FALSE(this->transactions_.is_busy());
}

TEST_F(VBusTransactionTest, WriteAcknowledgementAloneDoesNotConfirmTheSetting) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  EXPECT_EQ(this->uart_.writes[0], wire_datagram(0x0200, 0x0020, 0x2211, 2, 123));
  this->step_(25);
  this->reply_(0);
  EXPECT_TRUE(this->results_.empty());
  ASSERT_EQ(this->uart_.writes.size(), 2u);
  EXPECT_EQ(this->uart_.writes[1], wire_datagram(0x0300, 0x0020, 0x2211, 2));
  this->reply_(123);
  EXPECT_TRUE(this->results_.empty());
  this->step_(25);
  this->reply_(123);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].command, 0x0200);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
  EXPECT_EQ(this->results_[0].value, 123u);
}

TEST_F(VBusTransactionTest, ReportsTheActualValueWhenVerificationDoesNotMatch) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123, 0x0400));
  this->step_(25);
  this->reply_(123);
  this->step_(25);
  this->reply_(122);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].command, 0x0400);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_VERIFY_MISMATCH);
  EXPECT_EQ(this->results_[0].value, 122u);
  EXPECT_FALSE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(1000);
  EXPECT_EQ(this->uart_.writes.size(), 3u);  // Write, verification read, release; no retry.
}

TEST_F(VBusTransactionTest, TimeoutRetiresTheLeaseAndIgnoresLateReplies) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(100);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  EXPECT_FALSE(this->bus_.read_value(this->grant_id_, 2));
  this->reply_(123);
  this->step_(500);
  EXPECT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->uart_.writes.size(), 2u);  // A late valid reply permits only release.
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 3));
  this->step_(25);
  this->reply_(123, 2);
  EXPECT_EQ(this->results_.size(), 1u);
  this->reply_(456, 3);
  ASSERT_EQ(this->results_.size(), 2u);
  EXPECT_EQ(this->results_[1].value, 456u);
}

TEST_F(VBusTransactionTest, NeitherWriteNorVerificationReadsAreRetried) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->step_(500);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->step_(25);
  this->reply_(123);
  this->step_(500);
  ASSERT_EQ(this->results_.size(), 2u);
  EXPECT_EQ(this->results_[1].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  EXPECT_EQ(this->uart_.writes.size(), 3u);
}

TEST_F(VBusTransactionTest, VerificationWaitsForUartCapacityWithoutRepeatingTheWrite) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->uart_.room = 15;
  this->step_(25);
  this->reply_(123);
  this->reply_(123);  // Duplicate ACK before the verification read is sent.
  this->step_(40);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_TRUE(this->results_.empty());
  this->uart_.room = 16;
  this->step_();
  ASSERT_EQ(this->uart_.writes.size(), 2u);
  this->step_(25);
  this->reply_(123);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
}

TEST_F(VBusTransactionTest, VerificationCapacityTimeoutNeverRepeatsTheWrite) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->uart_.room = 15;
  this->step_(25);
  this->reply_(123);
  this->step_(100);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  this->uart_.room = 16;
  this->step_(500);
  ASSERT_EQ(this->uart_.writes.size(), 2u);
  EXPECT_EQ(this->uart_.writes[1], wire_datagram(0x0600, 0x0020, 0x2211));
}

TEST_F(VBusTransactionTest, WrongAcknowledgementCommandDoesNotStartVerification) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->step_(25);
  this->receive_(wire_datagram(0x0900, 0x2211, 0x0020, 2, 123));
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_TRUE(this->results_.empty());
  this->step_(75);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  this->reply_(123);
  EXPECT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->uart_.writes.size(), 2u);
}

TEST_F(VBusTransactionTest, EachWriteStageHasItsOwnResponseTimeout) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->step_(99);
  this->reply_(123);
  this->step_(99);
  this->reply_(123);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
}

TEST_F(VBusTransactionTest, LeaseLossTakesPrecedenceOverALongerResponseTimeout) {
  this->transactions_.set_response_timeout(1000);
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(500);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_BUS_LOST);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
}

TEST_F(VBusTransactionTest, WrongDestinationCannotCompleteTheRead) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(25);
  this->receive_(wire_datagram(0x0100, 0x2211, 0x0030, 2, 123));
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_BUS_LOST);
}

TEST_F(VBusTransactionTest, ZeroAndMaximumRawValuesRemainSuccessfulValues) {
  this->acquire_();
  for (uint32_t value : {0u, 0xffffffffu}) {
    ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 0xffff));
    this->step_(25);
    this->reply_(value, 0xffff);
    ASSERT_EQ(this->results_.back().status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
    EXPECT_EQ(this->results_.back().value_id, 0xffff);
    EXPECT_EQ(this->results_.back().value, value);
  }
}

TEST_F(VBusTransactionTest, AReadCanFinishInTheReservedReleaseWindow) {
  this->acquire_();
  this->step_(340);
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(25);
  EXPECT_FALSE(this->bus_.has_bus());
  this->reply_(123);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
}

TEST_F(VBusTransactionTest, ALateWriteAckCannotStartVerificationOutsideTheSendWindow) {
  this->acquire_();
  this->step_(340);
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->step_(25);
  this->reply_(123);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_BUS_LOST);
  ASSERT_EQ(this->uart_.writes.size(), 2u);
  EXPECT_EQ(this->uart_.writes[1], wire_datagram(0x0600, 0x0020, 0x2211));
}

TEST_F(VBusTransactionTest, WrongPeerAbortsWithoutAcceptingItsValue) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(25);
  this->receive_(wire_datagram(0x0100, 0x2212, 0x0020, 2, 123));
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_BUS_LOST);
  this->reply_(123);
  EXPECT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
}

TEST_F(VBusTransactionTest, CallbackCanStartTheNextReadAndOtherCallbacksKeepTheOriginalArguments) {
  this->acquire_();
  this->transactions_.add_on_result_callback(
      [this](uint32_t grant_id, uint16_t, uint16_t value_id, uint32_t, TransactionStatus) {
        if (value_id == 2)
          EXPECT_TRUE(this->bus_.read_value(grant_id, 3));
      });
  std::vector<uint16_t> ids;
  this->transactions_.add_on_result_callback(
      [&ids](uint32_t, uint16_t, uint16_t value_id, uint32_t, TransactionStatus) { ids.push_back(value_id); });
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(25);
  this->reply_(123);
  EXPECT_TRUE(this->transactions_.is_busy());
  this->step_(25);
  this->reply_(456, 3);
  ASSERT_EQ(this->results_.size(), 2u);
  EXPECT_EQ(ids, (std::vector<uint16_t>{2, 3}));
  EXPECT_EQ(this->results_[0].value, 123u);
  EXPECT_EQ(this->results_[1].value, 456u);
}

TEST_F(VBusTransactionTest, RejectedRequestsDoNotAllocateAnOperationOrPublishAResult) {
  EXPECT_FALSE(this->bus_.read_value(0, 2));
  this->acquire_();
  EXPECT_FALSE(this->bus_.read_value(this->grant_id_ + 1, 2));
  for (uint16_t command : {0x0100, 0x0300, 0x0500, 0x0600, 0x8000})
    EXPECT_FALSE(this->bus_.write_value(this->grant_id_, 2, 123, command));
  this->uart_.room = 15;
  EXPECT_FALSE(this->bus_.read_value(this->grant_id_, 2));
  EXPECT_TRUE(this->results_.empty());
  EXPECT_TRUE(this->uart_.writes.empty());
  EXPECT_FALSE(this->transactions_.is_busy());
}

TEST_F(VBusTransactionTest, ShutdownCannotRearmCallbacksOrSendAnotherFrame) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->transactions_.on_shutdown();
  this->tx_.on_shutdown();
  this->step_(25);
  this->reply_(123);
  this->step_(1000);
  EXPECT_TRUE(this->results_.empty());
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_FALSE(this->bus_.read_value(this->grant_id_, 2));
}

TEST_F(VBusTransactionTest, ExplicitReleaseDuringAWriteCannotConfirmAnUnverifiedValue) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->bus_.release_bus(this->grant_id_);
  this->step_(25);
  this->reply_(123);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_BUS_LOST);
  ASSERT_EQ(this->uart_.writes.size(), 2u);
  EXPECT_EQ(this->uart_.writes[1], wire_datagram(0x0600, 0x0020, 0x2211));
}

TEST_F(VBusTransactionTest, AnOldGrantCannotStartATransactionAfterAnotherLeaseIsAcquired) {
  this->acquire_();
  const uint32_t old_grant = this->grant_id_;
  this->bus_.release_bus(old_grant);
  this->step_(100);
  this->acquire_();
  EXPECT_FALSE(this->bus_.read_value(old_grant, 2));
  EXPECT_FALSE(this->bus_.write_value(old_grant, 2, 123));
  EXPECT_TRUE(this->bus_.read_value(this->grant_id_, 2));
}

TEST_F(VBusTransactionTest, TwoManagersDoNotSharePendingRequestsOrResults) {
  TransmitTestUART other_uart;
  VBus other_bus;
  VBusTransmitter other_tx{&other_bus};
  VBusTransactions other_manager{&other_bus, &other_tx};
  other_bus.set_uart_parent(&other_uart);
  other_bus.set_transmitter(&other_tx);
  other_bus.set_transactions(&other_manager);
  other_tx.set_uart_parent(&other_uart);
  other_tx.set_destination(0x2211);
  other_manager.setup();
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  EXPECT_FALSE(other_bus.read_value(this->grant_id_, 2));
  other_uart.inject(wire_datagram(0x0100, 0x2211, 0x0020, 2, 999));
  this->step_(25);
  other_bus.loop();
  EXPECT_TRUE(this->results_.empty());
  this->reply_(123);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].value, 123u);
  EXPECT_TRUE(other_uart.writes.empty());
}

TEST_F(VBusTransactionTest, TimeoutsAndSuccessfulRepliesWorkAcrossClockRollover) {
  this->now_ = 0xffffff60;
  this->step_(10);
  this->step_(10);
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(60);
  this->reply_(123);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 3));
  this->step_(100);
  ASSERT_EQ(this->results_.size(), 2u);
  EXPECT_EQ(this->results_[1].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
}

class VBusTransactionSplitTest : public VBusTransactionTest, public ::testing::WithParamInterface<int> {};
TEST_P(VBusTransactionSplitTest, EveryReplySplitPreservesThePendingRead) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(25);
  const auto reply = wire_datagram(0x0100, 0x2211, 0x0020, 2, 0x89abcdef);
  this->receive_({reply.begin(), reply.begin() + GetParam()});
  EXPECT_TRUE(this->results_.empty());
  this->step_(10);
  this->receive_({reply.begin() + GetParam(), reply.end()});
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
  EXPECT_EQ(this->results_[0].value, 0x89abcdef);
}
INSTANTIATE_TEST_SUITE_P(EverySplit, VBusTransactionSplitTest, ::testing::Range(1, 16));

class VBusTransactionTimeoutTest : public VBusTransactionTest, public ::testing::WithParamInterface<int> {};
TEST_P(VBusTransactionTimeoutTest, ResponseTimeoutHasAnExactBoundary) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(GetParam());
  this->reply_(123);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, GetParam() < 100 ? TransactionStatus::TRANSACTION_STATUS_SUCCESS
                                                       : TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
}
INSTANTIATE_TEST_SUITE_P(Deadline, VBusTransactionTimeoutTest, ::testing::Values(99, 100, 101));

class VBusTransactionCorruptTest : public VBusTransactionTest, public ::testing::WithParamInterface<int> {};
TEST_P(VBusTransactionCorruptTest, CorruptWriteAckNeverStartsVerificationOrReportsSuccess) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->step_(25);
  auto reply = wire_datagram(0x0100, 0x2211, 0x0020, 2, 123);
  reply[1 + GetParam() / 8] ^= 1u << (GetParam() % 8);
  this->receive_(reply);
  this->step_(500);
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_NE(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
}
INSTANTIATE_TEST_SUITE_P(EveryBodyBit, VBusTransactionCorruptTest, ::testing::Range(0, 120));

class VBusTransactionPayloadTest : public VBusTransactionTest, public ::testing::WithParamInterface<int> {};
TEST_P(VBusTransactionPayloadTest, ReadAndVerifiedWritePreserveAllPayloadHighBits) {
  const uint8_t septet = GetParam();
  uint16_t id = 0x1234 | ((septet & 1) << 7) | ((septet & 2) << 14);
  uint32_t value = 0x12345678;
  for (uint8_t i = 0; i < 4; ++i)
    value |= uint32_t((septet >> (i + 2)) & 1) << (7 + 8 * i);
  this->acquire_();
  ASSERT_TRUE(this->bus_.read_value(this->grant_id_, id));
  this->step_(25);
  this->reply_(value, id);
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, id, value));
  this->step_(25);
  this->reply_(0, id);
  this->step_(25);
  this->reply_(value, id);
  ASSERT_EQ(this->results_.size(), 2u);
  for (const auto &result : this->results_) {
    EXPECT_EQ(result.status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
    EXPECT_EQ(result.value_id, id);
    EXPECT_EQ(result.value, value);
  }
}
INSTANTIATE_TEST_SUITE_P(EverySeptet, VBusTransactionPayloadTest, ::testing::Range(0, 64));
#endif

#ifdef USE_VBUS_POLLING
class VBusPollingTest : public VBusTransactionTest {
 protected:
  void SetUp() override {
    VBusTransactionTest::SetUp();
    this->bus_.set_polling(&this->polling_);
    this->polling_.register_value(1, false);
    this->polling_.register_value(2, false);
    this->polling_.register_value(3, true);
    this->polling_.set_update_interval(60000);
    this->polling_.add_on_value_callback(
        [this](uint16_t id, uint32_t value) { this->values_.emplace_back(id, value); });
    this->polling_.add_on_result_callback(
        [this](uint32_t grant, uint16_t command, uint16_t id, uint32_t value, TransactionStatus status) {
          this->queue_results_.push_back({grant, command, id, value, status});
        });
    this->polling_.setup();
  }
  void tick_(uint32_t elapsed = 0) {
    do {
      const uint32_t step = std::min(elapsed, uint32_t{10});
      this->now_ += step;
      elapsed -= step;
      LoopBlockingGuard guard{nullptr, nullptr, this->now_};
      this->bus_.loop();
      this->tx_.loop();
      this->transactions_.loop();
      this->polling_.loop();
    } while (elapsed > 0);
  }
  void grant_() {
    this->tick_();
    this->uart_.inject(wire_datagram());
    this->tick_();
    this->tick_(100);
  }
  void answer_(uint16_t id, uint32_t value) {
    this->tick_(20);
    this->uart_.inject(wire_datagram(0x0100, 0x2211, 0x0020, id, value));
    this->tick_();
  }
  std::vector<uint8_t> last_() { return this->uart_.writes.back(); }
  void finish_initial_() {
    this->grant_();
    this->answer_(1, 10);
    this->answer_(2, 20);
    this->tick_(20);
  }
  VBusPolling polling_{&this->transactions_, &this->tx_};
  std::vector<std::pair<uint16_t, uint32_t>> values_;
  std::vector<TransactionResult> queue_results_;
};

TEST_F(VBusPollingTest, PollsOncePerIdSkipsRxAndSuppressesUnchangedPublications) {
  this->polling_.register_value(1, false);
  this->polling_.update();
  this->finish_initial_();
  ASSERT_EQ(this->values_.size(), 2u);
  ASSERT_EQ(this->uart_.writes.size(), 3u);
  EXPECT_EQ(this->last_(), wire_datagram(0x0600, 0x0020, 0x2211));
  EXPECT_FALSE(this->bus_.queue_read(3));
  EXPECT_FALSE(this->bus_.queue_read(99));
  this->polling_.update();
  this->grant_();
  this->answer_(1, 10);
  this->answer_(2, 21);
  this->tick_(20);
  ASSERT_EQ(this->values_.size(), 3u);
  EXPECT_EQ(this->values_.back(), (std::pair<uint16_t, uint32_t>{2, 21}));
  EXPECT_EQ(this->queue_results_.size(), 4u);
}

TEST_F(VBusPollingTest, WritesHavePriorityAndVerificationSatisfiesPendingPoll) {
  ASSERT_TRUE(this->bus_.queue_write(2, 42));
  this->grant_();
  EXPECT_EQ(this->last_(), wire_datagram(0x0200, 0x0020, 0x2211, 2, 42));
  this->answer_(2, 0);
  EXPECT_EQ(this->last_(), wire_datagram(0x0300, 0x0020, 0x2211, 2));
  this->answer_(2, 42);
  EXPECT_EQ(this->last_(), wire_datagram(0x0300, 0x0020, 0x2211, 1));
  this->answer_(1, 10);
  this->tick_(20);
  EXPECT_EQ(this->uart_.writes.size(), 4u);
  ASSERT_EQ(this->queue_results_.size(), 2u);
  EXPECT_EQ(this->queue_results_[0].command, 0x0200);
  EXPECT_EQ(this->queue_results_[0].status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
}

TEST_F(VBusPollingTest, DoesNotPreemptInFlightReadButSendsWriteBeforeNextRead) {
  this->grant_();
  ASSERT_TRUE(this->bus_.queue_write(9, 55, 0x0400));
  ASSERT_TRUE(this->bus_.queue_read(1));
  this->polling_.update();
  this->answer_(1, 10);
  EXPECT_EQ(this->last_(), wire_datagram(0x0400, 0x0020, 0x2211, 9, 55));
  this->answer_(9, 0);
  this->answer_(9, 55);
  this->answer_(2, 20);
  this->tick_(20);
  EXPECT_EQ(this->uart_.writes.size(), 5u);
}

TEST_F(VBusPollingTest, PendingReadsAreCoalescedAndScanIsFairAcrossUpdates) {
  this->grant_();
  for (unsigned i = 0; i < 20; ++i) {
    EXPECT_TRUE(this->bus_.queue_read(2));
    this->polling_.update();
  }
  this->answer_(1, 10);
  this->answer_(2, 20);
  this->tick_(20);
  EXPECT_EQ(this->uart_.writes.size(), 3u);
}

TEST_F(VBusPollingTest, ReadBatchEndsAtConfiguredLimitAndResumesOnFreshGrant) {
  this->polling_.set_batch_size(1);
  this->grant_();
  this->answer_(1, 10);
  this->tick_(20);
  EXPECT_EQ(this->last_(), wire_datagram(0x0600, 0x0020, 0x2211));
  this->grant_();
  EXPECT_EQ(this->last_(), wire_datagram(0x0300, 0x0020, 0x2211, 2));
  this->answer_(2, 20);
  this->tick_(20);
  EXPECT_EQ(this->uart_.writes.size(), 4u);
}

TEST_F(VBusPollingTest, MaximumBatchIsSixteenReadsEvenWithLongerLease) {
  this->tx_.set_lease_duration(1000);
  this->transactions_.set_response_timeout(20);
  for (uint16_t id = 4; id <= 20; ++id)
    this->polling_.register_value(id, false);
  this->polling_.update();
  this->grant_();
  for (uint16_t id : {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17}) {
    // Replies need >=20ms, so allow a 30ms timeout for these boundary frames.
    this->transactions_.set_response_timeout(30);
    this->answer_(id, id);
  }
  this->tick_(20);
  ASSERT_EQ(this->uart_.writes.size(), 17u);
  EXPECT_EQ(this->last_(), wire_datagram(0x0600, 0x0020, 0x2211));
  this->grant_();
  EXPECT_EQ(this->last_(), wire_datagram(0x0300, 0x0020, 0x2211, 18));
}

TEST_F(VBusPollingTest, LeaseBudgetReleasesBeforeStartingWriteThatCannotFit) {
  this->grant_();
  this->tick_(50);
  this->answer_(1, 10);
  ASSERT_TRUE(this->bus_.queue_write(9, 55));
  this->tick_(50);
  this->answer_(2, 20);
  this->tick_(20);
  ASSERT_EQ(this->uart_.writes.size(), 3u);
  EXPECT_EQ(this->last_(), wire_datagram(0x0600, 0x0020, 0x2211));
  this->grant_();
  EXPECT_EQ(this->last_(), wire_datagram(0x0200, 0x0020, 0x2211, 9, 55));
}

TEST_F(VBusPollingTest, WriteFifoIsBoundedAndDoesNotReplaceDistinctRequests) {
  for (uint32_t i = 0; i < 16; ++i)
    ASSERT_TRUE(this->bus_.queue_write(9, i));
  EXPECT_FALSE(this->bus_.queue_write(9, 16));
  this->grant_();
  for (uint32_t i = 0; i < 3; ++i) {
    EXPECT_EQ(this->last_(), wire_datagram(0x0200, 0x0020, 0x2211, 9, i));
    this->answer_(9, 0);
    this->answer_(9, i);
    this->tick_(20);
    if (i < 2 && this->last_() == wire_datagram(0x0600, 0x0020, 0x2211)) {
      this->tick_(20);
      this->grant_();
    }
  }
}

TEST_F(VBusPollingTest, UnsentWritesExpireInsteadOfExecutingAfterAnOutage) {
  ASSERT_TRUE(this->bus_.queue_write(9, 123));
  this->tick_(10050);
  EXPECT_TRUE(this->uart_.writes.empty());
  ASSERT_EQ(this->queue_results_.size(), 1u);
  EXPECT_EQ(this->queue_results_[0].grant_id, 0u);
  EXPECT_EQ(this->queue_results_[0].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  this->grant_();
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusPollingTest, FailedWritesAreNotRetriedAndDoNotPublishZero) {
  ASSERT_TRUE(this->bus_.queue_write(1, 123));
  this->grant_();
  this->tick_(100);
  ASSERT_EQ(this->queue_results_.size(), 1u);
  EXPECT_EQ(this->queue_results_[0].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  EXPECT_TRUE(this->values_.empty());
  this->tick_(400);
  this->grant_();
  EXPECT_EQ(this->last_(), wire_datagram(0x0300, 0x0020, 0x2211, 1));
  EXPECT_EQ(
      std::count(this->uart_.writes.begin(), this->uart_.writes.end(), wire_datagram(0x0200, 0x0020, 0x2211, 1, 123)),
      1);
}

TEST_F(VBusPollingTest, FullRawRangeIsCachedAndMismatchDoesNotPublish) {
  ASSERT_TRUE(this->bus_.queue_write(1, 0xffffffff));
  this->grant_();
  this->answer_(1, 0);
  this->answer_(1, 0xffffffff);
  ASSERT_EQ(this->values_.size(), 1u);
  EXPECT_EQ(this->values_[0].second, 0xffffffff);
  this->answer_(2, 20);
  this->tick_(20);
  const size_t published = this->values_.size();
  ASSERT_TRUE(this->bus_.queue_write(2, 42));
  this->grant_();
  this->answer_(2, 0);
  this->answer_(2, 43);
  EXPECT_EQ(this->values_.size(), published);
  EXPECT_EQ(this->queue_results_.back().status, TransactionStatus::TRANSACTION_STATUS_VERIFY_MISMATCH);
}

TEST_F(VBusPollingTest, RawCallbacksAndRxOnlyHubRemainAvailableButManualTxIsRejected) {
  unsigned raw = 0;
  this->bus_.add_on_datagram_callback([&raw](uint16_t, uint16_t, uint16_t, uint16_t, uint32_t) { ++raw; });
  EXPECT_FALSE(this->bus_.request_bus());
  EXPECT_FALSE(this->bus_.read_value(1, 1));
  EXPECT_FALSE(this->bus_.write_value(1, 1, 1));
  EXPECT_FALSE(this->bus_.send_datagram(1, 0x0300, 1, 0));
  VBus rx_only;
  EXPECT_FALSE(rx_only.queue_read(1));
  EXPECT_FALSE(rx_only.queue_write(1, 1));
  this->grant_();
  this->bus_.release_bus(this->grant_id_);
  this->answer_(1, 10);
  this->answer_(2, 20);
  EXPECT_EQ(raw, 3u);
}

TEST_F(VBusPollingTest, ShutdownCancelsEverythingAndLateCallbacksCannotRestartTx) {
  ASSERT_TRUE(this->bus_.queue_write(1, 42));
  this->grant_();
  this->polling_.on_shutdown();
  this->answer_(1, 0);
  this->polling_.update();
  EXPECT_FALSE(this->bus_.queue_read(1));
  EXPECT_FALSE(this->bus_.queue_write(1, 1));
  this->tick_(20000);
  this->grant_();
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_TRUE(this->queue_results_.empty());
  EXPECT_TRUE(this->values_.empty());
}

TEST_F(VBusPollingTest, ResultCallbackMayEnqueueWriteWithoutCorruptingTheCompletedRead) {
  this->polling_.add_on_result_callback([this](uint32_t, uint16_t command, uint16_t id, uint32_t, TransactionStatus) {
    if (command == 0x0300 && id == 1)
      EXPECT_TRUE(this->bus_.queue_write(9, 99));
  });
  this->grant_();
  this->answer_(1, 10);
  EXPECT_EQ(this->last_(), wire_datagram(0x0200, 0x0020, 0x2211, 9, 99));
  ASSERT_EQ(this->queue_results_.size(), 1u);
  EXPECT_EQ(this->queue_results_[0].value_id, 1u);
}
TEST_F(VBusPollingTest, BackpressureNeverSubmitsExpiredOrUnsentWrites) {
  this->finish_initial_();
  this->uart_.room = 15;
  ASSERT_TRUE(this->bus_.queue_write(9, 55));
  const size_t before = this->uart_.writes.size();
  this->grant_();
  this->tick_(10100);
  this->uart_.room = 16;
  this->grant_();
  EXPECT_EQ(this->uart_.writes.size(), before);
  EXPECT_EQ(this->queue_results_.back().grant_id, 0u);
}

TEST_F(VBusPollingTest, ExpiryReportsEachQueuedWriteInOrderAndAcceptsNewWork) {
  this->finish_initial_();
  for (uint16_t id = 10; id < 26; ++id)
    EXPECT_TRUE(this->bus_.queue_write(id, id));
  this->tick_(10200);
  ASSERT_EQ(this->queue_results_.size(), 18u);
  for (size_t i = 2; i < this->queue_results_.size(); ++i) {
    EXPECT_EQ(this->queue_results_[i].value_id, i + 8);
    EXPECT_EQ(this->queue_results_[i].grant_id, 0u);
    EXPECT_EQ(this->queue_results_[i].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  }
  EXPECT_TRUE(this->bus_.queue_write(9, 42));
  this->grant_();
  EXPECT_EQ(this->last_(), wire_datagram(0x0200, 0x0020, 0x2211, 9, 42));
}

TEST_F(VBusPollingTest, GrantWaitAndQueuedExpirySurviveMillisRollover) {
  this->finish_initial_();
  this->now_ = 0xfffffff0;
  this->tick_();
  EXPECT_TRUE(this->bus_.queue_write(9, 42));
  this->tick_(10050);
  EXPECT_EQ(this->queue_results_.back().grant_id, 0u);
  EXPECT_EQ(this->queue_results_.back().status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  EXPECT_EQ(this->uart_.writes.size(), 3u);
}

TEST_F(VBusPollingTest, InvalidQueuedWritesCannotOccupyTheQueue) {
  for (uint16_t command : {0, 0x0100, 0x0300, 0x0500, 0x0600, 0xffff})
    EXPECT_FALSE(this->bus_.queue_write(9, 42, command));
  this->finish_initial_();
  EXPECT_EQ(this->uart_.writes.size(), 3u);
}

TEST_F(VBusPollingTest, WritesToRxCoveredValuesStillVerifyWithoutDuplicatePublication) {
  this->finish_initial_();
  EXPECT_TRUE(this->bus_.queue_write(3, 42));
  this->grant_();
  this->answer_(3, 0);
  this->answer_(3, 42);
  this->tick_(20);
  EXPECT_EQ(this->values_.size(), 2u);
  EXPECT_EQ(this->queue_results_.back().status, TransactionStatus::TRANSACTION_STATUS_SUCCESS);
}

TEST_F(VBusPollingTest, FailedRefreshRetainsCacheAndUnchangedRecoveryDoesNotRepublish) {
  this->finish_initial_();
  EXPECT_TRUE(this->bus_.queue_read(1));
  this->grant_();
  this->tick_(500);
  EXPECT_EQ(this->values_.size(), 2u);
  EXPECT_TRUE(this->bus_.queue_read(1));
  this->grant_();
  this->answer_(1, 10);
  EXPECT_EQ(this->values_.size(), 2u);
}

TEST_F(VBusPollingTest, ValueCallbackCanShutDownWithoutFurtherCommandsOrResults) {
  this->polling_.add_on_value_callback([this](uint16_t, uint32_t) { this->polling_.on_shutdown(); });
  this->grant_();
  this->answer_(1, 10);
  this->tick_(1000);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_TRUE(this->queue_results_.empty());
}

class VBusPollingBatchTest : public VBusPollingTest, public ::testing::WithParamInterface<unsigned> {};

TEST_P(VBusPollingBatchTest, EveryBatchSizeSplitsAtTheExactLimit) {
  this->tx_.set_lease_duration(1000);
  this->transactions_.set_response_timeout(30);
  this->polling_.set_batch_size(GetParam());
  for (uint16_t id = 4; id <= 20; ++id)
    this->polling_.register_value(id, false);
  this->polling_.update();
  this->grant_();
  for (unsigned i = 0; i < GetParam(); ++i)
    this->answer_(i < 2 ? i + 1 : i + 2, i);
  this->tick_(20);
  EXPECT_EQ(this->uart_.writes.size(), GetParam() + 1);
  EXPECT_EQ(this->last_(), wire_datagram(0x0600, 0x0020, 0x2211));
  this->grant_();
  const uint16_t next_id = GetParam() == 1 ? 2 : GetParam() + 2;
  EXPECT_EQ(this->last_(), wire_datagram(0x0300, 0x0020, 0x2211, next_id));
}

INSTANTIATE_TEST_SUITE_P(AllBatchSizes, VBusPollingBatchTest, ::testing::Range(1u, 17u));

class VBusP2SensorTest : public VBusPollingTest {
 protected:
  void SetUp() override {
    VBusPollingTest::SetUp();
    this->signed_.setup();
    this->unsigned_.setup();
    this->duplicate_.setup();
    this->signed_.add_on_state_callback([this](float) { ++this->signed_updates_; });
    this->unsigned_.add_on_state_callback([this](float) { ++this->unsigned_updates_; });
  }
  VBusP2Sensor signed_{&this->polling_, 1, true};
  VBusP2Sensor unsigned_{&this->polling_, 2, false};
  VBusP2Sensor duplicate_{&this->polling_, 1, false};
  unsigned signed_updates_{0};
  unsigned unsigned_updates_{0};
};

TEST_F(VBusP2SensorTest, PublishesOnlyItsIdAndInterpretsSignedRawValues) {
  EXPECT_FALSE(this->signed_.has_state());
  EXPECT_FALSE(this->unsigned_.has_state());
  this->grant_();
  this->answer_(1, 0xffffffff);
  EXPECT_FLOAT_EQ(this->signed_.state, -1);
  EXPECT_FLOAT_EQ(this->duplicate_.state, static_cast<float>(0xffffffffu));
  EXPECT_FALSE(this->unsigned_.has_state());
  this->answer_(2, 0xffffffff);
  EXPECT_FLOAT_EQ(this->unsigned_.state, static_cast<float>(0xffffffffu));
  EXPECT_EQ(this->signed_updates_, 1u);
  EXPECT_EQ(this->unsigned_updates_, 1u);
  this->tick_(20);
  EXPECT_EQ(this->uart_.writes.size(), 3u);
}

TEST_F(VBusP2SensorTest, UnchangedReadsAndFailedRefreshDoNotRepublishOrClearState) {
  this->finish_initial_();
  EXPECT_TRUE(this->bus_.queue_read(1));
  this->grant_();
  this->answer_(1, 10);
  this->tick_(20);
  EXPECT_EQ(this->signed_updates_, 1u);
  EXPECT_TRUE(this->bus_.queue_read(1));
  this->grant_();
  this->tick_(100);
  EXPECT_EQ(this->signed_updates_, 1u);
  EXPECT_FLOAT_EQ(this->signed_.state, 10);
  EXPECT_TRUE(this->signed_.has_state());
}

TEST_F(VBusP2SensorTest, OnlyVerifiedWritesChangeTheSensor) {
  this->finish_initial_();
  EXPECT_TRUE(this->bus_.queue_write(1, 123));
  this->grant_();
  EXPECT_FLOAT_EQ(this->signed_.state, 10);
  this->answer_(1, 123);
  EXPECT_FLOAT_EQ(this->signed_.state, 10);
  this->answer_(1, 123);
  EXPECT_FLOAT_EQ(this->signed_.state, 123);
  EXPECT_EQ(this->signed_updates_, 2u);
}

TEST_F(VBusP2SensorTest, WrongReadbackCannotReplaceTheLastConfirmedValue) {
  this->finish_initial_();
  EXPECT_TRUE(this->bus_.queue_write(1, 123));
  this->grant_();
  this->answer_(1, 123);
  this->answer_(1, 124);
  EXPECT_FLOAT_EQ(this->signed_.state, 10);
  EXPECT_EQ(this->signed_updates_, 1u);
}

TEST_F(VBusP2SensorTest, ASeparatePollingInstanceCannotPublishIntoThisSensor) {
  VBus other_bus;
  VBusTransmitter other_tx{&other_bus};
  VBusTransactions other_transactions{&other_bus, &other_tx};
  VBusPolling other_polling{&other_transactions, &other_tx};
  VBusP2Sensor other_sensor{&other_polling, 1, true};
  other_sensor.setup();
  this->finish_initial_();
  EXPECT_FALSE(other_sensor.has_state());
  EXPECT_FLOAT_EQ(this->signed_.state, 10);
}

class VBusP2SignedValueTest : public VBusP2SensorTest, public ::testing::WithParamInterface<int32_t> {};

TEST_P(VBusP2SignedValueTest, PreservesSignedBoundaryAndNegativeValues) {
  this->grant_();
  this->answer_(1, static_cast<uint32_t>(GetParam()));
  EXPECT_FLOAT_EQ(this->signed_.state, static_cast<float>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(RawBoundaries, VBusP2SignedValueTest,
                         ::testing::Values(INT32_MIN, -999, -32768, -1, 0, 1, 32767, INT32_MAX));

#endif

}  // namespace
}  // namespace esphome::vbus::testing
