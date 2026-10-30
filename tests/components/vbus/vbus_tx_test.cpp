#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <map>
#include <set>

#include "esphome/components/vbus/vbus.h"
#include "esphome/components/vbus/resol_deltasol_cs_plus.h"
#include "esphome/components/vbus/sensor/vbus_sensor.h"
#include "esphome/components/vbus/number/vbus_number.h"
#include "esphome/components/vbus/select/vbus_select.h"
#ifdef USE_VBUS_WRITE_RESULT
#include "esphome/components/vbus/text_sensor/vbus_text_sensor.h"
#include "esphome/components/text_sensor/filter.h"
#endif
#ifdef USE_VBUS_SETTING_TIME
#include "esphome/components/vbus/datetime/vbus_time.h"
#endif
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

TEST_F(VBusPollingTest, GrantTimeoutCallbackMaySuspendWithoutRestartingTransmission) {
  this->polling_.add_on_result_callback(
      [this](uint32_t, uint16_t, uint16_t, uint32_t, TransactionStatus) { this->bus_.suspend_polling(); });
  this->tick_(10020);
  EXPECT_FALSE(this->bus_.queue_read(1));
  this->grant_();
  EXPECT_TRUE(this->uart_.writes.empty());
  EXPECT_EQ(this->queue_results_.size(), 2u);
}

