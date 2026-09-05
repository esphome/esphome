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
}  // namespace

TEST(Mk2PVRouterListenerTest, GetTagReturnsConstructorTag) {
  TestListener listener("P1");
  EXPECT_STREQ(listener.get_tag(), "P1");
}

TEST(Mk2PVRouterTest, CalculateCrcMatchesKnownGroup) {
  TestableMk2PVRouter sut;
  // "P1\t1234\t" sums to 0x3D ('=') per the mk2pvrouter CRC algorithm.
  const char grp[] = "P1\t1234\t=";
  EXPECT_EQ(sut.calculate_crc_(grp, sizeof(grp) - 1), '=');
}

TEST(Mk2PVRouterTest, CheckCrcAcceptsMatchingCrc) {
  TestableMk2PVRouter sut;
  const char grp[] = "P1\t1234\t=";
  EXPECT_TRUE(sut.check_crc_(grp, grp + sizeof(grp) - 1));
}

TEST(Mk2PVRouterTest, CheckCrcRejectsMismatchedCrc) {
  TestableMk2PVRouter sut;
  const char grp[] = "P1\t1234\t!";
  EXPECT_FALSE(sut.check_crc_(grp, grp + sizeof(grp) - 1));
}

TEST(Mk2PVRouterTest, CheckCrcRejectsEmptyGroup) {
  TestableMk2PVRouter sut;
  const char grp[] = "";
  EXPECT_FALSE(sut.check_crc_(grp, grp));
}

TEST(Mk2PVRouterTest, ProcessGroupPublishesValidGroupToMatchingListener) {
  TestableMk2PVRouter sut;
  TestListener listener("P1");
  sut.register_mk2pvrouter_listener(&listener);

  const char grp[] = "P1\t1234\t=";
  sut.process_group_(grp, grp + sizeof(grp) - 1);

  EXPECT_TRUE(listener.published_);
  EXPECT_EQ(listener.last_val_, "1234");
}

TEST(Mk2PVRouterTest, ProcessGroupDropsGroupWithBadCrc) {
  TestableMk2PVRouter sut;
  TestListener listener("P1");
  sut.register_mk2pvrouter_listener(&listener);

  const char grp[] = "P1\t1234\t!";
  sut.process_group_(grp, grp + sizeof(grp) - 1);

  EXPECT_FALSE(listener.published_);
}

TEST(Mk2PVRouterTest, ProcessGroupDropsGroupMissingValue) {
  TestableMk2PVRouter sut;
  TestListener listener("P1");
  sut.register_mk2pvrouter_listener(&listener);

  // "P1\t" sums to 0x2A ('*'), so the CRC check passes, but there's no second
  // TAB left for get_field() to find the value.
  const char grp[] = "P1\t*";
  sut.process_group_(grp, grp + sizeof(grp) - 1);

  EXPECT_FALSE(listener.published_);
}

TEST(Mk2PVRouterTest, LoopParsesFullFrameAndPublishesToListener) {
  MockUARTComponent uart;
  TestableMk2PVRouter sut;
  sut.set_uart_parent(&uart);
  TestListener listener("P1");
  sut.register_mk2pvrouter_listener(&listener);

  EXPECT_EQ(sut.state_, TestableMk2PVRouter::State::WAITING_FOR_START);

  // STX, LF "P1\t1234\t=" CR, ETX
  uart.push_rx({0x02, 0x0a, 'P', '1', '\t', '1', '2', '3', '4', '\t', '=', 0x0d, 0x03});

  sut.loop();  // Consume bytes up to and including START_FRAME.
  EXPECT_EQ(sut.state_, TestableMk2PVRouter::State::START_FRAME_RECEIVED);

  sut.loop();  // Buffer the frame body up to END_FRAME.
  EXPECT_EQ(sut.state_, TestableMk2PVRouter::State::END_FRAME_RECEIVED);

  sut.loop();  // Parse the buffered groups and publish them.
  EXPECT_EQ(sut.state_, TestableMk2PVRouter::State::WAITING_FOR_START);
  EXPECT_EQ(sut.buf_index_, 0);
  EXPECT_TRUE(listener.published_);
  EXPECT_EQ(listener.last_val_, "1234");
}

TEST(Mk2PVRouterTest, LoopIgnoresBytesBeforeStartFrame) {
  MockUARTComponent uart;
  TestableMk2PVRouter sut;
  sut.set_uart_parent(&uart);

  uart.push_rx({'g', 'a', 'r', 'b', 'a', 'g', 'e', 0x02});

  sut.loop();

  EXPECT_EQ(sut.state_, TestableMk2PVRouter::State::START_FRAME_RECEIVED);
  EXPECT_EQ(sut.buf_index_, 0);
}

}  // namespace esphome::mk2pvrouter::testing
