#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/teleinfo/teleinfo.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::teleinfo::testing {

namespace {
// Historical mode frames with one group: STX, LF, tag, space, value, space, checksum, CR, ETX.
// The checksum is the sum of "PAPP 00500" (0x246), cut to 6 bits (0x06), plus 0x20: '&'.
const char *const FRAME_500_VA = "\x02\nPAPP 00500 &\r\x03";
// "PAPP 00750" sums to 0x24D; 0x0D plus 0x20 gives '-'.
const char *const FRAME_750_VA = "\x02\nPAPP 00750 -\r\x03";

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;

  void push_rx(const char *data) { this->rx.insert(this->rx.end(), data, data + strlen(data)); }

  // UARTComponent
  void write_array(const uint8_t *data, size_t len) override {}

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len) {
      return false;
    }

    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }

  size_t available() override { return this->rx.size(); }

  MOCK_METHOD(bool, peek_byte, (uint8_t *data), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif  // defined(USE_ESP8266) || defined(USE_ESP32)
};

// Records every PAPP value the hub hands to its listeners.
class PappListener : public TeleInfoListener {
 public:
  PappListener() { this->tag = "PAPP"; }
  void publish_val(const std::string &val) override { this->values.push_back(val); }

  std::vector<std::string> values;
};

class TeleInfoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->teleinfo_.set_uart_parent(&this->uart_);
    this->teleinfo_.register_teleinfo_listener(&this->listener_);
    this->teleinfo_.setup();
  }

  MockUARTComponent uart_;
  TeleInfo teleinfo_{true};  // historical mode
  PappListener listener_;
};
}  // namespace

// The meter sends without pause. A frame that arrives between two updates is old by the next update,
// so it is dropped and the update publishes the frame that starts after it.
TEST_F(TeleInfoTest, DropsFrameSentBetweenUpdates) {
  this->uart_.push_rx(FRAME_500_VA);
  this->teleinfo_.loop();  // Before the update: drops the frame.

  this->teleinfo_.update();
  this->uart_.push_rx(FRAME_750_VA);
  this->teleinfo_.loop();  // Finds the start byte 0x02.
  this->teleinfo_.loop();  // Reads up to the end byte 0x03.
  this->teleinfo_.loop();  // Decodes the frame and publishes PAPP.

  EXPECT_THAT(this->listener_.values, ::testing::ElementsAre("00750"));
}

}  // namespace esphome::teleinfo::testing