TEST_F(VBusPollingTest, ReadGrantTimeoutSurvivesMillisRollover) {
  this->finish_initial_();
  this->now_ = 0xfffffff0;
  ASSERT_TRUE(this->bus_.queue_read(1));
  this->tick_(10020);
  ASSERT_EQ(this->queue_results_.size(), 3u);
  EXPECT_EQ(this->queue_results_.back().value_id, 1u);
  EXPECT_EQ(this->queue_results_.back().status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  EXPECT_EQ(this->uart_.writes.size(), 3u);
}

TEST_F(VBusPollingTest, GrantTimeoutCallbackMayShutDownWithoutFurtherResults) {
  this->polling_.add_on_result_callback(
      [this](uint32_t, uint16_t, uint16_t, uint32_t, TransactionStatus) { this->polling_.on_shutdown(); });
  this->tick_(10020);
  ASSERT_EQ(this->queue_results_.size(), 1u);
  EXPECT_EQ(this->queue_results_[0].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  this->grant_();
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusPollingTest, GrantTimeoutCallbacksCanRequeueReadsAndWrites) {
  this->finish_initial_();
  ASSERT_TRUE(this->bus_.queue_read(1));
  ASSERT_TRUE(this->bus_.queue_read(2));
  this->polling_.add_on_result_callback(
      [this](uint32_t, uint16_t command, uint16_t id, uint32_t, TransactionStatus status) {
        if (command == 0x0300 && id == 1 && status == TransactionStatus::TRANSACTION_STATUS_TIMEOUT) {
          EXPECT_TRUE(this->bus_.queue_read(1));
          EXPECT_TRUE(this->bus_.queue_write(9, 99));
        }
      });
  this->tick_(10020);
  ASSERT_EQ(this->queue_results_.size(), 4u);
  this->grant_();
  EXPECT_EQ(this->last_(), wire_datagram(0x0200, 0x0020, 0x2211, 9, 99));
  this->answer_(9, 0);
  this->answer_(9, 99);
  EXPECT_EQ(this->last_(), wire_datagram(0x0300, 0x0020, 0x2211, 1));
  this->answer_(1, 10);
  EXPECT_EQ(this->queue_results_.size(), 6u);
  EXPECT_EQ(this->values_.size(), 2u);
}

TEST_F(VBusPollingTest, GrantWaitExpiryReportsPendingReadsWithoutClearingCachedValues) {
  this->finish_initial_();
  ASSERT_TRUE(this->bus_.queue_read(1));
  ASSERT_TRUE(this->bus_.queue_read(2));
  this->tick_(10020);
  ASSERT_EQ(this->queue_results_.size(), 4u);
  for (size_t i = 2; i < 4; ++i) {
    EXPECT_EQ(this->queue_results_[i].grant_id, 0u);
    EXPECT_EQ(this->queue_results_[i].command, 0x0300u);
    EXPECT_EQ(this->queue_results_[i].value_id, i - 1);
    EXPECT_EQ(this->queue_results_[i].status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  }
  EXPECT_EQ(this->values_.size(), 2u);
  this->tick_(10020);
  EXPECT_EQ(this->queue_results_.size(), 4u);
  EXPECT_EQ(this->uart_.writes.size(), 3u);
}

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
  ASSERT_EQ(this->queue_results_.size(), 3u);
  EXPECT_EQ(this->queue_results_[0].command, 0x0200u);
  EXPECT_EQ(this->queue_results_[0].value_id, 9u);
  EXPECT_EQ(this->queue_results_[1].value_id, 1u);
  EXPECT_EQ(this->queue_results_[2].value_id, 2u);
  for (const auto &result : this->queue_results_) {
    EXPECT_EQ(result.grant_id, 0u);
    EXPECT_EQ(result.status, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  }
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
  rx_only.suspend_polling();
  rx_only.resume_polling();
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

TEST_F(VBusPollingTest, SuspensionBeforeGrantCancelsUnsentWritesAndRejectsNewWork) {
  ASSERT_TRUE(this->bus_.queue_write(9, 99));
  this->tick_();
  this->bus_.suspend_polling();
  this->bus_.suspend_polling();
  EXPECT_FALSE(this->bus_.queue_read(1));
  EXPECT_FALSE(this->bus_.queue_write(9, 100));
  this->polling_.update();
  this->grant_();
  this->tick_(20000);
  EXPECT_TRUE(this->uart_.writes.empty());
  ASSERT_EQ(this->queue_results_.size(), 1u);
  EXPECT_EQ(this->queue_results_[0].grant_id, 0u);
  EXPECT_EQ(this->queue_results_[0].status, TransactionStatus::TRANSACTION_STATUS_CANCELLED);
}

TEST_F(VBusPollingTest, SuspensionCancelsActiveReadAndIgnoresItsLateReply) {
  this->grant_();
  const uint32_t old_grant = this->grant_id_;
  this->bus_.suspend_polling();
  this->answer_(1, 10);
  this->tick_(1000);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_TRUE(this->values_.empty());
  ASSERT_EQ(this->queue_results_.size(), 1u);
  EXPECT_EQ(this->queue_results_[0].grant_id, old_grant);
  EXPECT_EQ(this->queue_results_[0].status, TransactionStatus::TRANSACTION_STATUS_CANCELLED);
  EXPECT_FALSE(this->transactions_.is_busy());
}

TEST_F(VBusPollingTest, SuspensionReportsActiveAndFullQueuedWriteFifoExactlyOnceInOrder) {
  ASSERT_TRUE(this->bus_.queue_write(9, 99));
  this->grant_();
  for (uint16_t id = 10; id < 26; ++id)
    ASSERT_TRUE(this->bus_.queue_write(id, id));
  this->bus_.suspend_polling();
  this->answer_(9, 99);
  this->bus_.suspend_polling();
  ASSERT_EQ(this->queue_results_.size(), 17u);
  for (size_t i = 0; i < this->queue_results_.size(); ++i) {
    EXPECT_EQ(this->queue_results_[i].value_id, i + 9);
    EXPECT_EQ(this->queue_results_[i].status, TransactionStatus::TRANSACTION_STATUS_CANCELLED);
    EXPECT_EQ(this->queue_results_[i].grant_id != 0, i == 0);
  }
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_TRUE(this->values_.empty());
}

TEST_F(VBusPollingTest, SuspensionWhileVerificationWaitsForCapacityCannotSendItLater) {
  ASSERT_TRUE(this->bus_.queue_write(9, 99));
  this->grant_();
  this->uart_.room = 15;
  this->answer_(9, 99);
  this->bus_.suspend_polling();
  this->uart_.room = 16;
  this->tick_(1000);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_FALSE(this->transactions_.is_busy());
  ASSERT_EQ(this->queue_results_.size(), 1u);
  EXPECT_EQ(this->queue_results_[0].status, TransactionStatus::TRANSACTION_STATUS_CANCELLED);
}

TEST_F(VBusPollingTest, SuspensionWhileWaitingForVerificationCannotAcceptLateReadback) {
  ASSERT_TRUE(this->bus_.queue_write(1, 99));
  this->grant_();
  this->answer_(1, 99);
  ASSERT_EQ(this->uart_.writes.size(), 2u);
  this->bus_.suspend_polling();
  this->answer_(1, 99);
  this->tick_(500);
  EXPECT_EQ(this->uart_.writes.size(), 2u);
  EXPECT_TRUE(this->values_.empty());
  ASSERT_EQ(this->queue_results_.size(), 1u);
  EXPECT_EQ(this->queue_results_[0].status, TransactionStatus::TRANSACTION_STATUS_CANCELLED);
}

TEST_F(VBusPollingTest, ResumeOnlyStartsFreshReadsAndNeverReplaysCancelledWrites) {
  ASSERT_TRUE(this->bus_.queue_write(9, 99));
  this->grant_();
  this->bus_.suspend_polling();
  this->bus_.resume_polling();
  this->bus_.resume_polling();
  this->grant_();
  EXPECT_EQ(this->last_(), wire_datagram(0x0300, 0x0020, 0x2211, 1));
  this->answer_(1, 10);
  this->answer_(2, 20);
  this->tick_(20);
  EXPECT_EQ(this->uart_.writes.size(), 4u);
  EXPECT_EQ(
      std::count(this->uart_.writes.begin(), this->uart_.writes.end(), wire_datagram(0x0200, 0x0020, 0x2211, 9, 99)),
      1);
}

TEST_F(VBusPollingTest, CancelledResultsCanResumeAndQueueNewExplicitWork) {
  this->polling_.add_on_result_callback([this](uint32_t, uint16_t, uint16_t id, uint32_t, TransactionStatus status) {
    if (id == 9 && status == TransactionStatus::TRANSACTION_STATUS_CANCELLED) {
      this->bus_.resume_polling();
      EXPECT_TRUE(this->bus_.queue_write(10, 100));
    }
  });
  ASSERT_TRUE(this->bus_.queue_write(9, 99));
  this->grant_();
  this->bus_.suspend_polling();
  this->grant_();
  EXPECT_EQ(this->last_(), wire_datagram(0x0200, 0x0020, 0x2211, 10, 100));
}

TEST_F(VBusPollingTest, ShutdownFromCancellationCallbackStopsFurtherNotificationsAndResume) {
  this->polling_.add_on_result_callback([this](uint32_t, uint16_t, uint16_t, uint32_t, TransactionStatus status) {
    if (status == TransactionStatus::TRANSACTION_STATUS_CANCELLED)
      this->polling_.on_shutdown();
  });
  ASSERT_TRUE(this->bus_.queue_write(9, 99));
  ASSERT_TRUE(this->bus_.queue_write(10, 100));
  this->grant_();
  this->bus_.suspend_polling();
  this->bus_.resume_polling();
  EXPECT_FALSE(this->bus_.queue_write(11, 101));
  this->grant_();
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_EQ(this->queue_results_.size(), 1u);
}

TEST_F(VBusP2SensorTest, SuspensionPreservesConfirmedSensorAndRecoversWithoutDuplicatePublication) {
  this->finish_initial_();
  this->bus_.suspend_polling();
  this->bus_.resume_polling();
  this->grant_();
  this->answer_(1, 10);
  this->answer_(2, 20);
  this->tick_(20);
  EXPECT_EQ(this->signed_updates_, 1u);
  EXPECT_EQ(this->unsigned_updates_, 1u);
  EXPECT_FLOAT_EQ(this->signed_.state, 10);
}

TEST_F(VBusPollingTest, RxPacketsContinueWhilePollingIsSuspended) {
  class Observer : public VBusListener {
   public:
    unsigned count{0};

   protected:
    void handle_message(std::vector<uint8_t> &) override { ++this->count; }
  } observer;
  this->bus_.register_listener(&observer);
  this->bus_.suspend_polling();
  this->uart_.inject({0xaa, 0x10, 0, 0x11, 0x22, 0x10, 0, 1, 1, 0x2a, 1, 2, 3, 4, 0, 0x75});
  this->tick_();
  EXPECT_EQ(observer.count, 1u);
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(VBusPollingTest, ResumeOnAnAlreadyRunningPollerDoesNotScheduleAnotherScan) {
  this->finish_initial_();
  this->bus_.resume_polling();
  this->grant_();
  EXPECT_EQ(this->uart_.writes.size(), 3u);
}

TEST_F(VBusTransactionTest, ExplicitCancellationNotifiesOnceAndRetiresTheLease) {
  this->acquire_();
  ASSERT_TRUE(this->bus_.write_value(this->grant_id_, 2, 123));
  this->transactions_.cancel();
  this->transactions_.cancel();
  ASSERT_EQ(this->results_.size(), 1u);
  EXPECT_EQ(this->results_[0].status, TransactionStatus::TRANSACTION_STATUS_CANCELLED);
  EXPECT_FALSE(this->bus_.read_value(this->grant_id_, 2));
  this->step_(20);
  this->reply_(123);
  EXPECT_EQ(this->results_.size(), 1u);
}

class VBusSuspendBeforeResultTest : public VBusPollingTest {
 protected:
  void SetUp() override {
    // Register the observer before polling installs its transaction callback.
    VBusTransactionTest::SetUp();  // NOLINT(bugprone-parent-virtual-call)
    this->transactions_.add_on_result_callback(
        [this](uint32_t, uint16_t, uint16_t, uint32_t, TransactionStatus status) {
          if (status == TransactionStatus::TRANSACTION_STATUS_SUCCESS)
            this->bus_.suspend_polling();
        });
    this->bus_.set_polling(&this->polling_);
    this->polling_.register_value(1, false);
    this->polling_.add_on_value_callback(
        [this](uint16_t id, uint32_t value) { this->values_.emplace_back(id, value); });
    this->polling_.add_on_result_callback(
        [this](uint32_t grant, uint16_t command, uint16_t id, uint32_t value, TransactionStatus status) {
          this->queue_results_.push_back({grant, command, id, value, status});
        });
    this->polling_.setup();
  }
};

TEST_F(VBusSuspendBeforeResultTest, EarlierTransactionCallbackCanCancelBeforeCachePublication) {
  this->grant_();
  this->answer_(1, 10);
  EXPECT_TRUE(this->values_.empty());
  ASSERT_EQ(this->queue_results_.size(), 1u);
  EXPECT_EQ(this->queue_results_[0].status, TransactionStatus::TRANSACTION_STATUS_CANCELLED);
  this->tick_(1000);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
}

#ifdef USE_VBUS_CS_PLUS
class CSPlusControllerTest : public VBusTransactionTest {
 protected:
  void SetUp() override {
    VBusTransactionTest::SetUp();
    this->bus_.set_polling(&this->owned_polling_);
    this->owned_polling_.set_update_interval(SCHEDULER_DONT_RUN);
    this->owned_polling_.setup();
    this->controller_.set_update_interval(300000);
    this->controller_.add_on_snapshot_callback([this] { ++this->snapshots_; });
    this->controller_.setup();
    for (size_t i = 0; i < cs_plus::SETTING_COUNT; ++i) {
      const auto spec = cs_plus::setting_at(i);
      if (!spec) {
        FAIL() << "Missing metadata";
      }
      this->source_values_[spec->index] = spec->minimum;
    }
    this->source_values_[1] = 1;
    this->source_values_[10] = 0;
    this->source_values_[21] = 6;
    this->source_values_[48] = 35;
    this->source_values_[50] = 65;
    this->source_values_[81] = 1;
    this->source_values_[90] = 1;
    this->source_values_[385] = 100;
    this->source_values_[401] = 40;
    this->source_values_[411] = 120;
    this->source_values_[12549] = 2;
    this->message_();
  }
  void TearDown() override { this->controller_.on_shutdown(); }
  void tick_(uint32_t elapsed = 0) {
    do {
      const uint32_t step = std::min(elapsed, uint32_t{10});
      this->now_ += step;
      elapsed -= step;
      LoopBlockingGuard guard{nullptr, nullptr, this->now_};
      this->bus_.loop();
      this->tx_.loop();
      this->transactions_.loop();
      this->owned_polling_.loop();
      this->controller_.loop();
    } while (elapsed > 0);
  }
  void message_(uint16_t version = 111, size_t length = 40, uint16_t source = 0x2211) {
    std::vector<uint8_t> payload(40);
    // P1 System/UnitType intentionally disagree with the authoritative P2 values.
    payload[16] = 1;
    payload[17] = 9;
    payload[22] = 62;
    payload[32] = version;
    payload[33] = version >> 8;
    payload.resize(length);
    this->controller_.on_message(0x0100, source, 0x0010, payload);
    this->tick_();
  }
  std::vector<uint16_t> group_(uint32_t response_delay = 20) {
    std::vector<uint16_t> ids;
    size_t cursor = this->uart_.writes.size();
    this->tick_();
    this->uart_.inject(wire_datagram());
    this->tick_();
    this->tick_(100);
    for (size_t pass = 0; pass < 100; ++pass) {
      if (cursor == this->uart_.writes.size()) {
        this->tick_(10);
        continue;
      }
      const auto frame = this->uart_.writes[cursor++];
      const uint16_t command = frame[6] | (uint16_t(frame[7]) << 8);
      if (command == 0x0600)
        return ids;
      EXPECT_EQ(command, 0x0300);
      const uint16_t id =
          uint16_t(frame[8] | ((frame[14] & 1u) << 7)) | (uint16_t(frame[9] | ((frame[14] & 2u) << 6)) << 8);
      ids.push_back(id);
      this->requested_.push_back(id);
      this->tick_(response_delay);
      this->uart_.inject(wire_datagram(0x0100, 0x2211, 0x0020, id, this->source_values_.at(id)));
      this->tick_();
    }
    ADD_FAILURE() << "Group did not release";
    return ids;
  }
  void complete_(uint32_t response_delay = 20) {
    for (size_t i = 0; i < 30 && this->controller_.is_scanning(); ++i) {
      const auto group = this->group_(response_delay);
      EXPECT_FALSE(group.empty());
      EXPECT_LE(group.size(), 16u);
      this->message_();
    }
    ASSERT_FALSE(this->controller_.is_scanning());
    ASSERT_TRUE(this->controller_.has_fresh_snapshot());
  }
  VBusPolling owned_polling_{&this->transactions_, &this->tx_, false};
  ResolDeltaSolCSPlus controller_{&this->bus_, &this->owned_polling_, &this->tx_};
  std::map<uint16_t, int32_t> source_values_;
  std::vector<uint16_t> requested_;
  size_t snapshots_{0};
};

TEST_F(CSPlusControllerTest, ShortTelemetryDoesNotInvalidateAConfirmedSnapshotOrClearAProfileWarning) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  this->message_(111, 36);
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
  this->message_(112);
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  EXPECT_TRUE(this->controller_.status_has_warning());
  this->message_(111, 36);
  EXPECT_TRUE(this->controller_.status_has_warning());
  EXPECT_FALSE(this->controller_.telemetry_online());
}

TEST_F(CSPlusControllerTest, UnsupportedFirmwareSetsWarningAndSupportedTelemetryClearsIt) {
  ASSERT_FALSE(this->controller_.status_has_warning());
  this->message_(112);
  EXPECT_TRUE(this->controller_.status_has_warning());
  EXPECT_FALSE(this->controller_.telemetry_online());
  EXPECT_FALSE(this->controller_.start_scan());
  this->message_(112);
  EXPECT_TRUE(this->controller_.status_has_warning());
  EXPECT_TRUE(this->uart_.writes.empty());
  this->message_();
  EXPECT_FALSE(this->controller_.status_has_warning());
  EXPECT_TRUE(this->controller_.telemetry_online());
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
}

TEST_F(CSPlusControllerTest, ControlledPollerDoesNotStartGenericInitialOrResumeRefreshes) {
  this->owned_polling_.update();
  this->tick_(1000);
  EXPECT_TRUE(this->uart_.writes.empty());
  this->owned_polling_.suspend();
  EXPECT_TRUE(this->owned_polling_.is_suspended());
  this->owned_polling_.resume();
  EXPECT_FALSE(this->owned_polling_.is_suspended());
  this->tick_(1000);
  EXPECT_TRUE(this->uart_.writes.empty());
}

TEST_F(CSPlusControllerTest, ScanStatusTracksAcceptedScansAndIncompleteAttemptsWithoutInvalidatingRetainedValues) {
  EXPECT_EQ(this->controller_.scan_status(), CSPlusScanStatus::CS_PLUS_SCAN_STATUS_IDLE);
  ASSERT_TRUE(this->controller_.start_scan());
  EXPECT_EQ(this->controller_.scan_status(), CSPlusScanStatus::CS_PLUS_SCAN_STATUS_RUNNING);
  this->complete_();
  EXPECT_EQ(this->controller_.scan_status(), CSPlusScanStatus::CS_PLUS_SCAN_STATUS_COMPLETE);
  this->tick_(60000);
  this->message_();
  ASSERT_TRUE(this->controller_.start_scan());
  this->owned_polling_.suspend();
  EXPECT_EQ(this->controller_.scan_status(), CSPlusScanStatus::CS_PLUS_SCAN_STATUS_INCOMPLETE);
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
}

TEST_F(CSPlusControllerTest, ReadsAuthoritativeContextBeforeFlagsAndWaitsForResumedTelemetry) {
  ASSERT_TRUE(this->controller_.start_scan());
  const auto context = this->group_();
  EXPECT_EQ(context, (std::vector<uint16_t>{1, 10}));
  EXPECT_EQ(this->controller_.completed_scans(), 0u);
  EXPECT_EQ(this->snapshots_, 0u);
  const size_t frames = this->uart_.writes.size();
  this->uart_.inject(wire_datagram());
  this->tick_(100);
  EXPECT_EQ(this->uart_.writes.size(), frames);
  this->message_(111, 39);
  this->message_(111, 40, 0x1234);
  this->tick_(100);
  EXPECT_EQ(this->uart_.writes.size(), frames);
  this->message_();
  this->complete_();
  int32_t value = -1;
  ASSERT_TRUE(this->controller_.get_value(1, value));
  EXPECT_EQ(value, 1);
  ASSERT_TRUE(this->controller_.get_value(10, value));
  EXPECT_EQ(value, 0);
  ASSERT_TRUE(this->controller_.get_value(2, value));
  EXPECT_EQ(value, 62);
  EXPECT_EQ(this->snapshots_, 1u);
}

TEST_F(CSPlusControllerTest, BatchesNeverExceed16AndEachParameterIsReadOnlyOnce) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  const std::set<uint16_t> unique(this->requested_.begin(), this->requested_.end());
  EXPECT_EQ(unique.size(), this->requested_.size());
  EXPECT_FALSE(unique.contains(2));
  EXPECT_FALSE(unique.contains(36));
  EXPECT_FALSE(unique.contains(37));
  EXPECT_FALSE(unique.contains(2561));
  EXPECT_FALSE(unique.contains(2771));
  EXPECT_EQ(this->controller_.request_count(), unique.size());
  EXPECT_EQ(this->controller_.received_count(), unique.size());
  EXPECT_EQ(this->controller_.release_count(), this->controller_.group_count());
  EXPECT_GT(this->controller_.group_count(), 2);
}

TEST_F(CSPlusControllerTest, SlowRepliesSplitPendingReadsAcrossLeasesWithTelemetryBetweenEach) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_(70);
  EXPECT_GT(this->controller_.group_count(), 4);
  EXPECT_EQ(this->controller_.request_count(), this->controller_.received_count());
  const std::set<uint16_t> unique(this->requested_.begin(), this->requested_.end());
  EXPECT_EQ(unique.size(), this->requested_.size());
}

TEST_F(CSPlusControllerTest, DoesNotPublishPartialResultsOrMarkPreviousSnapshotUnavailable) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  int32_t value = -1;
  ASSERT_TRUE(this->controller_.get_value(55, value));
  EXPECT_EQ(value, 1);
  this->tick_(60000);
  this->message_();
  this->source_values_[55] = 4;
  ASSERT_TRUE(this->controller_.start_scan());
  this->group_();
  ASSERT_TRUE(this->controller_.get_value(55, value));
  EXPECT_EQ(value, 1);
  EXPECT_EQ(this->snapshots_, 1u);
  this->message_();
  this->complete_();
  ASSERT_TRUE(this->controller_.get_value(55, value));
  EXPECT_EQ(value, 4);
  EXPECT_EQ(this->snapshots_, 2u);
}

TEST_F(CSPlusControllerTest, MissingResumedTelemetryAbortsWithoutPublishingPartialSnapshot) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->group_();
  this->tick_(15000);
  EXPECT_FALSE(this->controller_.is_scanning());
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  EXPECT_EQ(this->snapshots_, 0u);
}

