#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus/modbus_helpers.h"

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

struct Response {
  uint8_t address;
  std::vector<uint8_t> request;
  std::vector<uint8_t> response;
};

// Lets the bus fall silent past the frame timeout without waiting for real time.
class QuietServerHub : public ModbusServerHub {
 public:
  void go_quiet() { this->last_modbus_byte_ -= 1000000; }
};

class MonitorServer : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_uart_parent(&this->uart_);
    this->hub_.setup();
    this->hub_.add_on_request_callback([this](uint8_t address, std::span<const uint8_t> pdu) {
      this->seen_.push_back({address, {pdu.begin(), pdu.end()}, this->uart_.written.size()});
    });
    this->hub_.add_on_response_callback(
        [this](uint8_t address, std::span<const uint8_t> request, std::span<const uint8_t> response) {
          this->paired_.push_back({address, vec(request), vec(response)});
        });
  }
  void receive_(uint8_t address, std::span<const uint8_t> pdu) {
    this->uart_.inject_frame(address, pdu);
    this->hub_.loop();
  }
  // A gap on the wire longer than the frame timeout: a partial frame in the buffer is dropped.
  void pause_() {
    this->hub_.go_quiet();
    this->hub_.loop();
  }
  static std::vector<uint8_t> vec(std::span<const uint8_t> pdu) { return {pdu.begin(), pdu.end()}; }

  InjectableUART uart_;
  QuietServerHub hub_;
  std::vector<Request> seen_;
  std::vector<Response> paired_;
};

constexpr uint8_t READ[] = {0x03, 0x00, 0x10, 0x00, 0x01};
constexpr uint8_t READ_REPLY[] = {0x03, 0x02, 0x00, 0x2A};
constexpr uint8_t READ_TWO[] = {0x03, 0x00, 0xCA, 0x00, 0x02};  // two registers from 202
constexpr uint8_t READ_TWO_REPLY[] = {0x03, 0x04, 0x10, 0x04, 0xFF, 0x30};
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
  EXPECT_EQ(this->seen_[0].pdu, vec(READ));
  EXPECT_FALSE(this->uart_.written.empty());
  EXPECT_EQ(this->seen_[0].written_before, this->uart_.written.size());
}

// The second request arrives while the hub still waits for the first one's reply.
TEST_F(MonitorServer, UnansweredRequestsAreSeenButNotPaired) {
  this->receive_(0x05, READ);  // absent device
  this->receive_(0x06, READ_TWO);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].address, 0x05);
  EXPECT_EQ(this->seen_[0].pdu, vec(READ));
  EXPECT_EQ(this->seen_[1].address, 0x06);
  EXPECT_EQ(this->seen_[1].pdu, vec(READ_TWO));
  EXPECT_TRUE(this->paired_.empty());
}

// A single write and its reply have the same shape.
TEST_F(MonitorServer, PeerWriteReplyIsNotSeen) {
  this->receive_(0x05, WRITE_1);
  this->receive_(0x05, WRITE_1);  // the other device's reply
  this->receive_(0x05, WRITE_2);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].pdu, vec(WRITE_1));
  EXPECT_EQ(this->seen_[1].pdu, vec(WRITE_2));
}

// Known gap: a write and its reply have the same shape, so the next write is taken for a missing reply.
TEST_F(MonitorServer, UnansweredWriteIsPairedWithTheNextWrite) {
  this->receive_(0x05, WRITE_1);
  this->receive_(0x05, WRITE_2);  // no reply came; this request is taken for it
  this->receive_(0x05, WRITE_1);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].pdu, vec(WRITE_1));
  EXPECT_EQ(this->seen_[1].pdu, vec(WRITE_1));
  ASSERT_EQ(this->paired_.size(), 1u);
  EXPECT_EQ(this->paired_[0].request, vec(WRITE_1));
  EXPECT_EQ(this->paired_[0].response, vec(WRITE_2));
}

