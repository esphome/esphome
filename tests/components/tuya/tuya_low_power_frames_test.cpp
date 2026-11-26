#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "esphome/components/tuya/tuya_uart.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"

// Drives whole frames through TuyaUART with the low_power protocol: handshake, write queue and recovery.
namespace esphome::tuya::testing {

namespace {

struct Frame {
  uint8_t version;
  uint8_t command;
  std::vector<uint8_t> payload;
};

std::vector<uint8_t> encode(const Frame &f) {
  std::vector<uint8_t> out{0x55,
                           0xAA,
                           f.version,
                           f.command,
                           static_cast<uint8_t>(f.payload.size() >> 8),
                           static_cast<uint8_t>(f.payload.size())};
  out.insert(out.end(), f.payload.begin(), f.payload.end());
  uint8_t sum = 0;
  for (uint8_t b : out)
    sum += b;
  out.push_back(sum);
  return out;
}

std::vector<uint8_t> bytes(const std::string &s) { return {s.begin(), s.end()}; }

class MockUART : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;
  std::vector<uint8_t> tx;

  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty())
      return false;
    *data = this->rx.front();
    return true;
  }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32) || defined(USE_LIBRETINY)
  void load_settings(bool dump_config) override {}
#endif

  // The frames sent since the last call
  std::vector<Frame> sent() {
    std::vector<Frame> frames;
    size_t i = 0;
    while (i + 7 <= this->tx.size()) {
      const size_t len = (this->tx[i + 4] << 8) | this->tx[i + 5];
      if (this->tx[i] != 0x55 || this->tx[i + 1] != 0xAA || i + 7 + len > this->tx.size())
        break;
      frames.push_back({this->tx[i + 2], this->tx[i + 3], {this->tx.begin() + i + 6, this->tx.begin() + i + 6 + len}});
      i += 7 + len;
    }
    this->tx.clear();
    return frames;
  }
};

class TuyaLowPowerFrames : public ::testing::Test {
 protected:
  void SetUp() override {
    this->tuya_.set_uart_parent(&this->uart_);
    this->tuya_.add_on_mcu_ready_callback([this]() { this->ready_count_++; });
    this->step_(0);
    this->tuya_.setup();
  }

  // One loop pass at loop time now. Commands keep a 10 ms gap measured with millis(), so wait that long first.
  void step_(uint32_t now) {
    std::this_thread::sleep_for(std::chrono::milliseconds(12));
    LoopBlockingGuard{nullptr, nullptr, now};
    this->tuya_.loop();
  }

  void receive_(uint32_t now, const Frame &frame) {
    const auto raw = encode(frame);
    this->uart_.rx.insert(this->uart_.rx.end(), raw.begin(), raw.end());
    this->step_(now);
  }

  // Product info of a version-0 or version-3 MCU, after the module asked for it
  void handshake_(uint8_t version) {
    auto sent = this->uart_.sent();
    ASSERT_EQ(sent.size(), 1u);
    EXPECT_EQ(sent[0].command, 0x01);
    this->receive_(100, {version, 0x01, bytes(R"({"p":"abcdefgh","v":"1.0.0"})")});
    this->step_(110);
  }

  MockUART uart_;
  TuyaUART tuya_;
  int ready_count_{0};
};

}  // namespace

TEST_F(TuyaLowPowerFrames, VersionZeroRecoversAfterAckTimeout) {
  this->handshake_(0);
  // The host build is online, so the module reports "connected to the cloud"
  auto sent = this->uart_.sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].command, 0x02);
  EXPECT_EQ(sent[0].payload, std::vector<uint8_t>{0x04});

  // No ACK: after the timeout the module asks for the product info directly, 500 ms later
  this->step_(110 + 1500);
  EXPECT_TRUE(this->uart_.sent().empty());
  this->step_(110 + 1500 + 500);
  sent = this->uart_.sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].command, 0x01);

  // The MCU answers: the network state goes out again and its ACK ends the recovery
  this->receive_(2200, {0x00, 0x01, bytes(R"({"p":"abcdefgh","v":"1.0.0"})")});
  this->step_(2210);
  sent = this->uart_.sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].command, 0x02);
  this->receive_(2300, {0x00, 0x02, {}});
  EXPECT_TRUE(this->uart_.sent().empty());

  // The full status follows as CMD05 reports; the MCU is ready once they have been quiet for 500 ms
  this->receive_(2400, {0x00, 0x05, {0x01, 0x01, 0x00, 0x01, 0x01}});
  EXPECT_EQ(this->uart_.sent().size(), 1u);  // its ACK
  this->step_(2899);
  EXPECT_EQ(this->ready_count_, 0);
  this->step_(2900);
  EXPECT_EQ(this->ready_count_, 1);
}

