#include <gtest/gtest.h>

#include <string>

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "common.h"

namespace esphome::mk2pvrouter::testing {

namespace {
class TestListener final : public Mk2PVRouterListener {
 public:
  explicit TestListener(const char *tag) : Mk2PVRouterListener(tag) {}
  void publish_val(const char *val) override {
    this->published_ = true;
    this->last_val_ = val;
  }

  bool published_{false};
  std::string last_val_;
};

class Mk2PVRouterTest : public ::testing::Test {
 protected:
  void SetUp() override { this->sut_.register_mk2pvrouter_listener(&this->listener_); }

  // Feeds one "tag<TAB>value<TAB>crc" literal, as the frame parser does after END_FRAME.
  template<size_t N> void process_(const char (&group)[N]) { this->sut_.process_group_(group, group + N - 1); }

  TestableMk2PVRouter sut_;
  TestListener listener_{"P1"};
};
}  // namespace

TEST(Mk2PVRouterListenerTest, GetTagReturnsConstructorTag) {
  TestListener listener("P1");
  EXPECT_STREQ(listener.get_tag(), "P1");
}

TEST_F(Mk2PVRouterTest, CalculateCrcMatchesKnownGroup) {
  // "P1\t1234\t" sums to 0x3D ('=') per the mk2pvrouter CRC algorithm.
  const char grp[] = "P1\t1234\t=";
  EXPECT_EQ(this->sut_.calculate_crc_(grp, sizeof(grp) - 1), '=');
}

TEST_F(Mk2PVRouterTest, CheckCrcAcceptsMatchingCrc) {
  const char grp[] = "P1\t1234\t=";
  EXPECT_TRUE(this->sut_.check_crc_(grp, grp + sizeof(grp) - 1));
}

TEST_F(Mk2PVRouterTest, CheckCrcRejectsMismatchedCrc) {
  const char grp[] = "P1\t1234\t!";
  EXPECT_FALSE(this->sut_.check_crc_(grp, grp + sizeof(grp) - 1));
}

TEST_F(Mk2PVRouterTest, CheckCrcRejectsEmptyGroup) {
  const char grp[] = "";
  EXPECT_FALSE(this->sut_.check_crc_(grp, grp));
}

TEST_F(Mk2PVRouterTest, ProcessGroupPublishesValidGroupToMatchingListener) {
  this->process_("P1\t1234\t=");
  EXPECT_TRUE(this->listener_.published_);
  EXPECT_EQ(this->listener_.last_val_, "1234");
}

TEST_F(Mk2PVRouterTest, ProcessGroupDropsGroupWithBadCrc) {
  this->process_("P1\t1234\t!");
  EXPECT_FALSE(this->listener_.published_);
}

TEST_F(Mk2PVRouterTest, ProcessGroupDropsGroupMissingValue) {
  // "P1\t" sums to 0x2A ('*'), so the CRC passes, but there is no second TAB for the value.
  this->process_("P1\t*");
  EXPECT_FALSE(this->listener_.published_);
}

TEST_F(Mk2PVRouterTest, LoopParsesFullFrameAndPublishesToListener) {
  MockUARTComponent uart;
  this->sut_.set_uart_parent(&uart);

  EXPECT_EQ(this->sut_.state_, TestableMk2PVRouter::State::WAITING_FOR_START);

  // STX, LF "P1\t1234\t=" CR, ETX
  uart.push_rx({0x02, 0x0a, 'P', '1', '\t', '1', '2', '3', '4', '\t', '=', 0x0d, 0x03});

  this->sut_.loop();  // Consume bytes up to and including START_FRAME.
  EXPECT_EQ(this->sut_.state_, TestableMk2PVRouter::State::START_FRAME_RECEIVED);

  this->sut_.loop();  // Buffer the frame body up to END_FRAME.
  EXPECT_EQ(this->sut_.state_, TestableMk2PVRouter::State::END_FRAME_RECEIVED);

  this->sut_.loop();  // Parse the buffered groups and publish them.
  EXPECT_EQ(this->sut_.state_, TestableMk2PVRouter::State::WAITING_FOR_START);
  EXPECT_EQ(this->sut_.buf_index_, 0);
  EXPECT_TRUE(this->listener_.published_);
  EXPECT_EQ(this->listener_.last_val_, "1234");
}

TEST_F(Mk2PVRouterTest, LoopIgnoresBytesBeforeStartFrame) {
  MockUARTComponent uart;
  this->sut_.set_uart_parent(&uart);

  uart.push_rx({'g', 'a', 'r', 'b', 'a', 'g', 'e', 0x02});

  this->sut_.loop();

  EXPECT_EQ(this->sut_.state_, TestableMk2PVRouter::State::START_FRAME_RECEIVED);
  EXPECT_EQ(this->sut_.buf_index_, 0);
}

}  // namespace esphome::mk2pvrouter::testing
