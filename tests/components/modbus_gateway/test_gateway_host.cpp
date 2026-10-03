#ifdef USE_HOST

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <vector>

#include "esphome/components/modbus_gateway/modbus_gateway.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus_gateway {
namespace {

class FakeUart : public uart::UARTComponent {
 public:
  std::vector<uint8_t> tx;
  std::vector<uint8_t> rx;

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
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  void push(const std::vector<uint8_t> &frame) { this->rx.insert(this->rx.end(), frame.begin(), frame.end()); }

 protected:
  void check_logger_conflict() override {}
};

std::vector<uint8_t> frame(std::initializer_list<uint8_t> body) {
  std::vector<uint8_t> out(body);
  uint16_t crc = crc16(out.data(), static_cast<uint16_t>(out.size()));
  out.push_back(crc & 0xFF);
  out.push_back(crc >> 8);
  return out;
}

class GatewayRoute : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bms_.set_baud_rate(115200);
    this->master_.set_baud_rate(9600);
    this->local_.set_baud_rate(115200);
    this->gate_.set_uart_parent(&this->bms_);
    this->gate_.set_port_count(2);
    this->gate_.set_port_uart(0, &this->master_);
    this->gate_.set_port_local(1, &this->local_);
    this->gate_.set_response_timeout(500);
    this->gate_.set_cache_time(10000);
    this->gate_.set_cache_entries(16);
    this->gate_.set_port_cache(0, true);
    this->gate_.set_port_cache(1, true);
  }

  FakeUart bms_;
  FakeUart master_;
  GatewayUart local_;
  ModbusGateway gate_;
};

TEST_F(GatewayRoute, ResponseGoesBackToTheSender) {
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->master_.push(request);
  this->gate_.loop();
  ASSERT_EQ(this->bms_.tx, request);

  auto response = frame({0x01, 0x03, 0x02, 0x00, 0x64});
  this->bms_.push(response);
  this->gate_.loop();
  EXPECT_EQ(this->master_.tx, response);
  EXPECT_EQ(this->local_.available(), 0u);
}

TEST_F(GatewayRoute, SecondMasterWaitsForTheResponse) {
  auto first = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto second = frame({0x01, 0x03, 0x00, 0x0A, 0x00, 0x01});
  this->master_.push(first);
  this->local_.write_array(second.data(), second.size());
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, first);

  this->bms_.push(frame({0x01, 0x03, 0x02, 0x00, 0x01}));
  this->gate_.loop();
  ASSERT_EQ(this->bms_.tx.size(), first.size() + second.size());
  EXPECT_EQ(std::vector<uint8_t>(this->bms_.tx.begin() + first.size(), this->bms_.tx.end()), second);
}

TEST_F(GatewayRoute, RepeatedReadUsesTheCache) {
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto response = frame({0x01, 0x03, 0x02, 0x00, 0x64});
  this->master_.push(request);
  this->gate_.loop();
  this->bms_.push(response);
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.tx.clear();

  this->master_.push(request);
  this->gate_.loop();
  EXPECT_TRUE(this->bms_.tx.empty());
  EXPECT_EQ(this->master_.tx, response);
}

TEST_F(GatewayRoute, WriteClearsTheCache) {
  auto read = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->master_.push(read);
  this->gate_.loop();
  this->bms_.push(frame({0x01, 0x03, 0x02, 0x00, 0x64}));
  this->gate_.loop();

  auto write = frame({0x01, 0x06, 0x00, 0x10, 0x00, 0x01});
  this->bms_.tx.clear();
  this->master_.push(write);
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, write);

  this->bms_.push(frame({0x01, 0x06, 0x00, 0x10, 0x00, 0x01}));
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.push(read);
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, read);
}

TEST_F(GatewayRoute, DifferentReadsStayCached) {
  auto first = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto second = frame({0x01, 0x03, 0x00, 0x0A, 0x00, 0x01});
  auto first_response = frame({0x01, 0x03, 0x02, 0x00, 0x01});
  auto second_response = frame({0x01, 0x03, 0x02, 0x00, 0x02});
  this->master_.push(first);
  this->gate_.loop();
  this->bms_.push(first_response);
  this->gate_.loop();
  this->master_.push(second);
  this->gate_.loop();
  this->bms_.push(second_response);
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.tx.clear();

  this->master_.push(first);
  this->gate_.loop();
  EXPECT_TRUE(this->bms_.tx.empty());
  EXPECT_EQ(this->master_.tx, first_response);
}

TEST_F(GatewayRoute, UncachedPortRefreshesWhatOthersRead) {
  this->gate_.set_port_cache(0, false);
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->master_.push(request);
  this->gate_.loop();
  this->bms_.push(frame({0x01, 0x03, 0x02, 0x00, 0x64}));
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.tx.clear();

  this->master_.push(request);
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, request);
  auto fresh = frame({0x01, 0x03, 0x02, 0x00, 0x65});
  this->bms_.push(fresh);
  this->gate_.loop();
  this->bms_.tx.clear();

  this->local_.write_array(request.data(), request.size());
  this->gate_.loop();
  EXPECT_TRUE(this->bms_.tx.empty());
  std::vector<uint8_t> got(this->local_.available());
  ASSERT_FALSE(got.empty());
  ASSERT_TRUE(this->local_.read_array(got.data(), got.size()));
  EXPECT_EQ(got, fresh);
}