TEST_F(CSPlusControllerTest, SuspensionCancelsTheScanWithoutRestartingOnLateReply) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->tick_();
  this->uart_.inject(wire_datagram());
  this->tick_();
  this->tick_(100);
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  this->owned_polling_.suspend();
  EXPECT_FALSE(this->controller_.is_scanning());
  this->uart_.inject(wire_datagram(0x0100, 0x2211, 0x0020, 1, 1));
  this->tick_(100);
  EXPECT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_EQ(this->snapshots_, 0u);
}

TEST_F(CSPlusControllerTest, InvalidContextStopsTheScanAndNeverPublishes) {
  this->source_values_[1] = 0;
  ASSERT_TRUE(this->controller_.start_scan());
  this->group_();
  EXPECT_FALSE(this->controller_.is_scanning());
  EXPECT_EQ(this->snapshots_, 0u);
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
}

TEST_F(CSPlusControllerTest, WrongFirmwareInvalidatesSnapshotAndDoesNotRestoreItFromOnePacket) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  this->message_(112);
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  EXPECT_FALSE(this->controller_.start_scan());
  this->message_();
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  EXPECT_EQ(this->snapshots_, 2u);
}

TEST_F(CSPlusControllerTest, SnapshotExpiresAfter660SecondsWithoutARefresh) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  this->tick_(660000);
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
  this->tick_(1);
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  int32_t value = 0;
  EXPECT_FALSE(this->controller_.get_value(1, value));
}

TEST_F(CSPlusControllerTest, AutomaticScanWaits30SecondsThenUsesTheConfiguredInterval) {
  this->controller_.update();
  EXPECT_FALSE(this->controller_.is_scanning());
  this->tick_(29999);
  this->message_();
  this->controller_.update();
  EXPECT_FALSE(this->controller_.is_scanning());
  this->tick_(1);
  this->controller_.update();
  EXPECT_TRUE(this->controller_.is_scanning());
  this->complete_();
  this->tick_(60000);
  this->message_();
  this->controller_.update();
  EXPECT_FALSE(this->controller_.is_scanning());
  this->tick_(240000);
  this->message_();
  this->controller_.update();
  EXPECT_TRUE(this->controller_.is_scanning());
}

TEST_F(CSPlusControllerTest, NeverDisablesAutomaticBootScansButAllowsExplicitRefresh) {
  this->controller_.set_update_interval(SCHEDULER_DONT_RUN);
  this->tick_(60000);
  this->message_();
  this->controller_.update();
  EXPECT_FALSE(this->controller_.is_scanning());
  EXPECT_TRUE(this->controller_.start_scan());
}

TEST_F(CSPlusControllerTest, OtaResumeSchedulesOneFreshScanWithTheMinimumAttemptInterval) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->group_();
  this->owned_polling_.suspend();
  this->owned_polling_.resume();
  this->controller_.update();
  EXPECT_FALSE(this->controller_.is_scanning());
  this->tick_(60000);
  this->message_();
  this->controller_.update();
  EXPECT_TRUE(this->controller_.is_scanning());
}

TEST_F(CSPlusControllerTest, ScanRateLimitAndShutdownArePermanentUntilNormalRestart) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  EXPECT_FALSE(this->controller_.start_scan());
  this->controller_.on_shutdown();
  this->tick_(60000);
  this->message_();
  EXPECT_FALSE(this->controller_.start_scan());
}
#endif

#ifdef USE_VBUS_CS_PLUS_WRITE
class CSPlusSettingWriteTest : public CSPlusControllerTest {
 protected:
  void SetUp() override {
    CSPlusControllerTest::SetUp();
    this->controller_.set_require_write_arm(false);
    this->controller_.add_on_setting_result_callback(
        [this](uint16_t id, SettingWriteStatus status) { this->write_results_.emplace_back(id, status); });
    ASSERT_TRUE(this->controller_.start_scan());
    this->complete_();
  }
  void preflight_(uint16_t index = 48, float value = 60) {
    ASSERT_TRUE(this->controller_.request_setting(index, value));
    const auto ids = this->group_();
    EXPECT_FALSE(ids.empty());
    EXPECT_EQ(this->controller_.writes_sent(), 0u);
    this->message_();
  }
  void acquire_write_(uint16_t index = 48, uint32_t value = 60) {
    this->tick_();
    this->uart_.inject(wire_datagram());
    this->tick_();
    this->tick_(100);
    ASSERT_EQ(this->uart_.writes.back(), wire_datagram(0x0200, 0x0020, 0x2211, index, value));
  }
  void reply_write_(uint32_t readback, uint16_t index = 48) {
    this->tick_(20);
    this->uart_.inject(wire_datagram(0x0100, 0x2211, 0x0020, index, 0));
    this->tick_();
    this->tick_(20);
    ASSERT_EQ(this->uart_.writes.back(), wire_datagram(0x0300, 0x0020, 0x2211, index));
    this->uart_.inject(wire_datagram(0x0100, 0x2211, 0x0020, index, readback));
    this->tick_();
    this->tick_(20);
  }
  std::vector<std::pair<uint16_t, SettingWriteStatus>> write_results_;
};

TEST_F(CSPlusSettingWriteTest, RejectedRequestCallbackCanStartANewRequestWithoutBeingOverwritten) {
  bool callback_started_request = false;
  this->controller_.add_on_setting_result_callback([&](uint16_t index, SettingWriteStatus status) {
    if (index == 0 && status == SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED)
      callback_started_request = this->controller_.request_setting(48, 60);
  });
  EXPECT_FALSE(this->controller_.request_setting(0, 1));
  ASSERT_TRUE(callback_started_request);
  EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_PREFLIGHT);
  this->group_();
  this->message_();
  this->acquire_write_();
  this->reply_write_(60);
  this->message_();
  EXPECT_EQ(this->write_results_.back(),
            std::make_pair(uint16_t{48}, SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS));
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
}

