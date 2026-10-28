#include <algorithm>
#include <functional>
#include <iterator>
#include "common.h"
#include "esphome/core/application.h"

namespace esphome::ld2450::testing {

using ::testing::_;
using ::testing::NiceMock;

namespace {
constexpr uint32_t LINK_TIMEOUT_MS = 1000;  // the shortest the option allows
constexpr uint32_t START_MS = 10000;
constexpr uint32_t NOT_LOST = 0xFFFFFFFFu;
}  // namespace

// The radar link watchdog, driven by the loop clock. Frames reach the hub through the UART mock, as in loop().
class LD2450LinkTest : public ::testing::Test {
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
  }

  // Restarts, factory reset, Bluetooth and baud changes leave timeouts of up to 200 ms + 1500 ms
  void TearDown() override { drain_scheduler(3000); }

  /// One pass of the main loop at time `now`, with `bytes` arriving at the UART first.
  void pass_(uint32_t now, const std::vector<uint8_t> &bytes = {}) {
    LoopBlockingGuard clock{nullptr, nullptr, now};
    this->rx_.insert(this->rx_.end(), bytes.begin(), bytes.end());
    this->ld2450_.loop();
  }
  void frame_(uint32_t now) { this->pass_(now, make_periodic_frame()); }
  /// Run the clock to `now` in steps of `step` ms without any data.
  void silence_until_(uint32_t from, uint32_t now, uint32_t step = 100) {
    for (uint32_t t = from + step; t < now; t += step) {
      this->pass_(t);
    }
  }
  /// Call `action` with the loop clock at `now`; it opens its grace window from there.
  template<typename F> void act_(uint32_t now, const F &action) {
    LoopBlockingGuard clock{nullptr, nullptr, now};
    action();
  }
  /// Open a grace window at `now` the way the radar actions do, without scheduling their follow-up commands.
  void open_window_(uint32_t now) {
    this->act_(now, [this]() { this->ld2450_.open_link_grace_(); });
  }
  /// The first pass at which the link is lost, searched in 1 ms steps from `from` (clock wrap-safe); NOT_LOST if
  /// it is still up at `until`.
  uint32_t lost_at_(uint32_t from, uint32_t until) {
    for (uint32_t t = from; t != until; t++) {
      this->pass_(t);
      if (this->ld2450_.link_state_ == LinkState::LINK_STATE_LOST)
        return t;
    }
    return NOT_LOST;
  }

  NiceMock<MockUARTComponent> mock_uart_;
  TestableLD2450 ld2450_;
  std::vector<uint8_t> rx_;
};

TEST_F(LD2450LinkTest, FramesEvery100MsKeepTheLinkUp) {
  this->frame_(START_MS);
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP);
  for (uint32_t t = START_MS + 100; t <= START_MS + 3 * LINK_TIMEOUT_MS; t += 100) {
    this->frame_(t);
    ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP) << "at " << t;
  }
}

TEST_F(LD2450LinkTest, LostExactlyAtTheLinkTimeout) {
  this->frame_(START_MS);
  EXPECT_EQ(this->lost_at_(START_MS + 1, START_MS + 2 * LINK_TIMEOUT_MS), START_MS + LINK_TIMEOUT_MS);
}

TEST_F(LD2450LinkTest, AckFramesDoNotKeepTheLinkAlive) {
  this->frame_(START_MS);
  for (uint32_t t = START_MS + 100; t < START_MS + LINK_TIMEOUT_MS; t += 100) {
    this->pass_(t, make_ack_frame());
    ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP) << "at " << t;
  }
  this->pass_(START_MS + LINK_TIMEOUT_MS, make_ack_frame());
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_LOST);
}

TEST_F(LD2450LinkTest, InvalidInputDoesNotKeepTheLinkAlive) {
  this->frame_(START_MS);
  // Bytes that never start a header are skipped by the parser, so only the frame with a bad footer is counted
  this->pass_(START_MS + 300, {0x01, 0x02, 0x03, 0x04, 0x55, 0xCC});
  auto bad_footer = make_periodic_frame();
  bad_footer[bad_footer.size() - 1] = 0xAB;
  this->pass_(START_MS + 500, bad_footer);
  EXPECT_EQ(this->ld2450_.invalid_frames_, 1u);
  EXPECT_EQ(this->lost_at_(START_MS + 501, START_MS + 2 * LINK_TIMEOUT_MS), START_MS + LINK_TIMEOUT_MS);
}