TEST_F(TuyaLowPowerFrames, VersionZeroAcksOnlyValidReports) {
  this->handshake_(0);
  this->uart_.sent();
  // A bitmask datapoint must be 1, 2 or 4 bytes: the whole frame gets no ACK
  this->receive_(200, {0x00, 0x05, {0x07, 0x05, 0x00, 0x03, 0x01, 0x02, 0x03}});
  EXPECT_TRUE(this->uart_.sent().empty());
  // Each datapoint of a valid report is acknowledged on its own
  this->receive_(300, {0x00, 0x05, {0x01, 0x01, 0x00, 0x01, 0x01, 0x02, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05}});
  auto sent = this->uart_.sent();
  ASSERT_EQ(sent.size(), 2u);
  EXPECT_EQ(sent[0].command, 0x05);
  EXPECT_EQ(sent[0].payload, (std::vector<uint8_t>{0x00, 0x01}));
  EXPECT_EQ(sent[1].payload, (std::vector<uint8_t>{0x00, 0x02}));
}

TEST_F(TuyaLowPowerFrames, VersionThreeHandshakeAndWrites) {
  this->handshake_(3);
  // Version 3: CMD02 is the empty working-mode query; its answer may carry two pin bytes
  auto sent = this->uart_.sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].command, 0x02);
  EXPECT_TRUE(sent[0].payload.empty());
  this->receive_(200, {0x03, 0x02, {0x0E, 0x0F}});
  sent = this->uart_.sent();
  ASSERT_EQ(sent.size(), 2u);
  EXPECT_EQ(sent[0].command, 0x03);
  EXPECT_EQ(sent[1].command, 0x08);

  // The full status arrives: the MCU is ready, once
  this->receive_(300, {0x03, 0x07, {0x01, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05}});
  EXPECT_EQ(this->ready_count_, 1);

  // Two writes: the second waits until the MCU's CMD07 confirms the first
  this->tuya_.set_boolean_datapoint_value(1, true);
  this->tuya_.set_integer_datapoint_value(2, 7);
  this->step_(400);
  sent = this->uart_.sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].command, 0x06);
  EXPECT_EQ(sent[0].payload, (std::vector<uint8_t>{0x01, 0x01, 0x00, 0x01, 0x01}));
  this->step_(500);
  EXPECT_TRUE(this->uart_.sent().empty());
  this->receive_(600, {0x03, 0x07, {0x01, 0x01, 0x00, 0x01, 0x01}});
  this->step_(610);
  sent = this->uart_.sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].command, 0x06);
  EXPECT_EQ(sent[0].payload[0], 0x02);
  this->receive_(700, {0x03, 0x07, {0x02, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x07}});
  EXPECT_EQ(this->ready_count_, 1);

  // A restarted MCU gets the working-mode handshake again
  this->receive_(800, {0x03, 0x00, {0x00}});
  this->step_(810);
  bool mode_query = false;
  for (const auto &f : this->uart_.sent())
    mode_query |= f.command == 0x02;
  EXPECT_TRUE(mode_query);
}

TEST_F(TuyaLowPowerFrames, VersionThreeWriteSurvivesLostConfirmation) {
  this->handshake_(3);
  this->uart_.sent();
  this->receive_(200, {0x03, 0x02, {}});
  this->receive_(300, {0x03, 0x07, {0x01, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05}});
  this->uart_.sent();

  this->tuya_.set_boolean_datapoint_value(1, true);
  this->step_(400);
  ASSERT_EQ(this->uart_.sent().size(), 1u);
  // A second write is still queued when the first goes unanswered
  this->tuya_.set_integer_datapoint_value(2, 9);
  this->step_(400 + 1500);
  this->step_(400 + 1500 + 500);
  auto sent = this->uart_.sent();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].command, 0x01);  // recovery

  // After the resync the kept write goes out
  this->receive_(2500, {0x03, 0x01, bytes(R"({"p":"abcdefgh","v":"1.0.0"})")});
  this->step_(2510);
  this->receive_(2600, {0x03, 0x02, {}});
  this->receive_(2700, {0x03, 0x07, {0x01, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05}});
  this->step_(2710);
  bool kept_write = false;
  for (const auto &f : this->uart_.sent())
    kept_write |= f.command == 0x06 && !f.payload.empty() && f.payload[0] == 0x02;
  EXPECT_TRUE(kept_write);
}

}  // namespace esphome::tuya::testing
