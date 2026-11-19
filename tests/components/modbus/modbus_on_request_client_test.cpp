#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus::testing {

namespace {

// Drives the send step, the timeout and the sweep without waiting for real time.
class MonitorClientHub : public ModbusClientHub {
 public:
  void send_next_for_test() {
    this->send_next_frame_();
    this->sweep_();
  }
  void timeout_for_test() {
    this->expire_waiting_();
    this->sweep_();
  }
  bool waiting() const { return this->waiting_for_response_; }
  void receive_for_test(uint8_t address, std::span<const uint8_t> pdu) {
    this->process_modbus_server_frame(address, pdu);
  }
  size_t entries() const { return this->tx_buffer_.size(); }
  bool tx_blocked() override { return this->blocked || this->ModbusClientHub::tx_blocked(); }
  bool blocked{false};
};

// Asks for one retry when no reply comes.
class RetryOnceDevice : public ModbusClientDevice {
 public:
  using ModbusClientDevice::ModbusClientDevice;
  bool on_no_response(std::span<const uint8_t> request_pdu) override { return this->retries_++ == 0; }
  int retries_{0};
};

struct Request {
  uint8_t address;
  std::vector<uint8_t> pdu;
};

struct Response {
  uint8_t address;
  std::vector<uint8_t> request;
  std::vector<uint8_t> response;
};

// Notes how many requests the monitor had seen each time on_sent runs.
class SentDevice : public ModbusClientDevice {
 public:
  SentDevice(ModbusClientHub *hub, uint8_t address, const std::vector<Request> &seen)
      : ModbusClientDevice(hub, address), seen_(seen) {}
  void on_sent(std::span<const uint8_t> request_pdu) override { this->seen_at_sent.push_back(this->seen_.size()); }
  std::vector<size_t> seen_at_sent;

 protected:
  const std::vector<Request> &seen_;
};

// Notes how many responses the monitor had seen each time on_response or on_error runs.
class ResponseDevice : public ModbusClientDevice {
 public:
  ResponseDevice(ModbusClientHub *hub, uint8_t address, const std::vector<Response> &paired)
      : ModbusClientDevice(hub, address), paired_(paired) {}
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->paired_at_terminal.push_back(this->paired_.size());
  }
  void on_error(std::span<const uint8_t> request_pdu, ExceptionCode exception_code) override {
    this->paired_at_terminal.push_back(this->paired_.size());
  }
  std::vector<size_t> paired_at_terminal;

 protected:
  const std::vector<Response> &paired_;
};

class MonitorClient : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_uart_parent(&this->uart_);
    this->hub_.setup();
    this->hub_.add_on_request_callback(
        [this](uint8_t address, std::span<const uint8_t> pdu) { this->seen_.push_back({address, vec(pdu)}); });
    this->hub_.add_on_response_callback(
        [this](uint8_t address, std::span<const uint8_t> request, std::span<const uint8_t> response) {
          this->paired_.push_back({address, vec(request), vec(response)});
        });
  }

  NullUART uart_;
  MonitorClientHub hub_;
  std::vector<Request> seen_;
  std::vector<Response> paired_;
};

constexpr uint8_t READ[] = {0x03, 0x01, 0x00, 0x00, 0x02};
constexpr uint8_t WRITE[] = {0x06, 0x00, 0x10, 0x00, 0x01};

}  // namespace

TEST_F(MonitorClient, SeenWhenSentNotWhenQueued) {
  ASSERT_TRUE(this->hub_.queue_pdu(0x02, READ));
  EXPECT_TRUE(this->seen_.empty());
  this->hub_.send_next_for_test();
  ASSERT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(this->seen_[0].address, 0x02);
  EXPECT_EQ(this->seen_[0].pdu, vec(READ));
  EXPECT_TRUE(this->hub_.waiting());
}

// The monitor runs at the end of the send step: after the hub has set its state and after the device's on_sent.
TEST_F(MonitorClient, RequestIsSeenAfterOnSent) {
  bool waiting_at_request = false;
  this->hub_.add_on_request_callback(
      [this, &waiting_at_request](uint8_t, std::span<const uint8_t>) { waiting_at_request = this->hub_.waiting(); });
  SentDevice device(&this->hub_, 0x02, this->seen_);
  ASSERT_TRUE(device.queue_pdu(READ));
  this->hub_.send_next_for_test();
  EXPECT_TRUE(waiting_at_request);
  ASSERT_EQ(device.seen_at_sent.size(), 1u);
  EXPECT_EQ(device.seen_at_sent[0], 0u);
}

TEST_F(MonitorClient, RetryIsSeenAgain) {
  RetryOnceDevice device(&this->hub_, 0x02);
  ASSERT_TRUE(device.queue_pdu(READ));
  this->hub_.send_next_for_test();
  this->hub_.timeout_for_test();  // no reply: the device asks for a retry
  this->hub_.send_next_for_test();
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].pdu, this->seen_[1].pdu);
}

TEST_F(MonitorClient, BroadcastIsSeen) {
  ASSERT_TRUE(this->hub_.queue_pdu(BROADCAST_ADDRESS, WRITE));
  this->hub_.send_next_for_test();
  ASSERT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(this->seen_[0].address, BROADCAST_ADDRESS);
  EXPECT_FALSE(this->hub_.waiting());
}

