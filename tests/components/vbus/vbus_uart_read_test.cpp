#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "esphome/components/vbus/vbus.h"

namespace esphome::vbus::testing {
namespace {

class FailingUART : public uart::UARTComponent {
 public:
  void inject(const std::vector<uint8_t> &bytes) { this->rx_.insert(this->rx_.end(), bytes.begin(), bytes.end()); }
  void fail_next_read() { this->fail_next_read_ = true; }
  size_t read_attempts() const { return this->read_attempts_; }
  size_t available() override { return this->rx_.size() - this->position_; }
  bool read_array(uint8_t *data, size_t size) override {
    ++this->read_attempts_;
    if (this->fail_next_read_) {
      this->fail_next_read_ = false;
      // A failed read does not provide a valid byte, even if it wrote to the buffer.
      *data = 0xaa;
      return false;
    }
    if (size > this->available())
      return false;
    std::copy_n(this->rx_.data() + this->position_, size, data);
    this->position_ += size;
    return true;
  }
  bool peek_byte(uint8_t *data) override {
    if (!this->available())
      return false;
    *data = this->rx_[this->position_];
    return true;
  }
  void write_array(const uint8_t *data, size_t size) override { ADD_FAILURE() << "RX must not transmit"; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

 protected:
  std::vector<uint8_t> rx_;
  size_t position_{0};
  size_t read_attempts_{0};
  bool fail_next_read_{false};
};

class UARTReadListener : public VBusListener {
 public:
  std::vector<std::vector<uint8_t>> messages;

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->messages.push_back(message); }
};

const std::vector<uint8_t> PACKET = {0xaa, 0x10, 0x00, 0x11, 0x22, 0x10, 0x00, 0x01, 0x02, 0x29, 0x01,
                                     0x02, 0x03, 0x04, 0x00, 0x75, 0x10, 0x20, 0x30, 0x40, 0x0f, 0x50};
const std::vector<uint8_t> EXPECTED_PAYLOAD = {0x01, 0x02, 0x03, 0x04, 0x90, 0xa0, 0xb0, 0xc0};

class VBusUARTReadFailureTest : public ::testing::TestWithParam<size_t> {
 protected:
  void SetUp() override {
    this->bus_.set_uart_parent(&this->uart_);
    this->listener_.set_command(0x0100);
    this->listener_.set_source(0x2211);
    this->listener_.set_dest(0x0010);
    this->bus_.register_listener(&this->listener_);
  }

  FailingUART uart_;
  VBus bus_{};
  UARTReadListener listener_;
};

TEST_P(VBusUARTReadFailureTest, YieldsOnReadFailureAndResumesThePartialPacket) {
  const auto split = PACKET.begin() + GetParam();
  this->uart_.inject({PACKET.begin(), split});
  this->bus_.loop();
  ASSERT_TRUE(this->listener_.messages.empty());
  this->uart_.inject({split, PACKET.end()});

  for (size_t failure = 0; failure < 2; ++failure) {
    const size_t pending = this->uart_.available();
    const size_t attempts = this->uart_.read_attempts();
    this->uart_.fail_next_read();
    this->bus_.loop();
    ASSERT_EQ(this->uart_.read_attempts(), attempts + 1);
    ASSERT_EQ(this->uart_.available(), pending);
    ASSERT_TRUE(this->listener_.messages.empty());
  }

  this->bus_.loop();
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], EXPECTED_PAYLOAD);
  EXPECT_EQ(this->uart_.available(), 0u);

  this->uart_.inject(PACKET);
  this->bus_.loop();
  ASSERT_EQ(this->listener_.messages.size(), 2u);
  EXPECT_EQ(this->listener_.messages[1], EXPECTED_PAYLOAD);
}

// Before sync, inside the header, inside a frame, and before the final checksum.
INSTANTIATE_TEST_SUITE_P(PacketPositions, VBusUARTReadFailureTest, ::testing::Values(0u, 5u, 13u, 21u));

}  // namespace
}  // namespace esphome::vbus::testing
