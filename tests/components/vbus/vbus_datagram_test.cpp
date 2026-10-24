#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

#include "esphome/components/vbus/vbus.h"

namespace esphome::vbus::testing {
namespace {

class DatagramTestUART : public uart::UARTComponent {
 public:
  void inject(const std::vector<uint8_t> &bytes) { this->rx_.insert(this->rx_.end(), bytes.begin(), bytes.end()); }
  size_t available() override { return this->rx_.size() - this->position_; }
  bool read_array(uint8_t *data, size_t size) override {
    if (size > this->available())
      return false;
    std::copy_n(this->rx_.data() + this->position_, size, data);
    this->position_ += size;
    return true;
  }
  bool peek_byte(uint8_t *data) override {
    if (!this->available())
      return false;
    *data = this->rx_[this->position_];
    return true;
  }
  void write_array(const uint8_t *data, size_t size) override { ADD_FAILURE() << "P2 RX must not transmit"; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

 protected:
  std::vector<uint8_t> rx_;
  size_t position_{0};
};

class DatagramTestP1Listener : public VBusListener {
 public:
  std::vector<std::vector<uint8_t>> messages;

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->messages.push_back(message); }
};

using datagram_fields_t = std::array<uint32_t, 5>;
const std::vector<uint8_t> REFERENCE_DATAGRAM = {0xaa, 0x10, 0x00, 0x11, 0x22, 0x20, 0x00, 0x09,
                                                 0x34, 0x12, 0x6f, 0x4d, 0x2b, 0x09, 0x3e, 0x1f};
const datagram_fields_t REFERENCE_FIELDS = {0x0900, 0x2211, 0x0010, 0x9234, 0x89abcdef};
const std::vector<uint8_t> DATAGRAM_TEST_P1 = {0xaa, 0x10, 0x00, 0x11, 0x22, 0x10, 0x00, 0x01,
                                               0x01, 0x2a, 0x01, 0x02, 0x03, 0x04, 0x00, 0x75};

void set_datagram_checksum(std::vector<uint8_t> &bytes) {
  uint32_t sum = 0;
  for (size_t i = 1; i < 15; ++i)
    sum += bytes[i];
  bytes[15] = (0x7f - sum) & 0x7f;
}

class VBusDatagramTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.set_uart_parent(&this->uart_);
    this->bus_.register_listener(&this->p1_);
    this->bus_.add_on_datagram_callback(
        [this](uint16_t command, uint16_t source, uint16_t destination, uint16_t value_id, uint32_t value) {
          this->datagrams_.push_back({command, source, destination, value_id, value});
        });
  }
  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.inject(bytes);
    this->bus_.loop();
  }
  void reference_() { this->feed_(REFERENCE_DATAGRAM); }

  DatagramTestUART uart_;
  VBus bus_{};
  DatagramTestP1Listener p1_;
  std::vector<datagram_fields_t> datagrams_;
};

TEST_F(VBusDatagramTest, DecodesReferenceDatagramWithoutPublishingP1) {
  this->reference_();
  ASSERT_EQ(this->datagrams_.size(), 1u);
  EXPECT_EQ(this->datagrams_[0], REFERENCE_FIELDS);
  EXPECT_TRUE(this->p1_.messages.empty());
}

#ifdef USE_VBUS_RX_DIAGNOSTICS
TEST_F(VBusDatagramTest, CountsChecksumAndReservedSeptetFailuresOncePerRejectedDatagram) {
  auto bad = REFERENCE_DATAGRAM;
  bad.back() ^= 1;
  this->feed_(bad);
  EXPECT_EQ(this->bus_.get_packet_error_count(), 1u);
  bad = REFERENCE_DATAGRAM;
  bad[14] |= 0x40;
  set_datagram_checksum(bad);
  this->feed_(bad);
  EXPECT_EQ(this->bus_.get_packet_error_count(), 2u);
  bad.back() ^= 1;
  this->feed_(bad);
  EXPECT_EQ(this->bus_.get_packet_error_count(), 3u);
  EXPECT_TRUE(this->datagrams_.empty());
  this->reference_();
  EXPECT_EQ(this->bus_.get_packet_error_count(), 3u);
  ASSERT_EQ(this->datagrams_.size(), 1u);
  EXPECT_EQ(this->datagrams_[0], REFERENCE_FIELDS);
}
#endif

TEST_F(VBusDatagramTest, SupportsMultipleCallbacksAndSeparateHubs) {
  std::vector<datagram_fields_t> second;
  this->bus_.add_on_datagram_callback([&second](uint16_t c, uint16_t s, uint16_t d, uint16_t i, uint32_t v) {
    second.push_back({c, s, d, i, v});
  });
  VBus other{};
  DatagramTestUART other_uart;
  other.set_uart_parent(&other_uart);
  other_uart.inject(REFERENCE_DATAGRAM);
  other.loop();
  EXPECT_TRUE(this->datagrams_.empty());
  this->reference_();
  ASSERT_EQ(second.size(), 1u);
  ASSERT_EQ(this->datagrams_.size(), 1u);
  EXPECT_EQ(second[0], REFERENCE_FIELDS);
}

TEST_F(VBusDatagramTest, KeepsPacketAndDatagramListenersSeparateInMixedTraffic) {
  for (const auto &bytes : {DATAGRAM_TEST_P1, REFERENCE_DATAGRAM, DATAGRAM_TEST_P1, REFERENCE_DATAGRAM})
    this->uart_.inject(bytes);
  // Works both with an unbounded parser and with the pending per-loop byte budget.
  while (this->uart_.available())
    this->bus_.loop();
  ASSERT_EQ(this->datagrams_.size(), 2u);
  ASSERT_EQ(this->p1_.messages.size(), 2u);
  EXPECT_EQ(this->datagrams_[0], REFERENCE_FIELDS);
  EXPECT_EQ(this->p1_.messages[0], (std::vector<uint8_t>{1, 2, 3, 4}));
}