TEST_F(CSPlusSettingWriteTest, BusyRejectionDoesNotOverwriteTheActiveWriteOrItsResult) {
  this->preflight_();
  const auto active_status = this->controller_.write_status();
  EXPECT_FALSE(this->controller_.request_setting(55, 4));
  ASSERT_EQ(this->write_results_.size(), 1u);
  EXPECT_EQ(this->write_results_[0], std::make_pair(uint16_t{55}, SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED));
  EXPECT_EQ(this->controller_.write_status(), active_status);
  this->controller_.arm_writes(true);
  EXPECT_FALSE(this->controller_.writes_armed());
  this->acquire_write_();
  this->reply_write_(60);
  this->message_();
  ASSERT_EQ(this->write_results_.size(), 2u);
  EXPECT_EQ(this->write_results_[1], std::make_pair(uint16_t{48}, SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS));
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
}

TEST_F(CSPlusSettingWriteTest, RejectedRequestsPublishTheirOwnIndexWithoutSendingWrites) {
  for (const auto [index, value] :
       std::array<std::pair<uint16_t, float>, 5>{{{0, 1}, {48, 61}, {48, 200}, {48, INFINITY}, {55, 0}}}) {
    const size_t previous = this->write_results_.size();
    EXPECT_FALSE(this->controller_.request_setting(index, value));
    ASSERT_EQ(this->write_results_.size(), previous + 1);
    EXPECT_EQ(this->write_results_.back(), std::make_pair(index, SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED));
    EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED);
  }
  this->controller_.set_require_write_arm(true);
  EXPECT_FALSE(this->controller_.request_setting(48, 60));
  EXPECT_EQ(this->write_results_.back().second, SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
}

#ifdef USE_VBUS_WRITE_RESULT
TEST_F(CSPlusSettingWriteTest, WriteResultReservesRawStorageOnlyForItsOwnFilters) {
  VBusWriteResultTextSensor plain;
  VBusWriteResultTextSensor filtered{true};
  text_sensor::ToUpperFilter plain_filter, configured_filter;
  plain.add_filter(&plain_filter);
  filtered.add_filter(&configured_filter);
  EXPECT_LT(plain.get_raw_state().capacity(), 144u);
  EXPECT_GE(filtered.get_raw_state().capacity(), 144u);
}

TEST_F(CSPlusSettingWriteTest, OptionalWriteResultExplainsRejectionsAndSuppressesRepeatedStates) {
  VBusWriteResultTextSensor result;
  EXPECT_GE(result.state.capacity(), 144u);
  unsigned publications = 0;
  result.add_on_state_callback([&](const std::string &) { ++publications; });
  this->controller_.set_last_write_result_text_sensor(&result);
  this->controller_.set_require_write_arm(true);
  EXPECT_FALSE(this->controller_.request_setting(48, 60));
  EXPECT_EQ(result.state, "Setting 48: writes are not armed");
  EXPECT_FALSE(this->controller_.request_setting(48, 60));
  EXPECT_EQ(publications, 1u);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
  this->controller_.set_last_write_result_text_sensor(nullptr);
}

TEST_F(CSPlusSettingWriteTest, OptionalWriteResultReportsVerifiedWritesWithoutExtraRequests) {
  VBusWriteResultTextSensor result;
  this->controller_.set_last_write_result_text_sensor(&result);
  this->preflight_();
  this->acquire_write_();
  this->reply_write_(60);
  EXPECT_TRUE(result.state.empty());
  this->message_();
  EXPECT_EQ(result.state, "Setting 48: verified (raw 60)");
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
  this->source_values_[48] = 60;
  ASSERT_TRUE(this->controller_.request_setting(48, 35));
  this->group_();
  this->message_();
  this->acquire_write_(48, 35);
  this->reply_write_(35);
  this->message_();
  EXPECT_EQ(result.state, "Setting 48: verified (raw 35)");
  EXPECT_EQ(this->controller_.writes_sent(), 2u);
  this->controller_.set_last_write_result_text_sensor(nullptr);
}

TEST_F(CSPlusSettingWriteTest, OptionalWriteResultKeepsFinishedValueWhenSnapshotCallbackStartsNextRequest) {
  VBusWriteResultTextSensor result;
  this->controller_.set_last_write_result_text_sensor(&result);
  bool started = false;
  this->controller_.add_on_snapshot_callback([&]() {
    if (!started && this->controller_.writes_verified() == 1) {
      started = true;
      EXPECT_TRUE(this->controller_.request_setting(48, 35));
    }
  });
  this->preflight_();
  this->acquire_write_();
  this->reply_write_(60);
  this->message_();
  EXPECT_TRUE(started);
  EXPECT_EQ(result.state, "Setting 48: verified (raw 60)");
  this->controller_.set_last_write_result_text_sensor(nullptr);
}
#endif

TEST_F(CSPlusSettingWriteTest, PreflightThenOneWriteReadbackReleaseAndTelemetryConfirmTheChange) {
  this->preflight_();
  EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_VERIFYING);
  this->acquire_write_();
  this->reply_write_(60);
  EXPECT_TRUE(this->write_results_.empty());
  EXPECT_EQ(this->controller_.writes_verified(), 0u);
  int32_t value = 0;
  ASSERT_TRUE(this->controller_.get_value(48, value));
  EXPECT_EQ(value, 35);
  this->message_();
  ASSERT_EQ(this->write_results_.size(), 1u);
  EXPECT_EQ(this->write_results_[0], std::make_pair(uint16_t{48}, SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS));
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
  EXPECT_EQ(this->controller_.writes_verified(), 1u);
  EXPECT_TRUE(this->controller_.get_value(48, value));
  EXPECT_EQ(value, 60);
}

TEST_F(CSPlusSettingWriteTest, WritePreflightVerificationAndReleasePreserveTheLastScanDiagnostics) {
  const auto statistics = [this]() -> std::array<uint32_t, 6> {
    return {this->controller_.request_count(), this->controller_.received_count(),
            this->controller_.group_count(),   this->controller_.release_count(),
            this->controller_.elapsed_ms(),    this->controller_.maximum_lease_ms()};
  };
  const auto previous = statistics();
  EXPECT_EQ(this->controller_.scan_status(), CSPlusScanStatus::CS_PLUS_SCAN_STATUS_COMPLETE);
  this->preflight_();
  EXPECT_EQ(statistics(), previous);
  this->acquire_write_();
  this->reply_write_(60);
  this->message_();
  EXPECT_EQ(statistics(), previous);
  EXPECT_EQ(this->controller_.scan_status(), CSPlusScanStatus::CS_PLUS_SCAN_STATUS_COMPLETE);
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
  EXPECT_EQ(this->controller_.writes_verified(), 1u);
}

TEST_F(CSPlusSettingWriteTest, EqualFreshValueIsANoopAfterPreflightWithoutAnyWrite) {
  this->preflight_(48, 35);
  ASSERT_EQ(this->write_results_.size(), 1u);
  EXPECT_EQ(this->write_results_[0].second, SettingWriteStatus::SETTING_WRITE_STATUS_UNCHANGED);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
  EXPECT_FALSE(this->controller_.is_scanning());
}

TEST_F(CSPlusSettingWriteTest, FreshPreflightRejectsChangedLimitsAndDisabledFunctions) {
  for (const bool disable : {false, true}) {
    this->source_values_[50] = disable ? 65 : 55;
    this->source_values_[12549] = disable ? 0 : 2;
    this->preflight_();
    EXPECT_EQ(this->write_results_.back().second, SettingWriteStatus::SETTING_WRITE_STATUS_FAILED);
    EXPECT_EQ(this->controller_.writes_sent(), 0u);
    EXPECT_FALSE(this->controller_.write_uncertain());
  }
}

TEST_F(CSPlusSettingWriteTest, FailedPreflightReconcilesContextWithoutSuspensionNotifications) {
  unsigned lifecycle_events = 0;
  this->owned_polling_.add_on_suspended_callback([&](bool) { ++lifecycle_events; });
  this->source_values_[50] = 55;
  this->preflight_();
  ASSERT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_FAILED);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
  EXPECT_EQ(lifecycle_events, 0u);
  this->tick_(60000);
  this->message_();
  this->controller_.update();
  EXPECT_TRUE(this->controller_.is_scanning());
}

TEST_F(CSPlusSettingWriteTest, CancellationCallbacksCannotStartAWriteBeforeCleanupFinishes) {
  bool cancelled = false;
  bool accepted = false;
  bool scan_accepted = false;
  this->transactions_.add_on_result_callback([&](uint32_t, uint16_t, uint16_t, uint32_t, TransactionStatus status) {
    if (status == TransactionStatus::TRANSACTION_STATUS_CANCELLED && !cancelled) {
      cancelled = true;
      this->controller_.arm_writes(true);
      EXPECT_FALSE(this->controller_.writes_armed());
      accepted = this->controller_.request_setting(55, 3);
      scan_accepted = this->controller_.start_scan();
    }
  });
  this->tick_(60000);
  this->message_();
  ASSERT_TRUE(this->controller_.request_setting(48, 60));
  this->tick_();
  this->uart_.inject(wire_datagram());
  this->tick_();
  this->tick_(100);
  ASSERT_TRUE(this->transactions_.is_busy());
  {
    LoopBlockingGuard guard{nullptr, nullptr, this->now_ + 15000};
    std::vector<uint8_t> payload(40);
    payload[32] = 111;
    this->controller_.on_message(0x0100, 0x2211, 0x0010, payload);
    this->controller_.loop();
  }
  EXPECT_TRUE(cancelled);
  EXPECT_FALSE(accepted);
  EXPECT_FALSE(scan_accepted);
  EXPECT_FALSE(this->controller_.writes_armed());
  EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_FAILED);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
  {
    LoopBlockingGuard guard{nullptr, nullptr, this->now_ + 15001};
    EXPECT_TRUE(this->controller_.start_scan());
  }
}

