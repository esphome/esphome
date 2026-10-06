#ifdef USE_HOST

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "esphome/components/modbus_gateway/modbus_gateway.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus_gateway::testing {

class FakeUart : public uart::UARTComponent {
 public:
  std::vector<uint8_t> tx;
  std::vector<uint8_t> rx;
  size_t room{SIZE_MAX};
  bool connected{true};

  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty()) {
      return false;
    }
    *data = this->rx[0];
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx.size()) {
      return false;
    }
    std::memcpy(data, this->rx.data(), len);
    this->rx.erase(this->rx.begin(), this->rx.begin() + static_cast<std::ptrdiff_t>(len));
    return true;
  }
  size_t available() override { return this->rx.size(); }
  size_t available_for_write() override { return this->room; }
  bool is_connected() override { return this->connected; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  void push(const std::vector<uint8_t> &frame) { this->rx.insert(this->rx.end(), frame.begin(), frame.end()); }

 protected:
  void check_logger_conflict() override {}
};

class TestGateway : public ModbusGateway {
 public:
  using ModbusGateway::run_;
};

std::vector<uint8_t> frame(std::vector<uint8_t> out) {
  uint16_t crc = crc16(out.data(), static_cast<uint16_t>(out.size()));
  out.push_back(crc & 0xFF);
  out.push_back(crc >> 8);
  return out;
}

// FC 0x10 with count registers: 9 + 2 * count bytes.
std::vector<uint8_t> write_registers(uint8_t unit, uint8_t count) {
  std::vector<uint8_t> body{unit, 0x10, 0x00, 0x00, 0x00, count, static_cast<uint8_t>(count * 2)};
  for (uint8_t i = 0; i < count; i++) {
    body.push_back(0x00);
    body.push_back(i);
  }
  return frame(body);
}

class GatewayRoute : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bms_.set_baud_rate(115200);
    this->client_.set_baud_rate(9600);
    this->local_.set_baud_rate(115200);
    this->gate_.set_uart_parent(&this->bms_);
    this->gate_.set_port_count(2);
    this->gate_.set_port_uart(0, &this->client_);
    this->gate_.set_port_local(1, &this->local_);
    this->gate_.set_response_timeout(500);
  }

  // Advances the clock and runs one loop pass.
  void run(uint32_t ms = 0) {
    this->now_ += ms;
    this->gate_.run_(this->now_);
  }
  std::vector<uint8_t> local_reply() {
    std::vector<uint8_t> got(this->local_.available());
    if (!got.empty()) {
      this->local_.read_array(got.data(), got.size());
    }
    return got;
  }

  uint32_t now_{1000};
  FakeUart bms_;
  FakeUart client_;
  GatewayUart local_;
  TestGateway gate_;
};

TEST_F(GatewayRoute, ResponseGoesBackToTheSender) {
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->client_.push(request);
  this->run();
  ASSERT_EQ(this->bms_.tx, request);

  auto response = frame({0x01, 0x03, 0x02, 0x00, 0x64});
  this->bms_.push(response);
  this->run();
  EXPECT_EQ(this->client_.tx, response);
  EXPECT_EQ(this->local_.available(), 0u);
}

TEST_F(GatewayRoute, SecondClientWaitsForTheResponse) {
  auto first = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto second = frame({0x01, 0x03, 0x00, 0x0A, 0x00, 0x01});
  this->client_.push(first);
  this->local_.write_array(second.data(), second.size());
  this->run();
  EXPECT_EQ(this->bms_.tx, first);

  this->bms_.push(frame({0x01, 0x03, 0x02, 0x00, 0x01}));
  this->run();
  ASSERT_EQ(this->bms_.tx.size(), first.size() + second.size());
  EXPECT_EQ(std::vector<uint8_t>(this->bms_.tx.begin() + first.size(), this->bms_.tx.end()), second);
}

TEST_F(GatewayRoute, RepeatedReadGoesToTheBus) {
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto response = frame({0x01, 0x03, 0x02, 0x00, 0x64});
  this->client_.push(request);
  this->run();
  this->bms_.push(response);
  this->run();
  this->bms_.tx.clear();
  this->client_.tx.clear();

  this->client_.push(request);
  this->run();
  EXPECT_EQ(this->bms_.tx, request);
}

