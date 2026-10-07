#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus::testing {

namespace {

// Answers register reads with one fixed value.
class RegisterDevice : public ModbusServerDevice {
 public:
  explicit RegisterDevice(uint8_t address) { this->set_address(address); }
  ResponseStatus on_read_registers(uint16_t start_address, uint16_t number_of_registers,
                                   RegisterValues &registers) override {
    for (uint16_t i = 0; i < number_of_registers; i++)
      registers.push_back(0x1234);
    return std::nullopt;
  }
};

struct Request {
  uint8_t address;
  std::vector<uint8_t> pdu;
  size_t written_before;  // bytes the hub had written when the callback ran
};

class MonitorServer : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_uart_parent(&this->uart_);
    this->hub_.setup();
    this->hub_.add_on_request_callback([this](uint8_t address, std::span<const uint8_t> pdu) {
      this->seen_.push_back({address, {pdu.begin(), pdu.end()}, this->uart_.written.size()});
    });
  }
  void receive_(uint8_t address, std::span<const uint8_t> pdu) {
    this->uart_.inject_frame(address, pdu);
    this->hub_.loop();
  }
  static std::vector<uint8_t> vec_(std::span<const uint8_t> pdu) { return {pdu.begin(), pdu.end()}; }

  InjectableUART uart_;
  ModbusServerHub hub_;
  std::vector<Request> seen_;
};

constexpr uint8_t READ[] = {0x03, 0x00, 0x10, 0x00, 0x01};
constexpr uint8_t READ_REPLY[] = {0x03, 0x02, 0x00, 0x2A};
constexpr uint8_t WRITE_1[] = {0x06, 0x00, 0x10, 0x00, 0x01};
constexpr uint8_t WRITE_2[] = {0x06, 0x00, 0x10, 0x00, 0x02};

}  // namespace

// A request to a local device is seen too, after the device has answered.
TEST_F(MonitorServer, ServedRequestIsSeenAfterTheReply) {
  RegisterDevice device(0x02);
  this->hub_.register_device(&device);
  this->receive_(0x02, READ);
  ASSERT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(this->seen_[0].address, 0x02);
  EXPECT_EQ(this->seen_[0].pdu, vec_(READ));
  EXPECT_FALSE(this->uart_.written.empty());
  EXPECT_EQ(this->seen_[0].written_before, this->uart_.written.size());
}

TEST_F(MonitorServer, UnservedRequestIsSeen) {
  this->receive_(0x05, READ);
  ASSERT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(this->seen_[0].address, 0x05);
  EXPECT_EQ(this->seen_[0].pdu, vec_(READ));
  EXPECT_TRUE(this->uart_.written.empty());
}

// The hub already expects the other device's reply when the callback runs, so the reply is not
// seen as a request, and the request after it is.
TEST_F(MonitorServer, PeerReadReplyIsNotSeen) {
  this->receive_(0x05, READ);
  this->receive_(0x05, READ_REPLY);
  this->receive_(0x06, READ);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].address, 0x05);
  EXPECT_EQ(this->seen_[1].address, 0x06);
}

// A single write and its reply have the same shape.
TEST_F(MonitorServer, PeerWriteReplyIsNotSeen) {
  this->receive_(0x05, WRITE_1);
  this->receive_(0x05, WRITE_1);  // the other device's reply
  this->receive_(0x05, WRITE_2);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].pdu, vec_(WRITE_1));
  EXPECT_EQ(this->seen_[1].pdu, vec_(WRITE_2));
}

// Known gap, pinned: when the other device does not answer, the next request with the same shape to
// the same address is taken for the missing reply and not seen.
TEST_F(MonitorServer, RepeatedWriteWithoutReplyIsTakenForTheReply) {
  this->receive_(0x05, WRITE_1);
  this->receive_(0x05, WRITE_2);  // no reply came; this request is taken for it
  this->receive_(0x05, WRITE_1);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].pdu, vec_(WRITE_1));
  EXPECT_EQ(this->seen_[1].pdu, vec_(WRITE_1));
}

// The same holds for a request to another address: any frame with the shape of a reply ends the wait.
TEST_F(MonitorServer, WriteToAnotherAddressWithoutReplyIsTakenForTheReply) {
  this->receive_(0x05, WRITE_1);
  this->receive_(0x06, WRITE_2);  // no reply came from 0x05; this request is taken for it
  this->receive_(0x06, WRITE_1);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[1].address, 0x06);
  EXPECT_EQ(this->seen_[1].pdu, vec_(WRITE_1));
}

// With no reply expected (at boot, after a broadcast), a write echo has the shape of a request.
TEST_F(MonitorServer, WriteEchoWithNothingExpectedIsSeen) {
  this->receive_(BROADCAST_ADDRESS, WRITE_1);
  this->receive_(0x05, WRITE_2);  // in fact the echo of a request sent before this hub listened
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[1].address, 0x05);
  EXPECT_EQ(this->seen_[1].pdu, vec_(WRITE_2));
}

TEST_F(MonitorServer, BroadcastIsSeen) {
  this->receive_(BROADCAST_ADDRESS, WRITE_1);
  ASSERT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(this->seen_[0].address, BROADCAST_ADDRESS);
  EXPECT_EQ(this->seen_[0].pdu, vec_(WRITE_1));
  EXPECT_TRUE(this->uart_.written.empty());
}

TEST_F(MonitorServer, EveryCallbackRuns) {
  int second = 0;
  this->hub_.add_on_request_callback([&second](uint8_t, std::span<const uint8_t>) { second++; });
  this->receive_(0x05, READ);
  EXPECT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(second, 1);
}

// The largest register write: 123 registers, a PDU of 252 bytes.
TEST_F(MonitorServer, LargestRequestIsSeenWhole) {
  std::vector<uint8_t> pdu = {0x10, 0x00, 0x00, 0x00, 123, 246};
  for (int i = 0; i < 246; i++)
    pdu.push_back(static_cast<uint8_t>(i));
  this->receive_(0x05, pdu);
  ASSERT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(this->seen_[0].pdu, pdu);
}

TEST_F(MonitorServer, UnknownFunctionIsSeen) {
  const uint8_t pdu[] = {0x49, 0x02, 0xAA, 0xBB};
  this->receive_(0x05, pdu);
  ASSERT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(this->seen_[0].pdu, vec_(pdu));
}

}  // namespace esphome::modbus::testing