TEST_F(CSPlusSettingWriteTest, MismatchedReadbackLocksFurtherWritesUntilACompleteScan) {
  this->preflight_();
  this->acquire_write_();
  this->reply_write_(40);
  ASSERT_EQ(this->write_results_.size(), 1u);
  EXPECT_EQ(this->write_results_[0].second, SettingWriteStatus::SETTING_WRITE_STATUS_UNCERTAIN);
  EXPECT_TRUE(this->controller_.write_uncertain());
  this->message_();
  EXPECT_FALSE(this->controller_.request_setting(48, 60));
  this->tick_(60000);
  this->message_();
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  EXPECT_FALSE(this->controller_.write_uncertain());
  EXPECT_TRUE(this->controller_.request_setting(48, 60));
}

TEST_F(CSPlusSettingWriteTest, UnsentWriteExpiryDiscardsTheRequestWithoutAnUncertainLockOrRetry) {
  this->preflight_();
  this->tick_(10100);
  EXPECT_EQ(this->write_results_.back().second, SettingWriteStatus::SETTING_WRITE_STATUS_FAILED);
  EXPECT_FALSE(this->controller_.write_uncertain());
  this->uart_.inject(wire_datagram());
  this->tick_(100);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
}

TEST_F(CSPlusSettingWriteTest, OtaDuringASentWriteCancelsVerificationAndKeepsTheUncertainLock) {
  this->preflight_();
  this->acquire_write_();
  this->owned_polling_.suspend();
  EXPECT_TRUE(this->owned_polling_.is_suspended());
  EXPECT_TRUE(this->controller_.write_uncertain());
  const size_t before = this->uart_.writes.size();
  this->tick_(20);
  this->uart_.inject(wire_datagram(0x0100, 0x2211, 0x0020, 48, 60));
  this->tick_(1000);
  EXPECT_EQ(this->uart_.writes.size(), before);
  EXPECT_EQ(this->write_results_.size(), 1u);
}

TEST_F(CSPlusSettingWriteTest, ExplicitArmingExpiresAndIsConsumedByOnlyOneAcceptedRequest) {
  this->controller_.set_require_write_arm(true);
  EXPECT_FALSE(this->controller_.request_setting(48, 60));
  this->controller_.arm_writes(true);
  EXPECT_TRUE(this->controller_.writes_armed());
  EXPECT_FALSE(this->controller_.request_setting(48, NAN));
  EXPECT_TRUE(this->controller_.writes_armed());
  this->tick_(60000);
  this->message_();
  EXPECT_FALSE(this->controller_.writes_armed());
  EXPECT_FALSE(this->controller_.request_setting(48, 60));
  this->controller_.arm_writes(true);
  EXPECT_TRUE(this->controller_.request_setting(48, 60));
  EXPECT_FALSE(this->controller_.writes_armed());
  EXPECT_FALSE(this->controller_.request_setting(48, 60));
}

TEST_F(CSPlusSettingWriteTest, InvalidRawShapesRangesUnknownIdsAndExpiredSnapshotsCannotWrite) {
  EXPECT_FALSE(this->controller_.request_setting(0, 1));
  EXPECT_FALSE(this->controller_.request_setting(48, 61));
  EXPECT_FALSE(this->controller_.request_setting(48, 200));
  EXPECT_FALSE(this->controller_.request_setting(48, INFINITY));
  EXPECT_FALSE(this->controller_.request_setting(55, 0));
  this->tick_(660001);
  this->message_();
  EXPECT_FALSE(this->controller_.request_setting(48, 60));
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
}

TEST_F(CSPlusSettingWriteTest, EnumWritesInvalidateContextOnlyAfterVerifiedReleaseAndTelemetry) {
  this->preflight_(55, 4);
  this->acquire_write_(55, 4);
  this->reply_write_(4, 55);
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
  this->message_();
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  EXPECT_EQ(this->controller_.writes_verified(), 1u);
  EXPECT_FALSE(this->controller_.request_setting(48, 60));
}

TEST_F(CSPlusSettingWriteTest, MissingWriteAckIsUncertainAndNeverRetried) {
  this->preflight_();
  this->acquire_write_();
  this->tick_(500);
  EXPECT_TRUE(this->controller_.write_uncertain());
  EXPECT_EQ(this->write_results_.back().second, SettingWriteStatus::SETTING_WRITE_STATUS_UNCERTAIN);
  this->message_();
  this->tick_(1000);
  this->uart_.inject(wire_datagram());
  this->tick_(100);
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
  EXPECT_EQ(this->controller_.writes_verified(), 0u);
}

TEST_F(CSPlusSettingWriteTest, OtaBeforeAnyWriteDisarmsAndDiscardsPreflight) {
  this->controller_.set_require_write_arm(true);
  this->controller_.arm_writes(true);
  ASSERT_TRUE(this->controller_.request_setting(48, 60));
  this->owned_polling_.suspend();
  this->owned_polling_.resume();
  EXPECT_FALSE(this->controller_.writes_armed());
  EXPECT_FALSE(this->controller_.write_uncertain());
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
  EXPECT_EQ(this->write_results_.back().second, SettingWriteStatus::SETTING_WRITE_STATUS_FAILED);
  this->uart_.inject(wire_datagram());
  this->tick_(100);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
}

TEST_F(CSPlusSettingWriteTest, EnumContextRefreshBypassesThePeriodicLimitOnceAndRetainsConfirmedData) {
  this->preflight_(55, 4);
  this->acquire_write_(55, 4);
  this->reply_write_(4, 55);
  this->message_();
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  EXPECT_TRUE(this->controller_.has_recent_snapshot());
  this->source_values_[55] = 4;
  this->tick_();
  ASSERT_TRUE(this->controller_.is_scanning());
  this->complete_();
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
  EXPECT_TRUE(this->controller_.has_recent_snapshot());
  EXPECT_FALSE(this->controller_.start_scan());
}

TEST_F(CSPlusSettingWriteTest, UncertainWriteRequestsAReadOnlyRefreshWithoutWaitingAMinute) {
  this->preflight_();
  this->acquire_write_();
  this->reply_write_(40);
  EXPECT_TRUE(this->controller_.write_uncertain());
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  EXPECT_TRUE(this->controller_.has_recent_snapshot());
  this->source_values_[48] = 40;
  this->tick_();
  ASSERT_TRUE(this->controller_.is_scanning());
  this->complete_();
  int32_t value = 0;
  EXPECT_TRUE(this->controller_.get_value(48, value));
  EXPECT_EQ(value, 40);
  EXPECT_FALSE(this->controller_.write_uncertain());
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
}

TEST_F(CSPlusSettingWriteTest, RetainedContextExpiresAndCannotResurrectAfterIncompatibleFirmware) {
  this->preflight_(55, 4);
  this->acquire_write_(55, 4);
  this->reply_write_(4, 55);
  this->message_();
  EXPECT_TRUE(this->controller_.has_recent_snapshot());
  this->tick_(660001);
  EXPECT_FALSE(this->controller_.has_recent_snapshot());
  this->message_(112);
  this->message_();
  EXPECT_FALSE(this->controller_.has_recent_snapshot());
}

TEST_F(CSPlusSettingWriteTest, IncompatibleFirmwareDiscardsRetainedContextBeforeItsExpiry) {
  this->preflight_(55, 4);
  this->acquire_write_(55, 4);
  this->reply_write_(4, 55);
  this->message_();
  ASSERT_TRUE(this->controller_.has_recent_snapshot());
  this->message_(112);
  EXPECT_FALSE(this->controller_.has_recent_snapshot());
  this->message_();
  EXPECT_FALSE(this->controller_.has_recent_snapshot());
}

#ifdef USE_VBUS_SETTING_TIME
TEST_F(CSPlusSettingWriteTest, OptionalClockRejectsInvalidOrIncompleteCallsWithoutUsingCachedFields) {
  VBusSettingTime clock{&this->controller_, 2};
  clock.on_setting_snapshot();
  ASSERT_TRUE(clock.has_state());
  for (const auto value : std::array<std::array<uint8_t, 3>, 4>{{{25, 30, 0}, {7, 60, 0}, {7, 30, 60}, {7, 30, 1}}}) {
    clock.make_call().set_time(value[0], value[1], value[2]).perform();
    EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_IDLE);
  }
  clock.make_call().set_hour(7).perform();
  clock.make_call().set_minute(30).perform();
  clock.make_call().set_hour(7).set_minute(30).perform();
  EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_IDLE);
  EXPECT_EQ(clock.hour, 1);
  EXPECT_EQ(clock.minute, 2);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
}

TEST_F(CSPlusSettingWriteTest, OptionalClockUsesConfirmedTimeAndSkipsUnchangedSnapshots) {
  VBusSettingTime clock{&this->controller_, 2};
  unsigned updates = 0;
  clock.add_on_state_callback([&]() { ++updates; });
  clock.on_setting_snapshot();
  ASSERT_TRUE(clock.has_state());
  EXPECT_EQ(clock.hour, 1);
  EXPECT_EQ(clock.minute, 2);
  EXPECT_EQ(clock.second, 0);
  clock.on_setting_snapshot();
  EXPECT_EQ(updates, 1u);
  const auto sent = this->controller_.writes_sent();
  clock.make_call().set_time(1, 3, 1).perform();
  EXPECT_EQ(this->controller_.writes_sent(), sent);
  EXPECT_EQ(clock.minute, 2);
  this->message_(112);
  clock.on_setting_snapshot();
  EXPECT_FALSE(clock.has_state());
}

TEST_F(CSPlusSettingWriteTest, OptionalClockWritesThroughPreflightAndWaitsForTelemetry) {
  VBusSettingTime clock{&this->controller_, 2};
  clock.on_setting_snapshot();
  clock.make_call().set_time(1, 3, 0).perform();
  EXPECT_EQ(clock.minute, 2);
  this->group_();
  this->message_();
  this->acquire_write_(2, 63);
  this->reply_write_(63, 2);
  std::vector<uint8_t> payload(40);
  payload[22] = 63;
  payload[32] = 111;
  this->controller_.on_message(0x0100, 0x2211, 0x0010, payload);
  this->tick_();
  clock.on_setting_snapshot();
  EXPECT_EQ(clock.hour, 1);
  EXPECT_EQ(clock.minute, 3);
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
}