TEST_F(GatewayRoute, BadCrcIsNotForwarded) {
  uint8_t junk[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
  this->client_.push(std::vector<uint8_t>(junk, junk + sizeof(junk)));
  this->run();
  EXPECT_TRUE(this->bms_.tx.empty());
}

TEST_F(GatewayRoute, ResponseGoesToThePortThatAsked) {
  FakeUart third;
  FakeUart fourth;
  third.set_baud_rate(9600);
  fourth.set_baud_rate(9600);
  this->gate_.set_port_count(4);
  this->gate_.set_port_uart(2, &third);
  this->gate_.set_port_uart(3, &fourth);

  auto first = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto second = frame({0x02, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto third_request = frame({0x03, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto fourth_request = frame({0x04, 0x03, 0x00, 0x00, 0x00, 0x01});
  const std::vector<uint8_t> requests[] = {first, second, third_request, fourth_request};
  this->client_.push(first);
  this->local_.write_array(second.data(), second.size());
  third.push(third_request);
  fourth.push(fourth_request);

  auto answer = [](uint8_t address) { return frame({address, 0x03, 0x02, 0x00, address}); };
  auto quiet = [&]() {
    EXPECT_TRUE(this->client_.tx.empty());
    EXPECT_EQ(this->local_.available(), 0u);
    EXPECT_TRUE(third.tx.empty());
    EXPECT_TRUE(fourth.tx.empty());
  };
  auto take = [&](int who, const std::vector<uint8_t> &want) {
    if (who == 0) {
      EXPECT_EQ(this->client_.tx, want);
      this->client_.tx.clear();
      return;
    }
    if (who == 1) {
      std::vector<uint8_t> got(this->local_.available());
      ASSERT_EQ(got.size(), want.size());
      ASSERT_TRUE(this->local_.read_array(got.data(), got.size()));
      EXPECT_EQ(got, want);
      return;
    }
    FakeUart *port = who == 2 ? &third : &fourth;
    EXPECT_EQ(port->tx, want);
    port->tx.clear();
  };

  this->run();
  size_t sent = 0;
  for (int i = 0; i < 4; i++) {
    ASSERT_GE(this->bms_.tx.size(), sent + requests[i].size());
    EXPECT_EQ(std::vector<uint8_t>(this->bms_.tx.begin() + static_cast<std::ptrdiff_t>(sent),
                                   this->bms_.tx.begin() + static_cast<std::ptrdiff_t>(sent + requests[i].size())),
              requests[i]);
    sent += requests[i].size();
    // The next port already asked. Its answer must not be given to anyone while this request is open.
    if (i < 3) {
      this->bms_.push(answer(static_cast<uint8_t>(i + 2)));
      this->run();
      EXPECT_EQ(this->bms_.tx.size(), sent);
      quiet();
    }
    this->bms_.push(answer(static_cast<uint8_t>(i + 1)));
    this->run();
    take(i, answer(static_cast<uint8_t>(i + 1)));
    quiet();
  }
  EXPECT_EQ(this->bms_.tx.size(), sent);
}

TEST_F(GatewayRoute, ExceptionGoesToThePortThatAsked) {
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto other = frame({0x02, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->client_.push(request);
  this->local_.write_array(other.data(), other.size());
  this->run();
  auto exception = frame({0x01, 0x83, 0x02});
  this->bms_.push(exception);
  this->run();
  EXPECT_EQ(this->client_.tx, exception);
  EXPECT_EQ(this->local_.available(), 0u);
}

TEST_F(GatewayRoute, BroadcastIsNotAnsweredAndHoldsTheBus) {
  auto broadcast = frame({0x00, 0x06, 0x00, 0x01, 0x00, 0x01});
  auto next = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->client_.push(broadcast);
  this->local_.write_array(next.data(), next.size());
  this->run();
  EXPECT_EQ(this->bms_.tx, broadcast);

  // A broadcast is not answered. A frame that arrives anyway is dropped.
  this->bms_.push(frame({0x01, 0x03, 0x02, 0x00, 0x01}));
  this->run(50);
  EXPECT_EQ(this->bms_.tx, broadcast);
  EXPECT_TRUE(this->client_.tx.empty());
  EXPECT_EQ(this->local_.available(), 0u);

  this->run(600);
  ASSERT_EQ(this->bms_.tx.size(), broadcast.size() + next.size());
  EXPECT_EQ(
      std::vector<uint8_t>(this->bms_.tx.begin() + static_cast<std::ptrdiff_t>(broadcast.size()), this->bms_.tx.end()),
      next);
  EXPECT_EQ(this->local_.available(), 0u);
}

TEST_F(GatewayRoute, ReadWithAnotherByteCountIsNotDelivered) {
  auto request = frame({0x01, 0x01, 0x00, 0x00, 0x00, 0x0A});
  this->client_.push(request);
  this->run();
  auto short_reply = frame({0x01, 0x01, 0x01, 0xFF});
  auto reply = frame({0x01, 0x01, 0x02, 0xFF, 0x03});
  this->bms_.push(short_reply);
  this->bms_.push(reply);
  this->run();
  EXPECT_EQ(this->client_.tx, reply);
}

// A slow server answers the first request after the timeout. The answer must not reach the second port.
TEST_F(GatewayRoute, LateResponseIsDroppedWhileTheBusIsClosed) {
  auto first = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x02});
  auto second = frame({0x01, 0x03, 0x00, 0x0A, 0x00, 0x01});
  this->client_.push(first);
  this->local_.write_array(second.data(), second.size());
  this->run();
  ASSERT_EQ(this->bms_.tx, first);

  this->run(500);
  this->bms_.push(frame({0x01, 0x03, 0x04, 0x11, 0x11, 0x22, 0x22}));
  this->run(10);
  this->bms_.push(frame({0x01, 0x03, 0x04, 0x11, 0x11, 0x22, 0x22}));
  this->run(20);
  EXPECT_EQ(this->bms_.tx, first);
  EXPECT_TRUE(this->client_.tx.empty());
  EXPECT_EQ(this->local_.available(), 0u);

  // One 256-byte frame at 115200 baud is 23 ms of quiet.
  this->run(23);
  ASSERT_EQ(this->bms_.tx.size(), first.size() + second.size());
  auto reply = frame({0x01, 0x03, 0x02, 0x00, 0xAA});
  this->bms_.push(reply);
  this->run();
  EXPECT_EQ(this->local_reply(), reply);
  EXPECT_TRUE(this->client_.tx.empty());
}

// The late answer comes after the next request is on the bus. Its byte count gives it away.
TEST_F(GatewayRoute, LateResponseAfterTheNextRequestIsDropped) {
  auto first = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x02});
  auto second = frame({0x01, 0x03, 0x00, 0x0A, 0x00, 0x01});
  this->client_.push(first);
  this->local_.write_array(second.data(), second.size());
  this->run();
  this->run(500);
  this->run(23);
  ASSERT_EQ(this->bms_.tx.size(), first.size() + second.size());

  auto reply = frame({0x01, 0x03, 0x02, 0x00, 0xAA});
  this->bms_.push(frame({0x01, 0x03, 0x04, 0x11, 0x11, 0x22, 0x22}));
  this->bms_.push(reply);
  this->run();
  EXPECT_EQ(this->local_reply(), reply);
  EXPECT_TRUE(this->client_.tx.empty());
}

// The client polls again before its turn. Only the latest request goes to the bus.
TEST_F(GatewayRoute, NewestRequestWins) {
  auto busy = frame({0x02, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->local_.write_array(busy.data(), busy.size());
  this->run();
  ASSERT_EQ(this->bms_.tx, busy);

  auto old_request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto new_request = frame({0x01, 0x03, 0x00, 0x05, 0x00, 0x01});
  this->client_.push(old_request);
  this->run();
  this->client_.push(new_request);
  this->run();
  this->bms_.push(frame({0x02, 0x03, 0x02, 0x00, 0x01}));
  this->run();
  ASSERT_EQ(this->bms_.tx.size(), busy.size() + new_request.size());
  EXPECT_EQ(std::vector<uint8_t>(this->bms_.tx.begin() + static_cast<std::ptrdiff_t>(busy.size()), this->bms_.tx.end()),
            new_request);
}

// A long request does not fit behind the waiting one. It takes its room; the rest waits in the client's UART.
TEST_F(GatewayRoute, LongNewerRequestTakesTheRoomOfTheWaitingOne) {
  auto busy = frame({0x02, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->local_.write_array(busy.data(), busy.size());
  this->run();
  auto waiting = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->client_.push(waiting);
  this->run();

  auto long_request = write_registers(0x01, 123);
  ASSERT_EQ(long_request.size(), 255u);
  this->client_.push(long_request);
  this->run();
  EXPECT_EQ(this->client_.rx.size(), 7u);
  this->run();
  EXPECT_TRUE(this->client_.rx.empty());

  this->bms_.push(frame({0x02, 0x03, 0x02, 0x00, 0x01}));
  this->run();
  ASSERT_EQ(this->bms_.tx.size(), busy.size() + long_request.size());
  EXPECT_EQ(std::vector<uint8_t>(this->bms_.tx.begin() + static_cast<std::ptrdiff_t>(busy.size()), this->bms_.tx.end()),
            long_request);
}

// A local port reports the room its buffer has, and a flush is not confirmed while its request waits.
TEST_F(GatewayRoute, LocalPortReportsItsRoom) {
  auto busy = frame({0x02, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->client_.push(busy);
  this->run();
  EXPECT_EQ(this->local_.available_for_write(), MAX_FRAME);
  EXPECT_EQ(this->local_.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS);

  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->local_.write_array(request.data(), request.size());
  EXPECT_EQ(this->local_.available_for_write(), MAX_FRAME - request.size());
  EXPECT_EQ(this->local_.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT);

  this->bms_.push(frame({0x02, 0x03, 0x02, 0x00, 0x01}));
  this->run();
  EXPECT_EQ(this->local_.available_for_write(), MAX_FRAME);
  EXPECT_EQ(this->local_.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS);
}

// The client left before its answer came. The answer is dropped and the next request goes out without a timeout.
TEST_F(GatewayRoute, PortThatIsGoneDoesNotHoldTheBus) {
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto next = frame({0x02, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->client_.push(request);
  this->local_.write_array(next.data(), next.size());
  this->run();
  ASSERT_EQ(this->bms_.tx, request);

  this->client_.connected = false;
  this->bms_.push(frame({0x01, 0x03, 0x02, 0x00, 0x01}));
  this->run();
  EXPECT_TRUE(this->client_.tx.empty());
  ASSERT_EQ(this->bms_.tx.size(), request.size() + next.size());
  auto reply = frame({0x02, 0x03, 0x02, 0x00, 0x02});
  this->bms_.push(reply);
  this->run();
  EXPECT_EQ(this->local_reply(), reply);
}

// The hub on the local port did not read its last answer. The next one does not fit and is dropped.
TEST_F(GatewayRoute, FullLocalPortDoesNotHoldTheBus) {
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x7D});
  std::vector<uint8_t> body{0x01, 0x03, 0xFA};
  body.resize(3 + 0xFA, 0x11);
  auto reply = frame(body);
  this->local_.write_array(request.data(), request.size());
  this->run();
  this->bms_.push(reply);
  this->run();
  ASSERT_EQ(this->local_.available(), reply.size());

  this->local_.write_array(request.data(), request.size());
  this->run();
  ASSERT_EQ(this->bms_.tx.size(), 2 * request.size());
  this->bms_.push(reply);
  this->run();
  EXPECT_EQ(this->local_.available(), reply.size());

  auto other = frame({0x02, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->client_.push(other);
  this->run();
  EXPECT_EQ(this->bms_.tx.size(), 2 * request.size() + other.size());
}

}  // namespace esphome::modbus_gateway::testing

#endif  // USE_HOST