// Any frame with the shape of a reply ends the wait, so a write to another address is not seen either.
TEST_F(MonitorServer, WriteToAnotherAddressWithoutReplyIsTakenForTheReply) {
  this->receive_(0x05, WRITE_1);
  this->receive_(0x06, WRITE_2);  // no reply came from 0x05; this request is taken for it
  this->receive_(0x06, WRITE_1);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[1].address, 0x06);
  EXPECT_EQ(this->seen_[1].pdu, vec(WRITE_1));
}

// Known gap: a read from 0x0300-0x03FF has the shape of a 3-byte read reply, so it is taken for a missing reply.
TEST_F(MonitorServer, UnansweredReadIsPairedWithAReadOf0x03xx) {
  const uint8_t read_0x0320[] = {0x03, 0x03, 0x20, 0x00, 0x01};
  this->receive_(0x05, READ);
  this->receive_(0x05, read_0x0320);
  EXPECT_EQ(this->seen_.size(), 1u);
  ASSERT_EQ(this->paired_.size(), 1u);
  EXPECT_EQ(this->paired_[0].request, vec(READ));
  EXPECT_EQ(this->paired_[0].response, vec(read_0x0320));
}

// A single write echo is identical to its request, so one seen with no reply expected (the hub started
// mid-exchange) is taken for a request. The next request replaces it.
TEST_F(MonitorServer, WriteEchoSeenFirstIsTakenForARequest) {
  this->receive_(0x05, WRITE_1);  // the echo; its request was missed
  this->receive_(0x05, READ);
  this->receive_(0x05, READ_REPLY);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].address, 0x05);
  EXPECT_EQ(this->seen_[0].pdu, vec(WRITE_1));
  ASSERT_EQ(this->paired_.size(), 1u);
  EXPECT_EQ(this->paired_[0].request, vec(READ));
}

// A broadcast is never answered, so it is seen but never paired, not even with a reply-shaped frame from address 0.
TEST_F(MonitorServer, BroadcastIsSeenButNeverPaired) {
  this->receive_(BROADCAST_ADDRESS, READ);
  this->receive_(BROADCAST_ADDRESS, READ_REPLY);
  ASSERT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(this->seen_[0].address, BROADCAST_ADDRESS);
  EXPECT_EQ(this->seen_[0].pdu, vec(READ));
  EXPECT_TRUE(this->paired_.empty());
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
  EXPECT_EQ(this->seen_[0].pdu, vec(pdu));
}

// The reply of another device is reported raw with the request it answers, which holds the start address
// the reply lacks.
TEST_F(MonitorServer, PeerResponseIsSeenWithItsRequest) {
  this->receive_(0x05, READ_TWO);
  this->receive_(0x05, READ_TWO_REPLY);
  ASSERT_EQ(this->paired_.size(), 1u);
  EXPECT_EQ(this->paired_[0].address, 0x05);
  EXPECT_EQ(this->paired_[0].request, vec(READ_TWO));
  EXPECT_EQ(this->paired_[0].response, vec(READ_TWO_REPLY));
  EXPECT_EQ(helpers::get_data<uint16_t>(this->paired_[0].request.data(), 1), 202);
  const auto payload = helpers::server_pdu_payload(this->paired_[0].response);
  ASSERT_EQ(payload.size(), 4u);
  EXPECT_EQ(helpers::get_data<uint16_t>(payload.data(), 0), 0x1004);
}

// Only the requests are seen, and each reply is paired with its own request.
TEST_F(MonitorServer, TwoExchangesInARowArePairedSeparately) {
  this->receive_(0x05, READ);
  this->receive_(0x05, READ_REPLY);
  this->receive_(0x06, READ_TWO);
  this->receive_(0x06, READ_TWO_REPLY);
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].address, 0x05);
  EXPECT_EQ(this->seen_[1].address, 0x06);
  ASSERT_EQ(this->paired_.size(), 2u);
  EXPECT_EQ(this->paired_[0].address, 0x05);
  EXPECT_EQ(this->paired_[0].request, vec(READ));
  EXPECT_EQ(this->paired_[1].address, 0x06);
  EXPECT_EQ(this->paired_[1].request, vec(READ_TWO));
  EXPECT_EQ(this->paired_[1].response, vec(READ_TWO_REPLY));
}