TEST_F(CSPlusSettingWriteTest, OptionalScheduledClockRejectsOffStepTimesWithoutRounding) {
  this->source_values_[1] = 3;
  this->tick_(60000);
  this->message_();
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  VBusSettingTime clock{&this->controller_, 3587};
  clock.on_setting_snapshot();
  ASSERT_TRUE(clock.has_state());
  clock.make_call().set_time(0, 7, 0).perform();
  EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED);
  EXPECT_EQ(clock.minute, 0);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
  clock.make_call().set_time(0, 15, 0).perform();
  EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_PREFLIGHT);
}
#endif

#ifdef USE_VBUS_CS_PLUS_ENTITIES
class CSPlusNumberTest : public CSPlusSettingWriteTest {
 protected:
  void SetUp() override {
    CSPlusSettingWriteTest::SetUp();
    this->minimum_.add_on_state_callback([this](float) { ++this->number_updates_; });
    this->controller_.register_setting_listener(&this->minimum_);
    this->controller_.register_setting_listener(&this->delta_);
    this->tick_();
    this->tick_();
  }
  void refresh_() {
    this->tick_(60000);
    this->message_();
    ASSERT_TRUE(this->controller_.start_scan());
    this->complete_();
  }
  VBusSettingNumber minimum_{&this->controller_, 48};
  VBusSettingNumber delta_{&this->controller_, 385};
  VBusSettingNumber clock_{&this->controller_, 2};
  size_t number_updates_{0};
};

TEST_F(CSPlusNumberTest, FailedRefreshDoesNotPublishMissingDependenciesAsZero) {
  const float before = this->controller_.setting_value(385);
  this->tick_(60000);
  this->message_();
  ASSERT_TRUE(this->controller_.start_scan());
  this->group_();
  this->owned_polling_.suspend();
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
  EXPECT_FLOAT_EQ(this->controller_.setting_value(385), before);
}

TEST_F(CSPlusNumberTest, InconsistentConfirmedRelationsRemainDistinctFromMissingOrInapplicableValues) {
  this->source_values_[385] = 40;
  this->source_values_[401] = 40;
  this->refresh_();
  int32_t value = -1;
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
  ASSERT_TRUE(this->controller_.get_value(385, value));
  EXPECT_EQ(value, 40);
  EXPECT_TRUE(std::isnan(this->controller_.setting_value(385)));
  EXPECT_FLOAT_EQ(this->controller_.setting_value(48), 35);
}

TEST_F(CSPlusNumberTest, InapplicableDrainbackFlagDoesNotHideValidSettings) {
  this->source_values_[1] = 4;
  this->source_values_[448] = 950;
  this->refresh_();
  int32_t value = -1;
  EXPECT_FALSE(this->controller_.get_value(2768, value));
  float minimum, maximum, step;
  ASSERT_TRUE(this->controller_.setting_range(448, minimum, maximum, step));
  EXPECT_FLOAT_EQ(this->controller_.setting_value(448), 95);
  EXPECT_TRUE(std::isnan(this->controller_.setting_value(2768)));
}

TEST_F(CSPlusNumberTest, ConfirmedStateUsesScalingAndTheCurrentRelationalRange) {
  ASSERT_TRUE(this->minimum_.has_state());
  EXPECT_FLOAT_EQ(this->minimum_.state, 35);
  EXPECT_FLOAT_EQ(this->minimum_.traits.get_min_value(), 10);
  EXPECT_FLOAT_EQ(this->minimum_.traits.get_max_value(), 60);
  EXPECT_FLOAT_EQ(this->minimum_.traits.get_step(), 5);
  EXPECT_FLOAT_EQ(this->delta_.state, 10);
  EXPECT_FLOAT_EQ(this->delta_.traits.get_min_value(), 4.5);
  EXPECT_FLOAT_EQ(this->delta_.traits.get_max_value(), 11.5);
  EXPECT_FLOAT_EQ(this->delta_.traits.get_step(), 0.5);
}

TEST_F(CSPlusNumberTest, ControlDoesNotPublishUntilReadbackReleaseAndResumedTelemetry) {
  auto call = this->minimum_.make_call();
  call.set_value(60);
  call.perform();
  EXPECT_FLOAT_EQ(this->minimum_.state, 35);
  this->group_();
  this->message_();
  this->acquire_write_();
  this->reply_write_(60);
  EXPECT_FLOAT_EQ(this->minimum_.state, 35);
  EXPECT_EQ(this->number_updates_, 1u);
  this->message_();
  this->tick_();
  this->tick_();
  EXPECT_FLOAT_EQ(this->minimum_.state, 60);
  EXPECT_EQ(this->number_updates_, 2u);
  EXPECT_EQ(this->controller_.writes_verified(), 1u);
}

TEST_F(CSPlusNumberTest, UnchangedRefreshPreservesAvailabilityStateAndMetadataRevision) {
  const uint32_t revision = this->controller_.metadata_revision();
  this->refresh_();
  this->tick_();
  this->tick_();
  EXPECT_TRUE(this->minimum_.has_state());
  EXPECT_EQ(this->number_updates_, 1u);
  EXPECT_EQ(this->controller_.metadata_revision(), revision);
}

TEST_F(CSPlusNumberTest, ContextRefreshAfterAnEnumWriteKeepsConfirmedNumbersAvailableWithoutRepublishing) {
  const uint32_t revision = this->controller_.metadata_revision();
  this->preflight_(55, 4);
  this->acquire_write_(55, 4);
  this->reply_write_(4, 55);
  this->message_();
  ASSERT_FALSE(this->controller_.has_fresh_snapshot());
  this->tick_();
  this->tick_();
  EXPECT_TRUE(this->minimum_.has_state());
  EXPECT_FLOAT_EQ(this->minimum_.state, 35);
  EXPECT_EQ(this->number_updates_, 1u);
  this->source_values_[55] = 4;
  this->controller_.update();
  this->complete_();
  this->tick_();
  this->tick_();
  EXPECT_TRUE(this->minimum_.has_state());
  EXPECT_EQ(this->number_updates_, 1u);
  EXPECT_EQ(this->controller_.metadata_revision(), revision);
}

TEST_F(CSPlusNumberTest, FailedContextRefreshDoesNotDisablePendingAvailabilityNotifications) {
  this->preflight_(55, 4);
  this->acquire_write_(55, 4);
  this->reply_write_(4, 55);
  this->message_();
  this->message_(112);
  EXPECT_FALSE(this->minimum_.has_state());
  EXPECT_TRUE(this->delta_.has_state());
  EXPECT_FALSE(this->controller_.is_idle());
  this->tick_();
  EXPECT_FALSE(this->delta_.has_state());
  EXPECT_TRUE(this->controller_.is_idle());
}

TEST_F(CSPlusNumberTest, MetadataChangesOnceAfterAllBoundedEntityNotificationsComplete) {
  const uint32_t revision = this->controller_.metadata_revision();
  this->source_values_[50] = 70;
  this->refresh_();
  EXPECT_FLOAT_EQ(this->minimum_.traits.get_max_value(), 60);
  this->tick_();
  EXPECT_FLOAT_EQ(this->minimum_.traits.get_max_value(), 65);
  EXPECT_EQ(this->controller_.metadata_revision(), revision);
  this->tick_();
  EXPECT_EQ(this->controller_.metadata_revision(), revision + 1);
  EXPECT_EQ(this->number_updates_, 1u);
}

TEST_F(CSPlusNumberTest, InapplicabilityAndRecoveryChangeAvailabilityWithoutDiscardingAdvertisedTraits) {
  const uint32_t revision = this->controller_.metadata_revision();
  this->source_values_[12549] = 0;
  this->refresh_();
  this->tick_();
  this->tick_();
  EXPECT_FALSE(this->minimum_.has_state());
  EXPECT_FLOAT_EQ(this->minimum_.traits.get_max_value(), 60);
  this->source_values_[12549] = 2;
  this->refresh_();
  this->tick_();
  this->tick_();
  EXPECT_TRUE(this->minimum_.has_state());
  EXPECT_FLOAT_EQ(this->minimum_.state, 35);
  EXPECT_EQ(this->number_updates_, 2u);
  EXPECT_EQ(this->controller_.metadata_revision(), revision);
}

TEST_F(CSPlusNumberTest, ExpiredSnapshotsInvalidateTheValueButRetainTheLastAdvertisedRange) {
  this->tick_(660001);
  this->minimum_.on_setting_snapshot();
  this->minimum_.on_setting_snapshot();
  EXPECT_FALSE(this->minimum_.has_state());
  EXPECT_FLOAT_EQ(this->minimum_.traits.get_max_value(), 60);
  EXPECT_EQ(this->number_updates_, 1u);
}

TEST_F(CSPlusNumberTest, ClockUsesChangedValidBroadcastsWithoutQueriesOrSnapshotCallbacks) {
  this->controller_.register_setting_listener(&this->clock_);
  const size_t frames = this->uart_.writes.size();
  const size_t snapshots = this->snapshots_;
  std::vector<uint8_t> payload(40);
  payload[32] = 111;
  payload[22] = 120;
  this->controller_.on_message(0x0100, 0x2211, 0x0010, payload);
  this->tick_();
  this->tick_();
  this->tick_();
  EXPECT_FLOAT_EQ(this->clock_.state, 120);
  payload[22] = 0xff;
  payload[23] = 0xff;
  this->controller_.on_message(0x0100, 0x2211, 0x0010, payload);
  this->tick_();
  EXPECT_FLOAT_EQ(this->clock_.state, 120);
  EXPECT_EQ(this->snapshots_, snapshots);
  EXPECT_EQ(this->uart_.writes.size(), frames);
}

class CSPlusSelectTest : public CSPlusNumberTest {
 protected:
  void SetUp() override {
    CSPlusNumberTest::SetUp();
    this->language_.traits.set_options_static(LANGUAGES, 5);
    this->language_.initialize_options();
    this->flow_.traits.set_options_static(FLOW_TYPES, 4);
    this->flow_.initialize_options();
    this->language_.add_on_state_callback([this](size_t) { ++this->language_updates_; });
    this->flow_.add_on_state_callback([this](size_t) { ++this->flow_updates_; });
    this->controller_.register_setting_listener(&this->language_);
    this->controller_.register_setting_listener(&this->flow_);
    this->flush_();
  }
  void flush_() {
    for (size_t i = 0; i < 6; ++i)
      this->tick_();
  }
  static constexpr const char *LANGUAGES[] = {"German", "English", "French", "Italian", "Spanish"};
  static constexpr const char *FLOW_TYPES[] = {"Off", "12", "40", "40F"};
  VBusSettingSelect language_{&this->controller_, 55, 1};
  VBusSettingSelect flow_{&this->controller_, 74, 0};
  size_t language_updates_{0};
  size_t flow_updates_{0};
};

