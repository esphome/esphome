#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include "common.h"
#include "esphome/core/application.h"

namespace esphome::ld2450::testing {

using ::testing::_;
using ::testing::NiceMock;

namespace {
constexpr uint32_t LINK_TIMEOUT_MS = 1000;  // the shortest the option allows
constexpr uint32_t NOT_LOST = 0xFFFFFFFFu;
}  // namespace

// The radar link watchdog, driven by the loop clock from the start time the test is run with, once early and once
// shortly before the 2^32 ms wrap. Frames reach the hub through the UART mock, as in loop().
class LD2450LinkTest : public ::testing::TestWithParam<uint32_t> {
 protected:
  void SetUp() override {
    ON_CALL(this->mock_uart_, available()).WillByDefault([this]() { return this->rx_.size(); });
    ON_CALL(this->mock_uart_, read_array(_, _)).WillByDefault([this](uint8_t *data, size_t len) {
      if (this->rx_.size() < len)
        return false;
      std::copy(this->rx_.begin(), this->rx_.begin() + len, data);
      this->rx_.erase(this->rx_.begin(), this->rx_.begin() + len);
      return true;
    });
    this->ld2450_.set_uart_parent(&this->mock_uart_);
    this->ld2450_.set_link_timeout(LINK_TIMEOUT_MS);
    // As setup() leaves them, without its boot restart
    this->ld2450_.last_frame_ms_ = this->t0_;
    this->ld2450_.link_deadline_ms_ = this->t0_ + LINK_TIMEOUT_MS;
  }

  // Restarts, factory reset, Bluetooth and baud changes leave timeouts of up to 200 ms + 1500 ms
  void TearDown() override { drain_scheduler(3000); }

  /// One pass of the main loop at t0_ + `at`, with `bytes` arriving at the UART first.
  void pass_(uint32_t at, const std::vector<uint8_t> &bytes = {}) {
    LoopBlockingGuard clock{nullptr, nullptr, this->t0_ + at};
    this->rx_.insert(this->rx_.end(), bytes.begin(), bytes.end());
    this->ld2450_.loop();
  }
  void frame_(uint32_t at) { this->pass_(at, make_periodic_frame()); }
  /// Call `action` with the loop clock at t0_ + `at`.
  template<typename F> void act_(uint32_t at, const F &action) {
    LoopBlockingGuard clock{nullptr, nullptr, this->t0_ + at};
    action();
  }
  /// Open a grace window the way the radar actions do, without scheduling their follow-up commands.
  void open_window_(uint32_t at) {
    this->act_(at, [this]() { this->ld2450_.open_link_grace_(); });
  }
  /// The first pass at which the link is lost, searched in 1 ms steps from `from`; NOT_LOST if it is still up at
  /// `until`. Times are relative to t0_.
  uint32_t lost_at_(uint32_t from, uint32_t until) {
    for (uint32_t at = from; at != until; at++) {
      this->pass_(at);
      if (this->ld2450_.link_state_ == LinkState::LINK_STATE_LOST)
        return at;
    }
    return NOT_LOST;
  }
  void expect_up_with_frames_(uint32_t from, uint32_t until) {
    for (uint32_t at = from; at <= until; at += 100) {
      this->frame_(at);
      ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP) << "at " << at;
    }
  }

  const uint32_t t0_ = GetParam();
  NiceMock<MockUARTComponent> mock_uart_;
  TestableLD2450 ld2450_;
  std::vector<uint8_t> rx_;
};

INSTANTIATE_TEST_SUITE_P(ClockStart, LD2450LinkTest, ::testing::Values(10000u, 0xFFFFF000u),
                         [](const ::testing::TestParamInfo<uint32_t> &info) {
                           return std::string(info.param == 10000u ? "Early" : "BeforeWrap");
                         });

TEST_P(LD2450LinkTest, FramesEvery100MsKeepTheLinkUp) { this->expect_up_with_frames_(0, 3 * LINK_TIMEOUT_MS); }

TEST_P(LD2450LinkTest, LostExactlyAtTheLinkTimeout) {
  this->frame_(0);
  EXPECT_EQ(this->lost_at_(1, 2 * LINK_TIMEOUT_MS), LINK_TIMEOUT_MS);
}

TEST_P(LD2450LinkTest, AckFramesDoNotKeepTheLinkAlive) {
  this->frame_(0);
  for (uint32_t at = 100; at < LINK_TIMEOUT_MS; at += 100) {
    this->pass_(at, make_ack_frame());
    ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP) << "at " << at;
  }
  this->pass_(LINK_TIMEOUT_MS, make_ack_frame());
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_LOST);
}

TEST_P(LD2450LinkTest, InvalidInputIsCountedButDoesNotKeepTheLinkAlive) {
  this->frame_(0);
  EXPECT_FALSE(this->ld2450_.bytes_since_frame_);
  EXPECT_EQ(this->ld2450_.invalid_frames_, 0u);
  // Bytes that never start a header are skipped by the parser, so only the frames with a bad footer are counted
  this->pass_(300, {0x01, 0x02, 0x03, 0x04, 0x55, 0xCC});
  EXPECT_TRUE(this->ld2450_.bytes_since_frame_);
  EXPECT_EQ(this->ld2450_.invalid_frames_, 0u);
  auto bad_footer = make_periodic_frame();
  bad_footer[bad_footer.size() - 1] = 0xAB;
  this->pass_(400, bad_footer);
  this->pass_(500, bad_footer);
  EXPECT_EQ(this->ld2450_.invalid_frames_, 2u);
  EXPECT_EQ(this->lost_at_(501, 2 * LINK_TIMEOUT_MS), LINK_TIMEOUT_MS);
  this->frame_(LINK_TIMEOUT_MS + 100);
  EXPECT_FALSE(this->ld2450_.bytes_since_frame_);
  EXPECT_EQ(this->ld2450_.invalid_frames_, 0u);
}