TEST_F(GatewayRoute, ExceptionIsNotCachedAndALongReadIs) {
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->master_.push(request);
  this->gate_.loop();
  this->bms_.push(frame({0x01, 0x83, 0x02}));
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.tx.clear();
  this->master_.push(request);
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, request);
  this->bms_.push(frame({0x01, 0x03, 0x02, 0x00, 0x01}));
  this->gate_.loop();

  auto wide = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x3E});
  std::vector<uint8_t> response{0x01, 0x03, 124};
  response.insert(response.end(), 124, 0x11);
  uint16_t crc = crc16(response.data(), static_cast<uint16_t>(response.size()));
  response.push_back(static_cast<uint8_t>(crc & 0xFF));
  response.push_back(static_cast<uint8_t>(crc >> 8));
  ASSERT_GT(response.size(), 128u);
  this->bms_.tx.clear();
  this->master_.push(wide);
  this->gate_.loop();
  this->bms_.push(response);
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.tx.clear();
  this->master_.push(wide);
  this->gate_.loop();
  EXPECT_TRUE(this->bms_.tx.empty());
}

TEST_F(GatewayRoute, LeastRecentlyUsedEntryIsDropped) {
  this->gate_.set_cache_entries(2);
  auto first = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto second = frame({0x01, 0x03, 0x00, 0x0A, 0x00, 0x01});
  auto third = frame({0x01, 0x03, 0x00, 0x14, 0x00, 0x01});
  auto reply = frame({0x01, 0x03, 0x02, 0x00, 0x01});
  this->master_.push(first);
  this->gate_.loop();
  this->bms_.push(reply);
  this->gate_.loop();
  this->master_.push(second);
  this->gate_.loop();
  this->bms_.push(reply);
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.tx.clear();
  this->master_.push(first);
  this->gate_.loop();
  EXPECT_TRUE(this->bms_.tx.empty());
  this->master_.tx.clear();
  this->master_.push(third);
  this->gate_.loop();
  this->bms_.push(reply);
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.tx.clear();
  this->master_.push(first);
  this->gate_.loop();
  EXPECT_TRUE(this->bms_.tx.empty());
  this->master_.push(second);
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, second);
}

TEST_F(GatewayRoute, NoEntriesDoesNotAnswerFromTheCache) {
  this->gate_.set_cache_entries(0);
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto response = frame({0x01, 0x03, 0x02, 0x00, 0x64});
  this->master_.push(request);
  this->gate_.loop();
  this->bms_.push(response);
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.tx.clear();
  this->master_.push(request);
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, request);
}

TEST_F(GatewayRoute, CachedReadExpires) {
  this->gate_.set_cache_time(1);
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto response = frame({0x01, 0x03, 0x02, 0x00, 0x64});
  this->master_.push(request);
  this->gate_.loop();
  this->bms_.push(response);
  this->gate_.loop();
  this->bms_.tx.clear();
  this->master_.tx.clear();
  usleep(20000);
  this->master_.push(request);
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, request);
}

TEST_F(GatewayRoute, BadCrcIsNotForwarded) {
  uint8_t junk[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
  this->master_.push(std::vector<uint8_t>(junk, junk + sizeof(junk)));
  this->gate_.loop();
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
  this->master_.push(first);
  this->local_.write_array(second.data(), second.size());
  third.push(third_request);
  fourth.push(fourth_request);

  auto answer = [](uint8_t address) { return frame({address, 0x03, 0x02, 0x00, address}); };
  auto quiet = [&]() {
    EXPECT_TRUE(this->master_.tx.empty());
    EXPECT_EQ(this->local_.available(), 0u);
    EXPECT_TRUE(third.tx.empty());
    EXPECT_TRUE(fourth.tx.empty());
  };
  auto take = [&](int who, const std::vector<uint8_t> &want) {
    if (who == 0) {
      EXPECT_EQ(this->master_.tx, want);
      this->master_.tx.clear();
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

  this->gate_.loop();
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
      this->gate_.loop();
      EXPECT_EQ(this->bms_.tx.size(), sent);
      quiet();
    }
    this->bms_.push(answer(static_cast<uint8_t>(i + 1)));
    this->gate_.loop();
    take(i, answer(static_cast<uint8_t>(i + 1)));
    quiet();
  }
  EXPECT_EQ(this->bms_.tx.size(), sent);
}

TEST_F(GatewayRoute, ExceptionGoesToThePortThatAsked) {
  auto request = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  auto other = frame({0x02, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->master_.push(request);
  this->local_.write_array(other.data(), other.size());
  this->gate_.loop();
  auto exception = frame({0x01, 0x83, 0x02});
  this->bms_.push(exception);
  this->gate_.loop();
  EXPECT_EQ(this->master_.tx, exception);
  EXPECT_EQ(this->local_.available(), 0u);
}

TEST_F(GatewayRoute, BroadcastIsNotAnsweredAndHoldsTheBus) {
  auto broadcast = frame({0x00, 0x06, 0x00, 0x01, 0x00, 0x01});
  auto next = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->master_.push(broadcast);
  this->local_.write_array(next.data(), next.size());
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, broadcast);

  // A slave must not answer a broadcast. A frame that arrives anyway is dropped.
  this->bms_.push(frame({0x01, 0x03, 0x02, 0x00, 0x01}));
  usleep(50000);
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, broadcast);
  EXPECT_TRUE(this->master_.tx.empty());
  EXPECT_EQ(this->local_.available(), 0u);

  usleep(600000);
  this->gate_.loop();
  ASSERT_EQ(this->bms_.tx.size(), broadcast.size() + next.size());
  EXPECT_EQ(
      std::vector<uint8_t>(this->bms_.tx.begin() + static_cast<std::ptrdiff_t>(broadcast.size()), this->bms_.tx.end()),
      next);
  EXPECT_EQ(this->local_.available(), 0u);
}

}  // namespace
}  // namespace esphome::modbus_gateway

#endif  // USE_HOST