TEST_F(CSPlusSelectTest, InapplicableDrainbackAndFailedRefreshPreserveValidOptions) {
  this->source_values_[1] = 4;
  this->refresh_();
  EXPECT_EQ(this->controller_.setting_option_mask(461), 3u);
  EXPECT_EQ(this->controller_.setting_option_mask(2768), 0u);
  this->tick_(60000);
  this->message_();
  ASSERT_TRUE(this->controller_.start_scan());
  this->group_();
  this->owned_polling_.suspend();
  EXPECT_EQ(this->controller_.setting_option_mask(461), 3u);
}

TEST_F(CSPlusSelectTest, InvalidPublicIndexAndUnknownLabelCannotReachTheWriteController) {
  auto bad_index = this->language_.make_call();
  bad_index.set_index(this->language_.size());
  bad_index.perform();
  auto bad_label = this->language_.make_call();
  bad_label.set_option("Unknown language");
  bad_label.perform();
  this->flush_();
  EXPECT_EQ(this->language_.current_option(), "German");
  EXPECT_EQ(this->language_updates_, 1u);
  EXPECT_TRUE(this->write_results_.empty());
  EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_IDLE);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
}

TEST_F(CSPlusSelectTest, ConfirmedInitialValuesAndOnlyCurrentlyAllowedOptionsAreAdvertised) {
  EXPECT_TRUE(this->language_.has_state());
  EXPECT_EQ(this->language_.current_option(), "German");
  EXPECT_EQ(this->language_.size(), 5u);
  EXPECT_TRUE(this->flow_.has_state());
  EXPECT_EQ(this->flow_.current_option(), "Off");
  EXPECT_EQ(this->flow_.size(), 3u);
  EXPECT_FALSE(this->flow_.has_option("40F"));
  EXPECT_EQ(this->language_updates_, 1u);
  EXPECT_EQ(this->flow_updates_, 1u);
}

TEST_F(CSPlusSelectTest, LanguageChangesOnlyAfterVerificationAndFreshContextWithoutNumericAvailabilityChurn) {
  auto call = this->language_.make_call();
  call.set_option("French");
  call.perform();
  EXPECT_EQ(this->language_.current_option(), "German");
  this->group_();
  this->message_();
  this->acquire_write_(55, 3);
  this->reply_write_(3, 55);
  EXPECT_EQ(this->language_.current_option(), "German");
  this->message_();
  this->flush_();
  EXPECT_EQ(this->language_.current_option(), "German");
  EXPECT_TRUE(this->minimum_.has_state());
  this->source_values_[55] = 3;
  this->controller_.update();
  this->complete_();
  this->flush_();
  EXPECT_EQ(this->language_.current_option(), "French");
  EXPECT_EQ(this->language_updates_, 2u);
  EXPECT_EQ(this->number_updates_, 1u);
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
}

TEST_F(CSPlusSelectTest, EqualRefreshesDoNotRepublishOrChangeMetadata) {
  const uint32_t revision = this->controller_.metadata_revision();
  this->refresh_();
  this->flush_();
  EXPECT_TRUE(this->language_.has_state());
  EXPECT_TRUE(this->flow_.has_state());
  EXPECT_EQ(this->language_updates_, 1u);
  EXPECT_EQ(this->flow_updates_, 1u);
  EXPECT_EQ(this->controller_.metadata_revision(), revision);
}

TEST_F(CSPlusSelectTest, OptionChangesReuseAllocatedStorageAndDoNotRepublishTheSameConfirmedLabel) {
  const auto *storage = this->flow_.traits.get_options().data();
  const uint32_t revision = this->controller_.metadata_revision();
  this->source_values_[90] = 0;
  this->refresh_();
  this->flush_();
  EXPECT_EQ(this->flow_.traits.get_options().data(), storage);
  EXPECT_EQ(this->flow_.size(), 4u);
  EXPECT_TRUE(this->flow_.has_option("40F"));
  EXPECT_EQ(this->flow_.current_option(), "Off");
  EXPECT_EQ(this->flow_updates_, 1u);
  EXPECT_EQ(this->controller_.metadata_revision(), revision + 1);
  this->source_values_[90] = 1;
  this->refresh_();
  this->flush_();
  EXPECT_EQ(this->flow_.traits.get_options().data(), storage);
  EXPECT_EQ(this->flow_.size(), 3u);
  EXPECT_EQ(this->flow_updates_, 1u);
}

TEST_F(CSPlusSelectTest, ReindexedOptionsKeepTheConfirmedLabelWithoutPublishingAnotherState) {
  VBusSettingSelect priority{&this->controller_, 21, 0};
  static constexpr const char *LABELS[] = {"0", "1", "2", "3", "4", "5", "6"};
  priority.traits.set_options_static(LABELS, 7);
  priority.initialize_options();
  this->controller_.register_setting_listener(&priority);
  size_t updates = 0;
  priority.add_on_state_callback([&updates](size_t) { ++updates; });
  this->source_values_[1] = 4;
  this->refresh_();
  this->flush_();
  ASSERT_TRUE(priority.has_state());
  EXPECT_EQ(priority.current_option(), "6");
  EXPECT_EQ(priority.size(), 5u);
  EXPECT_EQ(priority.active_index(), 4u);
  this->source_values_[1] = 6;
  this->refresh_();
  this->flush_();
  EXPECT_EQ(priority.current_option(), "6");
  EXPECT_EQ(priority.active_index(), 6u);
  EXPECT_EQ(updates, 1u);
}

TEST_F(CSPlusSelectTest, ExpiryInvalidatesOnceAndRetainsAdvertisedOptions) {
  const auto size = this->language_.size();
  this->tick_(660001);
  this->language_.on_setting_snapshot();
  this->language_.on_setting_snapshot();
  EXPECT_FALSE(this->language_.has_state());
  EXPECT_EQ(this->language_.size(), size);
  EXPECT_EQ(this->language_updates_, 1u);
}

TEST_F(CSPlusSelectTest, UnsupportedVisibleOptionCannotStartAWrite) {
  auto call = this->flow_.make_call();
  call.set_option("40F");
  call.perform();
  this->flush_();
  EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_IDLE);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
  EXPECT_EQ(this->flow_.current_option(), "Off");
}

TEST_F(CSPlusSelectTest, UncertainWriteKeepsConfirmedStatesUntilTheReadOnlyRefreshCompletes) {
  this->preflight_(55, 3);
  this->acquire_write_(55, 3);
  this->reply_write_(4, 55);
  this->flush_();
  EXPECT_TRUE(this->controller_.write_uncertain());
  EXPECT_EQ(this->language_.current_option(), "German");
  EXPECT_TRUE(this->minimum_.has_state());
  this->source_values_[55] = 4;
  this->controller_.update();
  this->complete_();
  this->flush_();
  EXPECT_EQ(this->language_.current_option(), "Italian");
  EXPECT_EQ(this->language_updates_, 2u);
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
  EXPECT_FALSE(this->controller_.write_uncertain());
}

TEST_F(CSPlusSelectTest, FunctionInapplicabilityAndRecoveryRetainOptionsAndPublishOneRecoveredState) {
  VBusSettingSelect holiday{&this->controller_, 64, 0};
  static constexpr const char *LABELS[] = {"Off", "On"};
  holiday.traits.set_options_static(LABELS, 2);
  holiday.initialize_options();
  this->controller_.register_setting_listener(&holiday);
  size_t updates = 0;
  holiday.add_on_state_callback([&updates](size_t) { ++updates; });
  this->flush_();
  EXPECT_FALSE(holiday.has_state());
  this->source_values_[71] = 1;
  this->refresh_();
  this->flush_();
  ASSERT_TRUE(holiday.has_state());
  EXPECT_EQ(holiday.current_option(), "Off");
  this->source_values_[2768] = 1;
  this->refresh_();
  this->flush_();
  EXPECT_FALSE(holiday.has_state());
  EXPECT_EQ(holiday.size(), 2u);
  this->source_values_[2768] = 0;
  this->refresh_();
  this->flush_();
  EXPECT_TRUE(holiday.has_state());
  EXPECT_EQ(holiday.current_option(), "Off");
  EXPECT_EQ(updates, 2u);
}
#endif
#endif
#endif

}  // namespace
#ifdef USE_VBUS_CS_PLUS_ADAPTIVE
class CSPlusAdaptiveTest : public CSPlusControllerTest {
 protected:
  void SetUp() override {
    CSPlusControllerTest::SetUp();
    this->controller_.set_context_update_interval(60000);
    this->controller_.set_update_interval(300000);
  }
  void initial_() {
    ASSERT_TRUE(this->controller_.start_scan());
    this->complete_();
    this->requested_.clear();
  }
  void due_(uint32_t elapsed = 60000) {
    this->tick_(elapsed);
    this->message_();
    this->controller_.update();
    this->tick_();
  }
};

TEST_F(CSPlusAdaptiveTest, LongFullIntervalRetainsAgesBeyondSixteenBitSeconds) {
  this->controller_.set_update_interval(86400000);
  this->initial_();
  this->now_ += 70000000;
  this->message_();
  EXPECT_TRUE(this->controller_.has_recent_snapshot());
  EXPECT_GT(this->controller_.value_age_seconds(48), 65535);
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
}

TEST_F(CSPlusAdaptiveTest, InapplicableDrainbackDoesNotRefreshUnchangedNumbers) {
  for (const int scheme : {4, 5, 6, 7, 10}) {
    SCOPED_TRACE(scheme);
    this->source_values_[1] = scheme;
    this->source_values_[448] = 800;
    if (scheme != 4) {
      this->tick_(60000);
      this->message_();
    }
    ASSERT_TRUE(this->controller_.start_scan());
    this->complete_();
    ASSERT_GT(std::count(this->requested_.begin(), this->requested_.end(), 448), 0);
    this->requested_.clear();
    this->due_();
    this->complete_();
    EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 2768), 0);
    EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 448), 0);
    this->requested_.clear();
  }
}