// The broadcast is already complete when the callback runs, so the same request queued from the
// callback is a new entry and is sent, not merged into the finished one.
TEST_F(MonitorClient, RequestQueuedFromTheCallbackIsSent) {
  bool queued = false;
  this->hub_.add_on_request_callback([this, &queued](uint8_t address, std::span<const uint8_t> pdu) {
    if (!queued)
      queued = this->hub_.queue_pdu(address, pdu);
  });
  ASSERT_TRUE(this->hub_.queue_pdu(BROADCAST_ADDRESS, WRITE));
  this->hub_.send_next_for_test();
  EXPECT_TRUE(queued);
  EXPECT_EQ(this->hub_.entries(), 1u);
  this->hub_.send_next_for_test();
  EXPECT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->hub_.entries(), 0u);
}

// The entry is consumed before the monitor runs, so the same write queued from on_response is accepted.
TEST_F(MonitorClient, SameRequestQueuedFromOnResponseIsAccepted) {
  ModbusClientDevice device(&this->hub_, 0x02);
  bool queued = false;
  this->hub_.add_on_response_callback(
      [&device, &queued](uint8_t, std::span<const uint8_t> request, std::span<const uint8_t>) {
        if (!queued)
          queued = device.queue_pdu(request);
      });
  ASSERT_TRUE(device.queue_pdu(WRITE));
  this->hub_.send_next_for_test();
  this->hub_.receive_for_test(0x02, WRITE);  // a write single register reply echoes the request
  EXPECT_TRUE(queued);
}

TEST_F(MonitorClient, NotSentIsNotSeen) {
  this->hub_.blocked = true;
  ASSERT_TRUE(this->hub_.queue_pdu(0x02, READ));
  this->hub_.send_next_for_test();
  EXPECT_TRUE(this->seen_.empty());
}

// A normal reply and an exception reply are both reported with their request.
TEST_F(MonitorClient, ReplyIsSeenWithItsRequest) {
  const std::vector<uint8_t> replies[] = {{0x03, 0x04, 0x00, 0x2A, 0x00, 0x2B}, {0x83, 0x02}};
  ModbusClientDevice device(&this->hub_, 0x02);
  for (const auto &reply : replies) {
    ASSERT_TRUE(device.queue_pdu(READ));
    this->hub_.send_next_for_test();
    this->hub_.receive_for_test(0x02, reply);
    this->hub_.send_next_for_test();  // nothing to send: sweeps the answered entry
  }
  ASSERT_EQ(this->paired_.size(), 2u);
  for (size_t i = 0; i < 2; i++) {
    EXPECT_EQ(this->paired_[i].address, 0x02);
    EXPECT_EQ(this->paired_[i].request, vec(READ));
    EXPECT_EQ(this->paired_[i].response, replies[i]);
  }
}

// The monitor runs after the device's on_response and on_error, for a normal and an exception reply.
TEST_F(MonitorClient, ResponseIsSeenAfterTheDevice) {
  const std::vector<uint8_t> replies[] = {{0x03, 0x04, 0x00, 0x2A, 0x00, 0x2B}, {0x83, 0x02}};
  ResponseDevice device(&this->hub_, 0x02, this->paired_);
  for (const auto &reply : replies) {
    ASSERT_TRUE(device.queue_pdu(READ));
    this->hub_.send_next_for_test();
    this->hub_.receive_for_test(0x02, reply);
    this->hub_.send_next_for_test();  // nothing to send: sweeps the answered entry
  }
  ASSERT_EQ(device.paired_at_terminal.size(), 2u);
  EXPECT_EQ(device.paired_at_terminal[0], 0u);
  EXPECT_EQ(device.paired_at_terminal[1], 1u);
  EXPECT_EQ(this->paired_.size(), 2u);
}

// A request queued on the hub with no device still has its reply reported, as a raw send with a monitor needs.
TEST_F(MonitorClient, ReplyToADeviceLessRequestIsSeen) {
  const std::vector<uint8_t> reply = {0x03, 0x04, 0x00, 0x2A, 0x00, 0x2B};
  ASSERT_TRUE(this->hub_.queue_pdu(0x02, READ));
  this->hub_.send_next_for_test();
  this->hub_.receive_for_test(0x02, reply);
  this->hub_.send_next_for_test();
  ASSERT_EQ(this->paired_.size(), 1u);
  EXPECT_EQ(this->paired_[0].address, 0x02);
  EXPECT_EQ(this->paired_[0].request, vec(READ));
  EXPECT_EQ(this->paired_[0].response, reply);
  EXPECT_EQ(this->hub_.entries(), 0u);
}

// A reply from another address interrupts the wait: the expected reply after it is ignored and the wire stays blocked.
TEST_F(MonitorClient, ReplyAfterAnInterruptionIsNotSeen) {
  ModbusClientDevice device(&this->hub_, 0x02);
  ASSERT_TRUE(device.queue_pdu(READ));
  this->hub_.send_next_for_test();
  const uint8_t reply[] = {0x03, 0x04, 0x00, 0x2A, 0x00, 0x2B};
  this->hub_.receive_for_test(0x05, reply);
  this->hub_.receive_for_test(0x02, reply);
  EXPECT_TRUE(this->paired_.empty());
  EXPECT_TRUE(this->hub_.waiting());
}

}  // namespace esphome::modbus::testing