TEST_F(LD2450LinkTest, BytesAndInvalidFramesAreCountedSinceTheLastValidFrame) {
  this->frame_(START_MS);
  EXPECT_EQ(this->ld2450_.bytes_since_frame_, 0u);
  EXPECT_EQ(this->ld2450_.invalid_frames_, 0u);
  auto bad_footer = make_periodic_frame();
  bad_footer[bad_footer.size() - 1] = 0xAB;
  this->pass_(START_MS + 100, {0x01, 0x02, 0x03, 0x04});
  this->pass_(START_MS + 150, bad_footer);
  this->pass_(START_MS + 200, bad_footer);
  EXPECT_EQ(this->ld2450_.bytes_since_frame_, 4u + 2 * bad_footer.size());
  EXPECT_EQ(this->ld2450_.invalid_frames_, 2u);
  this->frame_(START_MS + 300);
  EXPECT_EQ(this->ld2450_.bytes_since_frame_, 0u);
  EXPECT_EQ(this->ld2450_.invalid_frames_, 0u);
}

// --- Grace windows ---

TEST_F(LD2450LinkTest, RestartThenFramesWithinTheWindowCausesNoLoss) {
  this->frame_(START_MS);
  this->open_window_(START_MS + 100);
  EXPECT_EQ(this->lost_at_(START_MS + 100, START_MS + 100 + 3000), NOT_LOST);
  // Back after the radar's restart, well inside the window
  this->frame_(START_MS + 100 + 3000);
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP);
  for (uint32_t t = START_MS + 3200; t <= START_MS + 3200 + 2 * LINK_TIMEOUT_MS; t += 100) {
    this->frame_(t);
    ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP) << "at " << t;
  }
}

TEST_F(LD2450LinkTest, RestartThenNoFramesIsLostAtTheWindowEnd) {
  this->frame_(START_MS);
  const uint32_t restart_ms = START_MS + 100;
  this->open_window_(restart_ms);
  EXPECT_EQ(this->lost_at_(restart_ms, restart_ms + LINK_GRACE_MS + 100), restart_ms + LINK_GRACE_MS);
}

TEST_F(LD2450LinkTest, AFrameInFlightAfterTheRestartDoesNotEndTheWindow) {
  this->frame_(START_MS);
  const uint32_t restart_ms = START_MS + 100;
  this->open_window_(restart_ms);
  this->frame_(restart_ms + 40);  // sent before the radar handled the command
  EXPECT_EQ(this->lost_at_(restart_ms + 41, restart_ms + LINK_GRACE_MS + 100), restart_ms + LINK_GRACE_MS);
}

// One test walks the loop clock through all actions, so their follow-up timeouts overlap and drain once
TEST_F(LD2450LinkTest, EachRadarActionOpensAWindow) {
  const std::function<void()> actions[] = {
      [this]() { this->ld2450_.restart_and_read_all_info(); },
      [this]() { this->ld2450_.read_all_info(); },
      [this]() { this->ld2450_.factory_reset(); },
      [this]() { this->ld2450_.set_bluetooth(true); },
      [this]() { this->ld2450_.set_baud_rate("115200"); },
  };
  uint32_t t = START_MS;
  for (size_t i = 0; i < std::size(actions); i++) {
    // Frames are flowing (after the previous loss, this frame restores the link); then no frame arrives
    this->frame_(t);
    ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP) << "action " << i;
    this->act_(t, actions[i]);
    EXPECT_EQ(this->lost_at_(t, t + LINK_GRACE_MS + 100), t + LINK_GRACE_MS) << "action " << i;
    t += LINK_GRACE_MS + 1000;
  }
}

TEST_F(LD2450LinkTest, ZoneWritesDuringSilenceDoNotExtendTheTimeout) {
  this->frame_(START_MS);
  for (uint32_t t = START_MS + 100; t < START_MS + LINK_TIMEOUT_MS; t += 200) {
    this->act_(t, [this]() { this->ld2450_.reset_radar_zone(); });
    this->pass_(t);
  }
  EXPECT_EQ(this->lost_at_(START_MS + 900, START_MS + 2 * LINK_TIMEOUT_MS), START_MS + LINK_TIMEOUT_MS);
}

TEST_F(LD2450LinkTest, WindowsOpenedLateCannotPassTheHardCap) {
  this->frame_(START_MS);
  // Each restart is repeated just before the previous window ends
  for (uint32_t offset : {900u, 5800u, 10700u, 14900u}) {
    this->silence_until_(START_MS, START_MS + offset);
    this->open_window_(START_MS + offset);
    this->pass_(START_MS + offset);
    ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP) << "at " << offset;
  }
  EXPECT_EQ(this->lost_at_(START_MS + 14900, START_MS + 16000), START_MS + LINK_GRACE_CAP_MS);
}

