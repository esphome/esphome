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
};

class BlockedClientHub : public MonitorClientHub {
 public:
  bool tx_blocked() override { return true; }
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

class MonitorClient : public ::testing::Test {
 protected:
  void SetUp() override { this->attach_(this->hub_); }
  void attach_(MonitorClientHub &hub) {
    hub.set_uart_parent(&this->uart_);
    hub.setup();
    hub.add_on_request_callback([this](uint8_t address, std::span<const uint8_t> pdu) {
      this->seen_.push_back({address, {pdu.begin(), pdu.end()}});
    });
  }

  NullUART uart_;
  MonitorClientHub hub_;
  std::vector<Request> seen_;
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
  EXPECT_EQ(this->seen_[0].pdu, std::vector<uint8_t>(std::begin(READ), std::end(READ)));
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

TEST(MonitorClientBlocked, NotSentIsNotSeen) {
  NullUART uart;
  BlockedClientHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  int seen = 0;
  hub.add_on_request_callback([&seen](uint8_t, std::span<const uint8_t>) { seen++; });
  ASSERT_TRUE(hub.queue_pdu(0x02, READ));
  hub.send_next_for_test();
  EXPECT_EQ(seen, 0);
}

TEST_F(MonitorClient, ResponseIsSeenWithItsRequest) {
  std::vector<uint8_t> request, response;
  uint8_t from = 0;
  this->hub_.add_on_response_callback(
      [&](uint8_t address, std::span<const uint8_t> req, std::span<const uint8_t> resp) {
        from = address;
        request.assign(req.begin(), req.end());
        response.assign(resp.begin(), resp.end());
      });
  ModbusClientDevice device(&this->hub_, 0x02);
  ASSERT_TRUE(device.queue_pdu(READ));
  this->hub_.send_next_for_test();
  const uint8_t reply[] = {0x03, 0x04, 0x00, 0x2A, 0x00, 0x2B};
  this->hub_.receive_for_test(0x02, reply);
  EXPECT_EQ(from, 0x02);
  EXPECT_EQ(request, std::vector<uint8_t>(std::begin(READ), std::end(READ)));
  EXPECT_EQ(response, std::vector<uint8_t>(std::begin(reply), std::end(reply)));
}

TEST_F(MonitorClient, ExceptionIsSeenWithItsRequest) {
  std::vector<uint8_t> request, response;
  this->hub_.add_on_response_callback([&](uint8_t, std::span<const uint8_t> req, std::span<const uint8_t> resp) {
    request.assign(req.begin(), req.end());
    response.assign(resp.begin(), resp.end());
  });
  ModbusClientDevice device(&this->hub_, 0x02);
  ASSERT_TRUE(device.queue_pdu(READ));
  this->hub_.send_next_for_test();
  const uint8_t reply[] = {0x83, 0x02};
  this->hub_.receive_for_test(0x02, reply);
  EXPECT_EQ(request, std::vector<uint8_t>(std::begin(READ), std::end(READ)));
  EXPECT_EQ(response, std::vector<uint8_t>(std::begin(reply), std::end(reply)));
}

// A reply from another address interrupts the wait: the expected reply after it is ignored and the wire stays blocked.
TEST_F(MonitorClient, ReplyAfterAnInterruptionIsNotSeen) {
  int seen = 0;
  this->hub_.add_on_response_callback([&seen](uint8_t, std::span<const uint8_t>, std::span<const uint8_t>) { seen++; });
  ModbusClientDevice device(&this->hub_, 0x02);
  ASSERT_TRUE(device.queue_pdu(READ));
  this->hub_.send_next_for_test();
  const uint8_t reply[] = {0x03, 0x04, 0x00, 0x2A, 0x00, 0x2B};
  this->hub_.receive_for_test(0x05, reply);
  this->hub_.receive_for_test(0x02, reply);
  EXPECT_EQ(seen, 0);
  EXPECT_TRUE(this->hub_.waiting());
}

}  // namespace esphome::modbus::testing
