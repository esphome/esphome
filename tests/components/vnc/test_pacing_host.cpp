#include "vnc_test_helpers.h"

#ifdef USE_HOST

namespace esphome::vnc::testing {

static Bytes frame(uint16_t x = 0, uint16_t pixel = 0x1234) {
  return Rfb().fbu(1).raw_rect(x, 0, 2, 2, {pixel, pixel, pixel, pixel}).bytes;
}

static const Bytes INCREMENTAL = fbur(true, 64, 48);
static const Bytes FULL = fbur(false, 64, 48);

// The session starts at t=1000, when the first (full) request goes out.
class VncPacing : public ::testing::Test {
 protected:
  void SetUp() override {
    this->s_ = std::make_unique<Session>(&this->d_);
    this->s_->start(64, 48);
  }
  Session &s() { return *this->s_; }
  FakeDisplay d_;
  std::unique_ptr<Session> s_;
};

TEST_F(VncPacing, UpdateOnlyAfterTheScreenStopsChanging) {
  set_now(1100);
  s().deliver(frame());
  EXPECT_EQ(d_.updates(), 0);
  s().at(1299);
  EXPECT_EQ(d_.updates(), 0);
  s().at(1300);
  EXPECT_EQ(d_.updates(), 1);
  s().run_for(5000);
  EXPECT_EQ(d_.updates(), 1);
}

TEST_F(VncPacing, UpdateHasItsOwnPass) {
  set_now(1100);
  s().deliver(frame(0));
  s().send(frame(10, 0x4321));
  ASSERT_GT(s().unread(), 0);
  s().at(1300);
  EXPECT_EQ(d_.updates(), 1);
  // Nothing was read in the pass that refreshed the display.
  EXPECT_GT(s().unread(), 0);
  EXPECT_EQ(d_.pixel(10, 0), UNSET);
  s().loop();
  EXPECT_EQ(s().unread(), 0);
  EXPECT_EQ(d_.pixel(10, 0), expect_px(0x4321, 2));
}

TEST_F(VncPacing, FastFramesPostponeTheUpdateUntilTheMaximumDelay) {
  // A frame every 100 ms: never quiet for 200 ms, so only the 1 s limit forces a refresh.
  std::vector<uint32_t> update_times;
  int last = 0;
  for (uint32_t t = 1100; t <= 2500; t += 100) {
    s().send(frame(t % 60));
    s().at(t);
    if (d_.updates() != last) {
      update_times.push_back(t);
      last = d_.updates();
      s().loop();
    }
    EXPECT_EQ(s().unread(), 0);
  }
  EXPECT_EQ(update_times, std::vector<uint32_t>({2100}));
  s().at(2699);
  EXPECT_EQ(d_.updates(), 1);
  s().at(2700);
  EXPECT_EQ(d_.updates(), 2);
}

TEST_F(VncPacing, NoUpdateInTheMiddleOfAFrameEvenPastTheMaximumDelay) {
  set_now(1100);
  s().deliver(frame());
  Bytes second = Rfb().fbu(1).raw_rect(0, 10, 4, 1, {1, 2, 3, 4}).bytes;
  set_now(1200);
  s().deliver(Bytes(second.begin(), second.end() - 3));
  ASSERT_TRUE(s().client.in_update());
  s().run_for(1400);
  EXPECT_EQ(d_.updates(), 0);
  s().deliver(Bytes(second.end() - 3, second.end()));
  EXPECT_FALSE(s().client.in_update());
  EXPECT_EQ(d_.updates(), 0);
  s().tick(10);
  EXPECT_EQ(d_.updates(), 1);
}

TEST(VncPacingDelays, CustomDelays) {
  FakeDisplay d;
  Session s(&d);
  s.client.set_update_delay(50);
  s.client.set_max_update_delay(300);
  s.start(64, 48);
  set_now(1100);
  s.deliver(frame());
  s.at(1149);
  EXPECT_EQ(d.updates(), 0);
  s.at(1150);
  EXPECT_EQ(d.updates(), 1);
  // Frames every 40 ms: the 300 ms limit applies.
  int updates = 1;
  uint32_t first_forced = 0;
  for (uint32_t t = 1200; t < 1700; t += 40) {
    s.send(frame(t % 60));
    s.at(t);
    if (d.updates() != updates && first_forced == 0) {
      first_forced = t;
    }
    updates = d.updates();
    s.loop();
  }
  EXPECT_EQ(first_forced, 1520u);
}

TEST(VncPacingDelays, ZeroDelayRefreshesOnTheNextPass) {
  FakeDisplay d;
  Session s(&d);
  s.client.set_update_delay(0);
  s.start(64, 48);
  set_now(1100);
  s.deliver(frame());
  s.loop();
  EXPECT_EQ(d.updates(), 1);
}

TEST_F(VncPacing, NextRequestRightAfterEachFrameWithA50msFloor) {
  set_now(1010);
  s().deliver(frame());
  EXPECT_EQ(s().take().size(), 0u);
  s().at(1049);
  EXPECT_EQ(s().take().size(), 0u);
  s().at(1050);
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  set_now(1500);
  s().deliver(frame());
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  // One request stays outstanding; nothing more until the next frame or the heartbeat.
  s().run_for(50000, 100);
  EXPECT_EQ(s().take().size(), 0u);
}

TEST_F(VncPacing, CursorOnlyFrameNeverRefreshes) {
  set_now(1100);
  s().deliver(Rfb().fbu(1).cursor(1, 1, 4, 4));
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  s().run_for(5000);
  EXPECT_EQ(d_.updates(), 0);
}

TEST_F(VncPacing, RefreshSendsAFullRequestEvenWithOneOutstanding) {
  set_now(1100);
  s().client.refresh();
  s().loop();
  EXPECT_EQ(hex(s().take()), hex(FULL));
  s().run_for(1000);
  EXPECT_EQ(s().take().size(), 0u);
}

TEST_F(VncPacing, RefreshKeepsThe50msFloor) {
  set_now(1100);
  s().deliver(frame());
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  s().client.refresh();
  s().at(1149);
  EXPECT_EQ(s().take().size(), 0u);
  s().at(1150);
  EXPECT_EQ(hex(s().take()), hex(FULL));
}

TEST_F(VncPacing, RefreshDuringAFrameWaitsForItsEnd) {
  Bytes f = Rfb().fbu(1).raw_rect(0, 0, 4, 1, {1, 2, 3, 4}).bytes;
  set_now(1100);
  s().deliver(Bytes(f.begin(), f.end() - 2));
  s().client.refresh();
  s().run_for(500);
  EXPECT_EQ(s().take().size(), 0u);
  s().deliver(Bytes(f.end() - 2, f.end()));
  // Only the full request: the incremental one it replaces is not sent as well.
  EXPECT_EQ(hex(s().take()), hex(FULL));
  s().run_for(500);
  EXPECT_EQ(s().take().size(), 0u);
}

TEST_F(VncPacing, HeartbeatAfterAMinuteOfSilence) {
  set_now(1100);
  s().deliver(frame());
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  while (now_ms() + 1000 < 61099) {
    s().tick(1000);
  }
  EXPECT_EQ(s().take().size(), 0u);
  s().at(61099);
  EXPECT_EQ(s().take().size(), 0u);
  s().at(61100);
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  s().at(121099);
  EXPECT_EQ(s().take().size(), 0u);
  s().at(121100);
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
}

TEST_F(VncPacing, DataResetsTheHeartbeat) {
  set_now(1100);
  s().deliver(frame());
  s().take();
  set_now(40000);
  s().deliver(Rfb().bell());
  s().at(61100);
  EXPECT_EQ(s().take().size(), 0u);
  s().at(100000);
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
}

TEST_F(VncPacing, ServerSilentMidFrameEndsTheSession) {
  set_now(1100);
  s().deliver(frame());
  s().take();
  Bytes partial = frame();
  partial.pop_back();
  s().deliver(partial);
  ASSERT_TRUE(s().client.in_update());
  s().at(1100 + 59999);
  EXPECT_EQ(s().client.state(), VncState::VNC_STATE_RUNNING);
  s().at(1100 + 60000);
  EXPECT_EQ(s().client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_FALSE(s().client.link().connected());
}

// --- A display that parks its loop when idle (it8951, epaper_spi).

class VncPacingParked : public ::testing::Test {
 protected:
  void SetUp() override {
    this->s_ = std::make_unique<Session>(&this->d_);
    this->s_->start(64, 48);
  }
  Session &s() { return *this->s_; }
  FakeParkingDisplay d_;
  std::unique_ptr<Session> s_;
};

TEST_F(VncPacingParked, BusyDisplayHoldsDrawsAndRequests) {
  set_now(1100);
  s().deliver(frame());
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  s().at(1300);
  ASSERT_EQ(d_.updates(), 1);
  ASSERT_TRUE(d_.busy());

  // The server answers the outstanding request while the panel refreshes.
  s().send(frame(20, 0x5555));
  s().at(1310);
  EXPECT_TRUE(s().client.display_busy());
  EXPECT_TRUE(s().client.in_update());
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
  s().run_for(3000);
  EXPECT_EQ(d_.draws_while_busy(), 0);
  EXPECT_EQ(d_.pixel(20, 0), UNSET);
  EXPECT_EQ(s().take().size(), 0u);

  d_.finish();
  s().tick(10);
  EXPECT_EQ(d_.pixel(20, 0), expect_px(0x5555, 2));
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  s().run_for(300);
  EXPECT_EQ(d_.updates(), 2);
}

// The pass that starts a refresh already sees the panel busy, so it sends no request while the refresh runs.
TEST_F(VncPacingParked, NoRequestInThePassThatStartsARefresh) {
  set_now(1100);
  s().deliver(frame());
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  // vnc.refresh from a button handled earlier in the same main loop iteration.
  s().client.refresh();
  s().at(1300);
  ASSERT_EQ(d_.updates(), 1);
  ASSERT_TRUE(d_.busy());
  EXPECT_EQ(s().take().size(), 0u) << "request sent while the panel refreshes";
  d_.finish();
  s().tick(10);
  EXPECT_EQ(hex(s().take()), hex(FULL));
}

TEST(VncPacingParkedDelay, NoRequestInTheRefreshPassWithZeroDelay) {
  FakeParkingDisplay d;
  Session s(&d);
  s.client.set_update_delay(0);
  s.start(64, 48);
  // The frame ends 20 ms after the request, inside the 50 ms floor, so the next request waits.
  set_now(1020);
  s.deliver(frame());
  EXPECT_EQ(s.take().size(), 0u);
  s.at(1060);
  ASSERT_EQ(d.updates(), 1);
  ASSERT_TRUE(d.busy());
  EXPECT_EQ(s.take().size(), 0u) << "request sent while the panel refreshes";
  d.finish();
  s.tick(10);
  EXPECT_EQ(hex(s.take()), hex(INCREMENTAL));
}

TEST_F(VncPacingParked, FailedDisplayEndsTheSessionAndStops) {
  set_now(1100);
  s().deliver(frame());
  s().take();
  s().at(1300);
  ASSERT_EQ(d_.updates(), 1);
  d_.mark_failed();
  s().tick(10);
  EXPECT_EQ(s().client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_FALSE(s().client.link().connected());
  EXPECT_TRUE(s().client.status_has_error());
  EXPECT_TRUE(s().client.is_idle());
}

TEST_F(VncPacingParked, NoRequestWhileBusyBetweenFrames) {
  set_now(1100);
  s().deliver(Rfb().fbu(0));
  EXPECT_EQ(hex(s().take()), hex(INCREMENTAL));
  d_.external_update();
  s().client.refresh();
  s().run_for(2000);
  EXPECT_EQ(s().take().size(), 0u);
  d_.finish();
  s().tick(10);
  EXPECT_EQ(hex(s().take()), hex(FULL));
}

TEST_F(VncPacingParked, ExternalUpdateMidFrameStopsReading) {
  Bytes f = Rfb().fbu(2).raw_rect(0, 0, 2, 1, {1, 2}).raw_rect(0, 5, 2, 1, {3, 4}).bytes;
  set_now(1100);
  s().deliver(Bytes(f.begin(), f.begin() + 4 + 12 + 4 + 6));
  ASSERT_TRUE(s().client.in_update());
  d_.external_update();
  s().send(Bytes(f.begin() + 4 + 12 + 4 + 6, f.end()));
  s().run_for(1000);
  EXPECT_GT(s().unread(), 0);
  EXPECT_EQ(d_.draws_while_busy(), 0);
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
  d_.finish();
  s().tick(10);
  EXPECT_EQ(s().unread(), 0);
  EXPECT_EQ(d_.pixel(1, 5), expect_px(4, 2));
  EXPECT_FALSE(s().client.in_update());
}

// --- Displays that never report busy must never stall the client.

template<typename D> static void runs_frames_without_deadlock() {
  D d;
  Session s(&d);
  s.start(64, 48);
  for (int i = 0; i < 5; i++) {
    set_now(now_ms() + 300);
    s.deliver(frame(i * 4, 0x1000 + i));
    EXPECT_EQ(hex(s.take()), hex(INCREMENTAL)) << "frame " << i;
    s.run_for(300);
    EXPECT_EQ(d.updates(), i + 1) << "frame " << i;
    EXPECT_EQ(d.pixel(i * 4, 0), expect_px(0x1000 + i, 2));
  }
  s.client.refresh();
  s.tick(100);
  EXPECT_EQ(hex(s.take()), hex(FULL));
}

TEST(VncPacingDisplays, PlainDisplay) { runs_frames_without_deadlock<FakeDisplay>(); }
TEST(VncPacingStart, FirstRefreshWaitsForAStartingDisplayToPark) {
  FakeStartingDisplay d;
  Session s(&d);
  s.start(64, 48);
  set_now(1100);
  s.deliver(frame());
  s.at(1300);
  EXPECT_EQ(d.updates(), 0);
  d.finish_start();
  s.tick(10);
  ASSERT_EQ(d.updates(), 1);
  // Known to park now, so nothing is drawn until this refresh is over.
  s.deliver(frame(10));
  EXPECT_EQ(d.draws_while_busy(), 0);
  d.finish();
  s.tick(10);
  EXPECT_EQ(d.pixel(10, 0), expect_px(0x1234, 2));
}

TEST(VncPacingStart, LoopingDisplayRefreshesAfterTheStartWindow) {
  FakeLoopingDisplay d;
  Session s(&d);
  s.start(64, 48);
  set_now(1100);
  s.deliver(frame());
  s.at(1000 + 4999);
  EXPECT_EQ(d.updates(), 0);
  s.at(1000 + 5000);
  EXPECT_EQ(d.updates(), 1);
}

TEST(VncPacingDisplays, FreeRunningDisplay) { runs_frames_without_deadlock<FakeFreeRunDisplay>(); }
TEST(VncPacingDisplays, SynchronousParkedDisplay) { runs_frames_without_deadlock<FakeSyncParkedDisplay>(); }

TEST_F(VncPacingParked, FailedDisplayClosesAConnectStillInFlight) {
  s().client.forget_session();
  ASSERT_TRUE(s().client.link().connected());
  d_.mark_failed();
  s().tick(10);
  EXPECT_FALSE(s().client.link().connected());
  EXPECT_TRUE(s().client.status_has_error());
}

TEST_F(VncPacingParked, DisplayBusyTooLongSetsAWarningUntilItParks) {
  d_.external_update();
  s().run_for(120000, 1000);
  EXPECT_TRUE(s().client.status_has_warning());
  d_.finish();
  s().tick(10);
  EXPECT_FALSE(s().client.status_has_warning());
}

}  // namespace esphome::vnc::testing

#endif  // USE_HOST