// The PDUs are passed on raw, so every function code pairs, exception replies included.
TEST_F(MonitorServer, EveryFunctionCodeIsPaired) {
  struct Case {
    const char *name;
    std::vector<uint8_t> request;
    std::vector<uint8_t> response;
  };
  const std::vector<Case> cases{
      {"write single register", {0x06, 0x04, 0x4D, 0x00, 0x01}, {0x06, 0x04, 0x4D, 0x00, 0x01}},
      {"write multiple registers", {0x10, 0x04, 0x4D, 0x00, 0x01, 0x02, 0x00, 0x01}, {0x10, 0x04, 0x4D, 0x00, 0x01}},
      {"read coils", {0x01, 0x00, 0x13, 0x00, 0x09}, {0x01, 0x02, 0xCD, 0x01}},
      {"read discrete inputs", {0x02, 0x00, 0xC4, 0x00, 0x16}, {0x02, 0x03, 0xAC, 0xDB, 0x35}},
      {"read/write multiple",
       {0x17, 0x00, 0xC8, 0x00, 0x01, 0x01, 0x2C, 0x00, 0x01, 0x02, 0x00, 0x07},
       {0x17, 0x02, 0x12, 0x34}},
      {"exception", {0x03, 0x00, 0xCA, 0x00, 0x02}, {0x83, 0x02}},
  };
  for (const Case &c : cases) {
    this->paired_.clear();
    this->receive_(0x05, c.request);
    this->receive_(0x05, c.response);
    ASSERT_EQ(this->paired_.size(), 1u) << c.name;
    EXPECT_EQ(this->paired_[0].request, c.request) << c.name;
    EXPECT_EQ(this->paired_[0].response, c.response) << c.name;
  }
}

TEST_F(MonitorServer, RequestIsPairedWithItsFirstResponseOnly) {
  this->receive_(0x05, READ);
  this->receive_(0x05, READ_REPLY);
  this->receive_(0x05, READ_REPLY);
  EXPECT_EQ(this->paired_.size(), 1u);
}

// RTU is half duplex: a new request replaces an unanswered one.
TEST_F(MonitorServer, UnansweredPollDoesNotCaptureTheNextDevicesReply) {
  this->receive_(0x05, READ);  // absent device
  this->receive_(0x06, READ_TWO);
  this->receive_(0x06, READ_TWO_REPLY);
  ASSERT_EQ(this->paired_.size(), 1u);
  EXPECT_EQ(this->paired_[0].address, 0x06);
  EXPECT_EQ(this->paired_[0].request, vec(READ_TWO));
}

TEST_F(MonitorServer, ResponseFromAnotherAddressIsNotPaired) {
  this->receive_(0x05, READ);
  this->receive_(0x07, READ_REPLY);
  EXPECT_TRUE(this->paired_.empty());
}

TEST_F(MonitorServer, ResponseWithAnotherFunctionCodeIsNotPaired) {
  const uint8_t input_reply[] = {0x04, 0x02, 0x00, 0x01};
  this->receive_(0x05, READ);
  this->receive_(0x05, input_reply);
  EXPECT_TRUE(this->paired_.empty());
}

// A read reply with no request before it (the hub started mid-exchange) is too short for a request and is
// dropped after the frame timeout; the next exchange pairs normally.
TEST_F(MonitorServer, ResponseSeenFirstIsDropped) {
  this->receive_(0x05, READ_REPLY);
  this->pause_();
  EXPECT_TRUE(this->seen_.empty());
  EXPECT_TRUE(this->paired_.empty());
  this->receive_(0x05, READ);
  this->receive_(0x05, READ_REPLY);
  ASSERT_EQ(this->paired_.size(), 1u);
  EXPECT_EQ(this->paired_[0].request, vec(READ));
}

// With no device of its own the hub never transmits.
TEST_F(MonitorServer, ListenerNeverTransmits) {
  this->receive_(0x05, READ);
  this->receive_(0x05, READ_REPLY);
  this->receive_(0x05, WRITE_1);
  EXPECT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->paired_.size(), 1u);
  EXPECT_TRUE(this->uart_.written.empty());
}

}  // namespace esphome::modbus::testing
