#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "esphome/components/tormatic/tormatic_cover.h"

namespace esphome::tormatic::testing {

class TestUART : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;
  std::vector<uint8_t> tx;
  size_t offset{0};

  void write_array(const uint8_t *data, size_t len) override { this->tx.assign(data, data + len); }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->available() < len)
      return false;
    memcpy(data, this->rx.data() + this->offset, len);
    this->offset += len;
    return true;
  }
  bool peek_byte(uint8_t *data) override {
    if (this->available() == 0)
      return false;
    *data = this->rx[this->offset];
    return true;
  }
  size_t available() override { return this->rx.size() - this->offset; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  MessageHeader last_header() {
    MessageHeader header;
    memcpy(&header, this->tx.data(), sizeof(header));
    header.byteswap();
    return header;
  }

  void append_status(uint16_t seq, GateStatus state) {
    auto header = serialize(MessageHeader(STATUS, seq, sizeof(StatusReply)));
    this->rx.insert(this->rx.end(), header.begin(), header.end());
    this->rx.insert(this->rx.end(), {0x02, static_cast<uint8_t>(state), 0x00});
  }

 protected:
  void check_logger_conflict() override {}
};

class TormaticTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->gate_.set_uart_parent(&this->uart_);
    this->gate_.set_open_duration(15000);
    this->gate_.set_close_duration(22000);
    this->gate_.position = COVER_CLOSED;
  }

  uint16_t request_gate_status() {
    this->gate_.current_operation = COVER_OPERATION_OPENING;
    this->gate_.update();
    this->gate_.current_operation = COVER_OPERATION_IDLE;
    return this->uart_.last_header().seq;
  }

  void report_status(GateStatus state) {
    auto seq = this->request_gate_status();
    this->uart_.append_status(seq, state);
    this->gate_.loop();
  }

  TestUART uart_;
  Tormatic gate_;
};

TEST_F(TormaticTest, MalformedStatusDoesNotConsumeNextFrame) {
  auto seq = this->request_gate_status();
  this->uart_.rx = serialize(MessageHeader(STATUS, seq, 1));
  this->uart_.rx.push_back(0x02);
  this->uart_.append_status(seq, VENTILATING);

  this->gate_.loop();
  EXPECT_EQ(this->uart_.available(), sizeof(MessageHeader) + sizeof(StatusReply));
  this->gate_.loop();
  EXPECT_EQ(this->uart_.available(), 0);
  EXPECT_FLOAT_EQ(this->gate_.position, COVER_OPEN);
}

TEST_F(TormaticTest, FragmentedPayloadWaitsForRemainingBytes) {
  auto seq = this->request_gate_status();
  this->uart_.rx = serialize(MessageHeader(STATUS, seq, sizeof(StatusReply)));
  this->uart_.rx.push_back(0x02);

  this->gate_.loop();
  EXPECT_EQ(this->uart_.available(), 1);
  EXPECT_FLOAT_EQ(this->gate_.position, COVER_CLOSED);

  this->uart_.rx.insert(this->uart_.rx.end(), {VENTILATING, 0x00});
  this->gate_.loop();
  EXPECT_EQ(this->uart_.available(), 0);
  EXPECT_FLOAT_EQ(this->gate_.position, COVER_OPEN);
}

TEST_F(TormaticTest, UnmatchedStatusIsIgnored) {
  auto old_seq = this->request_gate_status();
  auto seq = this->request_gate_status();
  this->uart_.append_status(old_seq, VENTILATING);
  this->uart_.append_status(seq, CLOSED);

  this->gate_.loop();
  EXPECT_FLOAT_EQ(this->gate_.position, COVER_CLOSED);
  this->gate_.loop();
  EXPECT_EQ(this->uart_.available(), 0);
  EXPECT_FLOAT_EQ(this->gate_.position, COVER_CLOSED);
}

TEST_F(TormaticTest, LightCommandDoesNotReplacePendingGateRequest) {
  auto seq = this->request_gate_status();
  this->gate_.send_light_command(true);
  this->uart_.append_status(seq, VENTILATING);
  this->gate_.loop();
  EXPECT_FLOAT_EQ(this->gate_.position, COVER_OPEN);
}

TEST_F(TormaticTest, VentilationReportsOpenAndRejectsPartialPosition) {
  this->report_status(VENTILATING);
  EXPECT_FLOAT_EQ(this->gate_.position, COVER_OPEN);
  EXPECT_EQ(this->gate_.current_operation, COVER_OPERATION_IDLE);

  this->uart_.tx.clear();
  this->gate_.make_call().set_position(0.5f).perform();
  EXPECT_TRUE(this->uart_.tx.empty());

  this->gate_.make_call().set_position(COVER_OPEN).perform();
  ASSERT_EQ(this->uart_.tx.size(), sizeof(MessageHeader) + sizeof(CommandRequestReply));
  EXPECT_EQ(this->uart_.tx.back(), OPENED);
}

TEST_F(TormaticTest, ClosedEndpointRestoresPartialPositionControl) {
  this->report_status(VENTILATING);
  this->report_status(CLOSED);

  this->uart_.tx.clear();
  this->gate_.make_call().set_position(0.5f).perform();
  ASSERT_EQ(this->uart_.tx.size(), sizeof(MessageHeader) + sizeof(CommandRequestReply));
  EXPECT_EQ(this->uart_.tx.back(), OPENED);
}

}  // namespace esphome::tormatic::testing
