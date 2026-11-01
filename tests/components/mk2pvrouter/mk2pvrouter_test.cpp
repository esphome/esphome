#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "common.h"

namespace esphome::mk2pvrouter::testing {

namespace {
// The parser works on raw frame bytes; the groups below are written as text for readability.
const uint8_t *as_bytes(const char *str) { return reinterpret_cast<const uint8_t *>(str); }

struct TestListener {
  const char *tag;
  bool published_{false};
  std::string last_val_;
};

// Stands in for the generated function; records values for the running test's listener tag
TestListener *active_listener = nullptr;
void record_value(const char *tag, const char *val) {
  if (strcmp(tag, active_listener->tag) != 0)
    return;
  active_listener->published_ = true;
  active_listener->last_val_ = val;
}

class Mk2PVRouterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    active_listener = &this->listener_;
    this->sut_.set_value_fn(record_value);
  }

  // Feeds one "tag<TAB>value<TAB>crc" literal, as the frame parser does after END_FRAME.
  template<size_t N> void process_(const char (&group)[N]) {
    this->sut_.process_group_(as_bytes(group), as_bytes(group) + N - 1);
  }

  TestableMk2PVRouter sut_;
  TestListener listener_{"P1"};
};
}  // namespace

TEST_F(Mk2PVRouterTest, CalculateCrcMatchesKnownGroup) {
  // "P1\t1234\t" sums to 0x3D ('=') per the mk2pvrouter CRC algorithm.
  const char grp[] = "P1\t1234\t=";
  EXPECT_EQ(this->sut_.calculate_crc_(as_bytes(grp), sizeof(grp) - 1), '=');
}

TEST_F(Mk2PVRouterTest, CheckCrcAcceptsMatchingCrc) {
  const char grp[] = "P1\t1234\t=";
  EXPECT_TRUE(this->sut_.check_crc_(as_bytes(grp), as_bytes(grp) + sizeof(grp) - 1));
}

TEST_F(Mk2PVRouterTest, CheckCrcRejectsMismatchedCrc) {
  const char grp[] = "P1\t1234\t!";
  EXPECT_FALSE(this->sut_.check_crc_(as_bytes(grp), as_bytes(grp) + sizeof(grp) - 1));
}

TEST_F(Mk2PVRouterTest, CheckCrcRejectsEmptyGroup) {
  const char grp[] = "";
  EXPECT_FALSE(this->sut_.check_crc_(as_bytes(grp), as_bytes(grp)));
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
