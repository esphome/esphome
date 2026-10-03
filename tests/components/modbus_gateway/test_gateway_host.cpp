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

TEST_F(GatewayRoute, BadCrcIsNotForwarded) {
  uint8_t junk[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
  this->master_.push(std::vector<uint8_t>(junk, junk + sizeof(junk)));
  this->gate_.loop();
  EXPECT_TRUE(this->bms_.tx.empty());
}

TEST_F(GatewayRoute, BroadcastDoesNotWaitForAResponse) {
  auto broadcast = frame({0x00, 0x06, 0x00, 0x01, 0x00, 0x01});
  auto next = frame({0x01, 0x03, 0x00, 0x00, 0x00, 0x01});
  this->master_.push(broadcast);
  this->local_.write_array(next.data(), next.size());
  this->gate_.loop();
  EXPECT_EQ(this->bms_.tx, broadcast);
  usleep(20000);
  this->gate_.loop();
  ASSERT_EQ(this->bms_.tx.size(), broadcast.size() + next.size());
  EXPECT_EQ(
      std::vector<uint8_t>(this->bms_.tx.begin() + static_cast<std::ptrdiff_t>(broadcast.size()), this->bms_.tx.end()),
      next);
}

}  // namespace
}  // namespace esphome::modbus_gateway

#endif  // USE_HOST
