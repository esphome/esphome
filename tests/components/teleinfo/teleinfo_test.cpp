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
  // Bytes the meter sends while the hub reads: each byte read lets one of them arrive in rx.
  std::vector<uint8_t> incoming;

  void push_rx(const char *data) { this->rx.insert(this->rx.end(), data, data + strlen(data)); }
  void push_incoming(const char *data) { this->incoming.insert(this->incoming.end(), data, data + strlen(data)); }

  // UARTComponent
  void write_array(const uint8_t *data, size_t len) override {}

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len) {
      return false;
    }

    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    const size_t arrived = std::min(len, this->incoming.size());
    this->rx.insert(this->rx.end(), this->incoming.begin(), this->incoming.begin() + arrived);
    this->incoming.erase(this->incoming.begin(), this->incoming.begin() + arrived);
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

// update() can come in the middle of a frame. The idle hub has already dropped the start of that frame,
// so the update drops the rest of it as well and publishes the frame that starts after it.
TEST_F(TeleInfoTest, DropsRestOfFrameCutByUpdate) {
  this->uart_.push_rx("\x02\nPAPP ");  // The first 7 of the 16 bytes of FRAME_500_VA, from the start byte 0x02.
  this->teleinfo_.loop();              // Before the update: drops them.

  this->teleinfo_.update();
  this->uart_.push_rx("00500 &\r\x03");  // The other 9 bytes: the value, its checksum and the end byte 0x03.
  this->uart_.push_rx(FRAME_750_VA);
  this->teleinfo_.loop();  // Drops those 9 bytes and finds the start byte 0x02 of FRAME_750_VA.
  this->teleinfo_.loop();  // Reads up to the end byte 0x03.
  this->teleinfo_.loop();  // Decodes the frame and publishes PAPP.

  EXPECT_THAT(this->listener_.values, ::testing::ElementsAre("00750"));
}

// Bytes can come in as fast as the idle hub drops them. The hub drops only the bytes that were there when
// the loop started, so the loop returns, and the bytes that came in meanwhile wait for the next loop.
TEST_F(TeleInfoTest, DropsOnlyBytesAlreadyThereWhileIdle) {
  this->uart_.push_rx(FRAME_500_VA);
  this->uart_.push_incoming(FRAME_750_VA);  // Comes in while the hub reads, one byte for each byte read.
  this->teleinfo_.loop();                   // Drops the 16 bytes of FRAME_500_VA.

  // FRAME_750_VA came in meanwhile and is still there.
  EXPECT_EQ(std::string(this->uart_.rx.begin(), this->uart_.rx.end()), FRAME_750_VA);
}

}  // namespace esphome::teleinfo::testing
