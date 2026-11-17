#include "common.h"

namespace esphome::ld2450::testing {

namespace {
// Version answer: command A0 01, status, firmware type, then the six version bytes.
constexpr uint8_t VERSION_BYTES[] = {0x07, 0x02, 0x16, 0x24, 0x06, 0x22};

std::vector<uint8_t> make_version_ack() {
  std::vector<uint8_t> frame = {0xFD, 0xFC, 0xFB, 0xFA, 0x0C, 0x00, 0xA0, 0x01, 0x00, 0x00, 0x00, 0x01};
  frame.insert(frame.end(), std::begin(VERSION_BYTES), std::end(VERSION_BYTES));
  frame.insert(frame.end(), {0x04, 0x03, 0x02, 0x01});
  return frame;
}

// A version answer cut after two version bytes: the footer sits where the version would be.
std::vector<uint8_t> make_short_version_ack() {
  return {0xFD, 0xFC, 0xFB, 0xFA, 0x08, 0x00, 0xA0, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x02, 0x04, 0x03, 0x02, 0x01};
}

// Target mode answer: command 91 01, status, then the two mode bytes.
std::vector<uint8_t> make_target_mode_ack(uint8_t mode) {
  return {0xFD, 0xFC, 0xFB, 0xFA, 0x06, 0x00, 0x91, 0x01, 0x00, 0x00, mode, 0x00, 0x04, 0x03, 0x02, 0x01};
}

// A target mode answer with no mode bytes: the footer sits where the mode would be.
std::vector<uint8_t> make_short_target_mode_ack() {
  return {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0x91, 0x01, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01};
}
}  // namespace

class LD2450ShortAckTest : public ::testing::Test {
 protected:
  void SetUp() override { this->ld2450_.set_uart_parent(&this->mock_uart_); }

  MockUARTComponent mock_uart_;
  TestableLD2450 ld2450_;
};

TEST_F(LD2450ShortAckTest, ShortVersionAckDoesNotChangeTheVersion) {
  this->ld2450_.feed(make_short_version_ack());
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  for (uint8_t byte : this->ld2450_.version_) {
    EXPECT_EQ(byte, 0);
  }

  this->ld2450_.feed(make_version_ack());
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  EXPECT_EQ(std::memcmp(this->ld2450_.version_, VERSION_BYTES, sizeof(VERSION_BYTES)), 0);
}

#ifdef USE_SWITCH
TEST_F(LD2450ShortAckTest, ShortTargetModeAckDoesNotChangeTheSwitch) {
  TestSwitch multi_target;
  this->ld2450_.set_multi_target_switch(&multi_target);
  multi_target.publish_state(true);

  this->ld2450_.feed(make_short_target_mode_ack());
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  EXPECT_TRUE(multi_target.state);

  this->ld2450_.feed(make_target_mode_ack(0x01));
  EXPECT_FALSE(multi_target.state);
  this->ld2450_.feed(make_target_mode_ack(0x02));
  EXPECT_TRUE(multi_target.state);
}
#endif

}  // namespace esphome::ld2450::testing