TEST_F(CSPlusAdaptiveTest, UnchangedContextSkipsNumbersAndDoesNotRenewTheirAge) {
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  const size_t full_reads = this->requested_.size();
  this->requested_.clear();
  this->due_();
  ASSERT_TRUE(this->controller_.is_scanning());
  this->complete_();
  EXPECT_LT(this->requested_.size(), full_reads);
  RecordProperty("full_scan_reads", full_reads);
  RecordProperty("context_scan_reads", this->requested_.size());
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 48), 0);
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 0);
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 2), 0);
  EXPECT_GT(this->controller_.value_age_seconds(48), 60);
  EXPECT_LT(this->controller_.value_age_seconds(1), 1);
  EXPECT_GT(this->controller_.snapshot_age_seconds(), 60);
  int32_t value = 0;
  ASSERT_TRUE(this->controller_.get_value(48, value));
  EXPECT_EQ(value, 35);
}

TEST_F(CSPlusAdaptiveTest, PanelFlagChangeReadsItsChildrenWithoutReadingUnrelatedValues) {
  this->initial_();
  this->source_values_[461] = 1;
  this->due_();
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 455), 1);
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 448), 1);
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 48), 0);
  EXPECT_EQ(this->requested_.size(), std::set<uint16_t>(this->requested_.begin(), this->requested_.end()).size());
}

TEST_F(CSPlusAdaptiveTest, UnitChangePromotesTheContextScanToAFullSnapshot) {
  this->initial_();
  this->source_values_[10] = 1;
  this->due_();
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 1);
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 48), 1);
  EXPECT_LT(this->controller_.snapshot_age_seconds(), 1);
}

TEST_F(CSPlusAdaptiveTest, SchemeChangePromotesTheContextScanToAFullSnapshot) {
  this->initial_();
  this->source_values_[1] = 2;
  this->due_();
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 1);
  int32_t value = 0;
  ASSERT_TRUE(this->controller_.get_value(1, value));
  EXPECT_EQ(value, 2);
}

TEST_F(CSPlusAdaptiveTest, FrequentContextScansDoNotPostponeTheFullRefresh) {
  this->initial_();
  for (int i = 0; i < 4; ++i) {
    this->due_();
    this->complete_();
    EXPECT_GT(this->controller_.snapshot_age_seconds(), 60);
  }
  this->requested_.clear();
  this->due_();
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 1);
  EXPECT_LT(this->controller_.snapshot_age_seconds(), 1);
}

TEST_F(CSPlusAdaptiveTest, ExpiredContextBlocksWritesButRetainsConfirmedValues) {
  this->initial_();
  this->tick_(180001);
  this->message_();
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  EXPECT_TRUE(this->controller_.has_recent_snapshot());
#ifdef USE_VBUS_CS_PLUS_WRITE
  this->controller_.set_require_write_arm(false);
  EXPECT_FALSE(this->controller_.request_setting(55, 4));
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
#endif
  this->controller_.update();
  this->complete_();
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
}

TEST_F(CSPlusAdaptiveTest, FailedPartialScanDoesNotPublishWorkingValuesAndRecoversWithFullScan) {
  this->initial_();
  const uint32_t scans = this->controller_.completed_scans();
  this->source_values_[461] = 1;
  this->due_();
  this->group_();
  this->message_();
  this->group_();
  this->tick_(15000);  // No resumed telemetry: the partial snapshot is not committed.
  EXPECT_FALSE(this->controller_.is_scanning());
  EXPECT_EQ(this->controller_.completed_scans(), scans);
  int32_t value = -1;
  ASSERT_TRUE(this->controller_.get_value(461, value));
  EXPECT_EQ(value, 0);
  this->requested_.clear();
  this->due_();
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 1);
}

TEST_F(CSPlusAdaptiveTest, ManualRefreshIsAlwaysFullAndOtaResumeDoesNotUsePartialCache) {
  this->initial_();
  this->due_();
  this->complete_();
  this->requested_.clear();
  this->tick_(60000);
  this->message_();
  ASSERT_TRUE(this->controller_.start_scan());
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 1);
  this->requested_.clear();
  this->owned_polling_.suspend();
  this->owned_polling_.resume();
  this->due_();
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 1);
}

TEST_F(CSPlusAdaptiveTest, WrongFirmwareInvalidatesCacheDuringAPartialScan) {
  this->initial_();
  this->due_();
  this->group_();
  this->message_(112);
  EXPECT_FALSE(this->controller_.is_scanning());
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  EXPECT_FALSE(this->controller_.has_recent_snapshot());
}

TEST_F(CSPlusAdaptiveTest, MalformedContextDoesNotCommitAndLeavesTheConfirmedSnapshotIntact) {
  this->initial_();
  const uint32_t scans = this->controller_.completed_scans();
  this->source_values_[1] = 0;
  this->due_();
  this->group_();
  EXPECT_FALSE(this->controller_.is_scanning());
  EXPECT_EQ(this->controller_.completed_scans(), scans);
  int32_t value = 0;
  ASSERT_TRUE(this->controller_.get_value(1, value));
  EXPECT_EQ(value, 1);
}

TEST_F(CSPlusAdaptiveTest, FullCacheLifetimeFollowsTheFullIntervalRatherThanContextChecks) {
  this->controller_.set_update_interval(900000);
  this->initial_();
  this->tick_(660001);
  this->message_();
  EXPECT_TRUE(this->controller_.has_recent_snapshot());
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  this->tick_(1200000);
  this->message_();
  EXPECT_FALSE(this->controller_.has_recent_snapshot());
}

#ifdef USE_VBUS_CS_PLUS_ENTITIES
TEST_F(CSPlusNumberTest, AdaptiveContextExpiryAndRecoveryKeepConfirmedEntityStateAndMetadata) {
  this->controller_.set_context_update_interval(60000);
  this->refresh_();
  this->tick_();
  this->tick_();
  const uint32_t revision = this->controller_.metadata_revision();
  const size_t updates = this->number_updates_;
  this->tick_(180001);
  this->message_();
  EXPECT_FALSE(this->controller_.has_fresh_snapshot());
  this->minimum_.on_setting_snapshot();
  EXPECT_TRUE(this->minimum_.has_state());
  EXPECT_FLOAT_EQ(this->minimum_.state, 35);
  EXPECT_EQ(this->number_updates_, updates);
  this->controller_.update();
  this->complete_();
  this->tick_();
  this->tick_();
  EXPECT_TRUE(this->minimum_.has_state());
  EXPECT_EQ(this->number_updates_, updates);
  EXPECT_EQ(this->controller_.metadata_revision(), revision);
}
#endif

TEST_F(CSPlusAdaptiveTest, MillisecondWrapDoesNotTriggerPrematureOrMissedRefresh) {
  this->now_ = UINT32_MAX - 30000;
  this->tick_();
  this->message_();
  this->initial_();
  this->due_();
  ASSERT_TRUE(this->controller_.is_scanning());
  this->complete_();
  EXPECT_GT(this->controller_.value_age_seconds(48), 60);
  EXPECT_LT(this->controller_.snapshot_age_seconds(), 70);
}

#ifdef USE_VBUS_CS_PLUS_WRITE
class CSPlusAdaptiveWriteTest : public CSPlusSettingWriteTest {
 protected:
  void SetUp() override {
    CSPlusSettingWriteTest::SetUp();
    this->controller_.set_context_update_interval(60000);
    this->tick_(60000);
    this->message_();
    ASSERT_TRUE(this->controller_.start_scan());
    this->complete_();
    this->requested_.clear();
  }
  void write_(uint16_t index, float value, uint32_t raw) {
    this->preflight_(index, value);
    this->acquire_write_(index, raw);
    this->reply_write_(raw, index);
    this->source_values_[index] = raw;
    this->message_();
    this->requested_.clear();
    this->tick_();
  }
};

TEST_F(CSPlusAdaptiveWriteTest, LanguageOnlyNeedsVerifiedReadback) {
  this->write_(55, 4, 4);
  EXPECT_FALSE(this->controller_.is_scanning());
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
  int32_t value = 0;
  ASSERT_TRUE(this->controller_.get_value(55, value));
  EXPECT_EQ(value, 4);
  EXPECT_TRUE(this->requested_.empty());
}

TEST_F(CSPlusAdaptiveWriteTest, NumericChangeRefreshesDependentLimitsOnly) {
  this->write_(48, 60, 60);
  ASSERT_TRUE(this->controller_.is_scanning());
  EXPECT_TRUE(this->controller_.has_recent_snapshot());
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 50), 1);
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 0);
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
}

TEST_F(CSPlusAdaptiveWriteTest, FeatureChangeRefreshesNewlyVisibleChildren) {
  this->write_(461, 1, 1);
  ASSERT_TRUE(this->controller_.is_scanning());
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 455), 1);
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 0);
}

TEST_F(CSPlusAdaptiveWriteTest, NoOpDoesNotStartAnUnnecessaryRefresh) {
  this->preflight_(48, 35);
  EXPECT_EQ(this->controller_.write_status(), SettingWriteStatus::SETTING_WRITE_STATUS_UNCHANGED);
  EXPECT_FALSE(this->controller_.is_scanning());
  EXPECT_TRUE(this->controller_.has_fresh_snapshot());
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
}

TEST_F(CSPlusAdaptiveWriteTest, ChangedPreflightUnitForcesFullRefreshEvenForANoOp) {
  this->source_values_[10] = 1;
  this->preflight_(55, 1);
  this->requested_.clear();
  this->tick_();
  ASSERT_TRUE(this->controller_.is_scanning());
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 48), 1);
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 1);
  EXPECT_EQ(this->controller_.writes_sent(), 0u);
}

TEST_F(CSPlusAdaptiveWriteTest, UncertainWriteForcesFullRecoveryAndNeverRetriesTheWrite) {
  this->preflight_();
  this->acquire_write_();
  this->reply_write_(40);
  this->source_values_[48] = 40;
  this->requested_.clear();
  this->tick_();
  this->message_();
  ASSERT_TRUE(this->controller_.is_scanning());
  this->complete_();
  EXPECT_EQ(std::count(this->requested_.begin(), this->requested_.end(), 55), 1);
  EXPECT_FALSE(this->controller_.write_uncertain());
  EXPECT_EQ(this->controller_.writes_sent(), 1u);
}
#endif
#endif

}  // namespace esphome::vbus::testing