TEST_F(LD2450LinkTest, AWindowDoesNotOutliveItsEnd) {
  this->frame_(START_MS);
  this->open_window_(START_MS + 100);
  for (uint32_t t = START_MS + 200; t <= START_MS + 20000; t += 100) {
    this->frame_(t);
  }
  EXPECT_EQ(this->lost_at_(START_MS + 20001, START_MS + 30000), START_MS + 20000 + LINK_TIMEOUT_MS);
}

TEST_F(LD2450LinkTest, TimeoutLongerThanTheCapIsKept) {
  this->ld2450_.set_link_timeout(20000);
  this->frame_(START_MS);
  this->open_window_(START_MS + 100);
  EXPECT_EQ(this->lost_at_(START_MS + 100, START_MS + 30000), START_MS + 20000);
}

// --- Boot ---

TEST_F(LD2450LinkTest, NoFrameAfterBootIsLostOnceAfterTheBootWindowPlusTheTimeout) {
  const uint32_t boot_ms = START_MS;
  this->act_(boot_ms, [this]() { this->ld2450_.setup(); });
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_WAITING);
  EXPECT_EQ(this->lost_at_(boot_ms, boot_ms + LINK_GRACE_MS + 2 * LINK_TIMEOUT_MS),
            boot_ms + LINK_GRACE_MS + LINK_TIMEOUT_MS);
  // Stays lost, without another transition
  this->pass_(boot_ms + 60000);
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_LOST);
}

TEST_F(LD2450LinkTest, FirstFrameAfterBootBringsTheLinkUp) {
  this->act_(START_MS, [this]() { this->ld2450_.setup(); });
  this->frame_(START_MS + 2500);
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP);
  // The boot window still covers the radar's restart silence
  EXPECT_EQ(this->lost_at_(START_MS + 2501, START_MS + 10000), START_MS + LINK_GRACE_MS);
}

// --- Recovery ---

TEST_F(LD2450LinkTest, AFrameAfterLossBringsTheLinkUpAndTheWatchdogRearms) {
  this->frame_(START_MS);
  this->pass_(START_MS + LINK_TIMEOUT_MS);
  ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_LOST);

  this->frame_(START_MS + 5000);
  EXPECT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP);
  EXPECT_EQ(this->ld2450_.bytes_since_frame_, 0u);
  EXPECT_EQ(this->lost_at_(START_MS + 5001, START_MS + 7000), START_MS + 5000 + LINK_TIMEOUT_MS);
}

TEST_F(LD2450LinkTest, AWindowOpenedWhileLostDoesNotOutliveTheLoss) {
  this->frame_(START_MS);
  this->pass_(START_MS + LINK_TIMEOUT_MS);
  ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_LOST);
  this->open_window_(START_MS + 2000);

  this->frame_(START_MS + 2500);
  ASSERT_EQ(this->ld2450_.link_state_, LinkState::LINK_STATE_UP);
  EXPECT_EQ(this->lost_at_(START_MS + 2501, START_MS + 7000), START_MS + 2500 + LINK_TIMEOUT_MS);
}

// --- Loop clock wrap ---

TEST_F(LD2450LinkTest, LossFiresOnTimeAcrossTheClockWrap) {
  const uint32_t last_ms = 0xFFFFFF00u;
  this->frame_(last_ms);
  EXPECT_EQ(this->lost_at_(last_ms + 1, last_ms + 2 * LINK_TIMEOUT_MS), last_ms + LINK_TIMEOUT_MS);
}

TEST_F(LD2450LinkTest, WindowEndIsRightAcrossTheClockWrap) {
  const uint32_t last_ms = 0xFFFFF000u;
  this->frame_(last_ms);
  this->open_window_(last_ms + 100);
  EXPECT_EQ(this->lost_at_(last_ms + 100, last_ms + 100 + LINK_GRACE_MS + 100), last_ms + 100 + LINK_GRACE_MS);
}

TEST_F(LD2450LinkTest, BootWindowIsRightAcrossTheClockWrap) {
  const uint32_t boot_ms = 0xFFFFF000u;
  this->act_(boot_ms, [this]() { this->ld2450_.setup(); });
  EXPECT_EQ(this->lost_at_(boot_ms, boot_ms + LINK_GRACE_MS + 2 * LINK_TIMEOUT_MS),
            boot_ms + LINK_GRACE_MS + LINK_TIMEOUT_MS);
}

}  // namespace esphome::ld2450::testing
