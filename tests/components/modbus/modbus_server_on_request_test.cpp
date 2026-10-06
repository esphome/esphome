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
};

class ServerOnRequest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_uart_parent(&this->uart_);
    this->hub_.setup();
  }
  void listen() {
    this->hub_.add_on_request_callback([this](uint8_t address, std::span<const uint8_t> pdu) {
      this->seen_.push_back({address, {pdu.begin(), pdu.end()}});
    });
  }

  InjectableUART uart_;
  ModbusServerHub hub_;
  std::vector<Request> seen_;
};

constexpr uint8_t READ[] = {0x03, 0x00, 0x10, 0x00, 0x01};
constexpr uint8_t WRITE_1[] = {0x06, 0x00, 0x10, 0x00, 0x01};
constexpr uint8_t WRITE_2[] = {0x06, 0x00, 0x10, 0x00, 0x02};

}  // namespace

TEST_F(ServerOnRequest, RequestToAnUnservedAddressIsHandedOver) {
  this->listen();
  this->uart_.inject_frame(0x05, READ);
  this->hub_.loop();
  ASSERT_EQ(this->seen_.size(), 1u);
  EXPECT_EQ(this->seen_[0].address, 0x05);
  EXPECT_EQ(this->seen_[0].pdu, std::vector<uint8_t>(std::begin(READ), std::end(READ)));
  EXPECT_TRUE(this->uart_.written.empty());
}

// A single write and its reply have the same shape. With a handler set, the second write is not taken
// for a reply from another device on the bus.
TEST_F(ServerOnRequest, TwoSingleWritesInARowAreBothHandedOver) {
  this->listen();
  this->uart_.inject_frame(0x05, WRITE_1);
  this->uart_.inject_frame(0x05, WRITE_2);
  this->hub_.loop();
  ASSERT_EQ(this->seen_.size(), 2u);
  EXPECT_EQ(this->seen_[0].pdu, std::vector<uint8_t>(std::begin(WRITE_1), std::end(WRITE_1)));
  EXPECT_EQ(this->seen_[1].pdu, std::vector<uint8_t>(std::begin(WRITE_2), std::end(WRITE_2)));
}

TEST_F(ServerOnRequest, ServedAddressIsAnsweredHere) {
  RegisterDevice device(0x02);
  this->hub_.register_device(&device);
  this->listen();
  this->uart_.inject_frame(0x02, READ);
  this->hub_.loop();
  EXPECT_TRUE(this->seen_.empty());
  EXPECT_FALSE(this->uart_.written.empty());
}

TEST_F(ServerOnRequest, BroadcastIsNotHandedOver) {
  this->listen();
  this->uart_.inject_frame(BROADCAST_ADDRESS, WRITE_1);
  this->hub_.loop();
  EXPECT_TRUE(this->seen_.empty());
}

}  // namespace esphome::modbus::testing
