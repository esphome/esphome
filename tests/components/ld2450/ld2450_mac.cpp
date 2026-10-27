#include "common.h"
#include "esphome/components/ld24xx/ld24xx.h"

namespace esphome::ld2450::testing {

namespace {
// MAC query ACK (command 0xA5): header, length 10, command, result ok, 6 MAC bytes, footer.
std::vector<uint8_t> make_mac_ack(const uint8_t (&mac)[MAC_ADDRESS_SIZE]) {
  std::vector<uint8_t> frame(std::begin(ld24xx::CMD_FRAME_HEADER), std::end(ld24xx::CMD_FRAME_HEADER));
  frame.insert(frame.end(), {0x0A, 0x00, 0xA5, 0x01, 0x00, 0x00});
  frame.insert(frame.end(), mac, mac + MAC_ADDRESS_SIZE);
  frame.insert(frame.end(), std::begin(ld24xx::CMD_FRAME_FOOTER), std::end(ld24xx::CMD_FRAME_FOOTER));
  return frame;
}
}  // namespace

class LD2450MacAckTest : public ::testing::Test {
 protected:
  void SetUp() override { this->ld2450_.set_uart_parent(&this->mock_uart_); }
  std::string mac_str_() const {
    char buffer[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    return ld24xx::format_mac_str(this->ld2450_.mac_address_, buffer);
  }

  MockUARTComponent mock_uart_;
  TestableLD2450 ld2450_;
};

TEST_F(LD2450MacAckTest, BluetoothOffAckClearsStoredMac) {
  // Captured from radar firmware 2.04.23101915; the first byte has the multicast bit set
  static constexpr uint8_t BEDROOM_MAC[MAC_ADDRESS_SIZE] = {0x81, 0x2C, 0x62, 0xBE, 0xC9, 0x96};
  this->ld2450_.feed(make_mac_ack(BEDROOM_MAC));
  EXPECT_TRUE(this->ld2450_.bluetooth_on_);
  EXPECT_THAT(this->ld2450_.mac_address_, ::testing::ElementsAreArray(BEDROOM_MAC));
  ASSERT_EQ(this->mac_str_(), "81:2C:62:BE:C9:96");

  this->ld2450_.feed(make_mac_ack(ld24xx::NO_MAC));
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  EXPECT_FALSE(this->ld2450_.bluetooth_on_);
  EXPECT_THAT(this->ld2450_.mac_address_, ::testing::Each(0));
}

}  // namespace esphome::ld2450::testing