// --- Grace windows ---

TEST_P(LD2450LinkTest, RestartThenFramesWithinTheWindowCausesNoLoss) {
  this->frame_(0);
  this->open_window_(100);
  EXPECT_EQ(this->lost_at_(100, 3100), NOT_LOST);
  // Back after the radar's restart, well inside the window
  this->expect_up_with_frames_(3100, 3100 + 2 * LINK_TIMEOUT_MS);
}

TEST_P(LD2450LinkTest, AFrameInFlightAfterTheRestartDoesNotEndTheWindow) {
  this->frame_(0);
  this->open_window_(100);
  this->frame_(140);  // sent before the radar handled the command
  EXPECT_EQ(this->lost_at_(141, 100 + LINK_GRACE_MS + 100), 100 + LINK_GRACE_MS);
}

// One test walks the loop clock through all actions, so their follow-up timeouts overlap and drain once.
// With no frame after the action, the link is lost when its window ends.
TEST_P(LD2450LinkTest, EachRadarActionOpensAWindow) {
  const std::function<void()> actions[] = {
      [this]() { this->ld2450_.open_link_grace_(); },  [this]() { this->ld2450_.restart_and_read_all_info(); },
      [this]() { this->ld2450_.read_all_info(); },     [this]() { this->ld2450_.factory_reset(); },
      [this]() { this->ld2450_.set_bluetooth(true); }, [this]() { this->ld2450_.set_baud_rate("115200"); },
  };
  uint32_t at = 0;
  for (size_t i = 0; i < std::size(actions); i++) {
    // Frames are flowing (after the previous loss, this frame restores the link); then no frame arrives
    this->frame_(at);
    ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP) << "action " << i;
    this->act_(at, actions[i]);
    EXPECT_EQ(this->lost_at_(at, at + LINK_GRACE_MS + 100), at + LINK_GRACE_MS) << "action " << i;
    at += LINK_GRACE_MS + 1000;
  }
}

TEST_P(LD2450LinkTest, ZoneWritesDuringSilenceDoNotExtendTheTimeout) {
  this->frame_(0);
  for (uint32_t at = 100; at < LINK_TIMEOUT_MS; at += 200) {
    this->act_(at, [this]() { this->ld2450_.reset_radar_zone(); });
    this->pass_(at);
  }
  EXPECT_EQ(this->lost_at_(900, 2 * LINK_TIMEOUT_MS), LINK_TIMEOUT_MS);
}

TEST_P(LD2450LinkTest, WindowsOpenedLateCannotPassTheHardCap) {
  this->frame_(0);
  // Each restart is repeated just before the previous window ends
  uint32_t from = 0;
  for (uint32_t at : {900u, 5800u, 10700u, 14900u}) {
    EXPECT_EQ(this->lost_at_(from + 1, at), NOT_LOST);
    this->open_window_(at);
    this->pass_(at);
    ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP) << "at " << at;
    from = at;
  }
  EXPECT_EQ(this->lost_at_(14901, 16000), LINK_GRACE_CAP_MS);
}

TEST_P(LD2450LinkTest, AWindowDoesNotOutliveItsEnd) {
  this->frame_(0);
  this->open_window_(100);
  this->expect_up_with_frames_(200, 20000);
  EXPECT_EQ(this->lost_at_(20001, 30000), 20000 + LINK_TIMEOUT_MS);
}

TEST_P(LD2450LinkTest, TimeoutLongerThanTheCapIsKept) {
  this->ld2450_.set_link_timeout(20000);
  this->frame_(0);
  this->open_window_(100);
  EXPECT_EQ(this->lost_at_(100, 30000), 20000u);
}

// --- Boot ---

TEST_P(LD2450LinkTest, NoFrameAfterBootIsLostOnceAfterTheBootWindowPlusTheTimeout) {
  this->act_(0, [this]() { this->ld2450_.setup(); });
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_WAITING);
  EXPECT_EQ(this->lost_at_(0, LINK_GRACE_MS + 2 * LINK_TIMEOUT_MS), LINK_GRACE_MS + LINK_TIMEOUT_MS);
  // Stays lost, without another transition
  this->pass_(60000);
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_LOST);
}

TEST_P(LD2450LinkTest, FirstFrameAfterBootBringsTheLinkUp) {
  this->act_(0, [this]() { this->ld2450_.setup(); });
  this->frame_(2500);
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP);
  // The boot window still covers the radar's restart silence
  EXPECT_EQ(this->lost_at_(2501, 10000), LINK_GRACE_MS);
}

// --- Recovery ---

TEST_P(LD2450LinkTest, AFrameAfterLossBringsTheLinkUpAndTheWatchdogRearms) {
  this->frame_(0);
  this->pass_(LINK_TIMEOUT_MS);
  ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_LOST);

  this->frame_(5000);
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP);
  EXPECT_EQ(this->lost_at_(5001, 7000), 5000 + LINK_TIMEOUT_MS);
}

TEST_P(LD2450LinkTest, AWindowOpenedWhileLostDoesNotOutliveTheLoss) {
  this->frame_(0);
  this->pass_(LINK_TIMEOUT_MS);
  ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_LOST);
  this->open_window_(2000);

  this->frame_(2500);
  ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP);
  EXPECT_EQ(this->lost_at_(2501, 7000), 2500 + LINK_TIMEOUT_MS);
}

}  // namespace esphome::ld2450::testing