TEST_F(VBusDatagramTest, RejectsUnusedSeptetBitEvenWithAValidChecksum) {
  auto bad = REFERENCE_DATAGRAM;
  bad[14] |= 0x40;
  set_datagram_checksum(bad);
  this->feed_(bad);
  EXPECT_TRUE(this->datagrams_.empty());
  this->reference_();
  ASSERT_EQ(this->datagrams_.size(), 1u);
}

TEST_F(VBusDatagramTest, PreservesMinimumAndMaximumFieldValues) {
  for (uint8_t byte : {0x00, 0x7f}) {
    auto bytes = REFERENCE_DATAGRAM;
    for (size_t i = 1; i < 15; ++i)
      bytes[i] = byte;
    bytes[5] = 0x20;
    bytes[14] = byte ? 0x3f : 0;
    set_datagram_checksum(bytes);
    this->feed_(bytes);
  }
  ASSERT_EQ(this->datagrams_.size(), 2u);
  EXPECT_EQ(this->datagrams_[0], (datagram_fields_t{0, 0, 0, 0, 0}));
  EXPECT_EQ(this->datagrams_[1], (datagram_fields_t{0x7f7f, 0x7f7f, 0x7f7f, 0xffff, 0xffffffff}));
}

TEST_F(VBusDatagramTest, IgnoresNoiseAndUnknownProtocolVersions) {
  this->feed_({0x00, 0xff, 0x55, 0x7f});
  for (uint8_t protocol : {0x00, 0x21, 0x30, 0x7f}) {
    auto bytes = REFERENCE_DATAGRAM;
    bytes[5] = protocol;
    set_datagram_checksum(bytes);
    this->feed_(bytes);
    ASSERT_TRUE(this->datagrams_.empty());
  }
  this->reference_();
  ASSERT_EQ(this->datagrams_.size(), 1u);
  EXPECT_EQ(this->datagrams_[0], REFERENCE_FIELDS);
}

class VBusSeptetTest : public VBusDatagramTest, public ::testing::WithParamInterface<uint8_t> {};

TEST_P(VBusSeptetTest, PreservesEveryPayloadHighBitCombination) {
  auto bytes = REFERENCE_DATAGRAM;
  for (size_t i = 8; i < 14; ++i)
    bytes[i] = 0x7f;
  bytes[14] = GetParam();
  set_datagram_checksum(bytes);
  this->feed_(bytes);
  const auto byte = [](size_t i) -> uint32_t { return (GetParam() & (1u << i)) ? 0xff : 0x7f; };
  ASSERT_EQ(this->datagrams_.size(), 1u);
  EXPECT_EQ(this->datagrams_[0][3], byte(0) | (byte(1) << 8));
  EXPECT_EQ(this->datagrams_[0][4], byte(2) | (byte(3) << 8) | (byte(4) << 16) | (byte(5) << 24));
}

INSTANTIATE_TEST_SUITE_P(AllSixBits, VBusSeptetTest, ::testing::Range(uint8_t{0}, uint8_t{64}));

class VBusCorruptionTest : public VBusDatagramTest, public ::testing::WithParamInterface<size_t> {};

TEST_P(VBusCorruptionTest, RejectsEachCorruptedWireBitAndRecovers) {
  auto bad = REFERENCE_DATAGRAM;
  const size_t index = 1 + GetParam() / 8;
  bad[index] ^= 1u << (GetParam() % 8);
  this->feed_(bad);
  ASSERT_TRUE(this->datagrams_.empty());
  this->reference_();
  ASSERT_EQ(this->datagrams_.size(), 1u);
  EXPECT_EQ(this->datagrams_[0], REFERENCE_FIELDS);
}

INSTANTIATE_TEST_SUITE_P(AllBodyBits, VBusCorruptionTest, ::testing::Range(size_t{0}, size_t{120}));

class VBusDatagramSplitTest : public VBusDatagramTest, public ::testing::WithParamInterface<size_t> {};

TEST_P(VBusDatagramSplitTest, PublishesOnlyAfterTheChecksumArrives) {
  const auto split = REFERENCE_DATAGRAM.begin() + GetParam();
  this->feed_({REFERENCE_DATAGRAM.begin(), split});
  ASSERT_TRUE(this->datagrams_.empty());
  this->feed_({split, REFERENCE_DATAGRAM.end()});
  ASSERT_EQ(this->datagrams_.size(), 1u);
  EXPECT_EQ(this->datagrams_[0], REFERENCE_FIELDS);
}

TEST_P(VBusDatagramSplitTest, ResynchronizesAtANewSyncInsideATruncatedDatagram) {
  this->feed_({REFERENCE_DATAGRAM.begin(), REFERENCE_DATAGRAM.begin() + GetParam()});
  ASSERT_TRUE(this->datagrams_.empty());
  this->reference_();
  ASSERT_EQ(this->datagrams_.size(), 1u);
  EXPECT_EQ(this->datagrams_[0], REFERENCE_FIELDS);
}

INSTANTIATE_TEST_SUITE_P(EveryWireByte, VBusDatagramSplitTest, ::testing::Range(size_t{1}, size_t{16}));

}  // namespace
}  // namespace esphome::vbus::testing
