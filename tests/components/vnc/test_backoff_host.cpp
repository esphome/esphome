#include "vnc_test_helpers.h"

#ifdef USE_HOST

namespace esphome::vnc::testing {

/// Runs one pass at time t, then lets an asynchronous connect finish before time moves on.
static void settle_at(Session &s, uint32_t t) {
  set_now(t);
  s.loop();
  for (int i = 0; i < 10000 && s.client.link().connecting(); i++) {
    usleep(100);
    s.loop();
  }
}

/// Times at which refused connect attempts ended, starting at the current time.
static std::vector<uint32_t> refused_attempts(Session &s, size_t count, uint32_t step = 100) {
  std::vector<uint32_t> times;
  uint32_t last_start = s.client.hold_start_ms();
  const uint32_t start = now_ms();
  for (uint32_t t = start; times.size() < count && t - start < 3600000; t += step) {
    settle_at(s, t);
    if (s.client.hold_start_ms() != last_start) {
      last_start = s.client.hold_start_ms();
      times.push_back(t);
    }
  }
  return times;
}

static std::vector<uint32_t> gaps(const std::vector<uint32_t> &times) {
  std::vector<uint32_t> out;
  for (size_t i = 1; i < times.size(); i++) {
    out.push_back(times[i] - times[i - 1]);
  }
  return out;
}

TEST(VncBackoff, RefusedConnectsBackOff) {
  FakeDisplay d;
  Session s(&d);
  auto times = refused_attempts(s, 7);
  ASSERT_EQ(times.size(), 7u);
  EXPECT_EQ(times[0], 1000u);
  EXPECT_EQ(gaps(times), std::vector<uint32_t>({5000, 10000, 20000, 40000, 60000, 60000}));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_TRUE(s.client.status_has_warning());
  EXPECT_FALSE(s.client.status_has_error());
}

TEST(VncBackoff, ShortReconnectInterval) {
  FakeDisplay d;
  Session s(&d);
  s.client.set_reconnect_interval(1000);
  auto times = refused_attempts(s, 9);
  EXPECT_EQ(gaps(times), std::vector<uint32_t>({1000, 2000, 4000, 8000, 16000, 32000, 60000, 60000}));
}

TEST(VncBackoff, ReconnectIntervalAboveTheCap) {
  FakeDisplay d;
  Session s(&d);
  s.client.set_reconnect_interval(90000);
  auto times = refused_attempts(s, 4, 1000);
  EXPECT_EQ(gaps(times), std::vector<uint32_t>({90000, 90000, 90000}));
}

TEST(VncBackoff, StableSessionResetsBackoff) {
  FakeDisplay d;
  Session s(&d);
  auto times = refused_attempts(s, 3);
  ASSERT_EQ(s.client.backoff_ms(), 20000u);

  set_now(times.back() + 1000);
  s.start(64, 48);
  s.run_for(60000, 1000);
  ASSERT_TRUE(s.client.is_connected());
  int disconnects = 0;
  s.client.add_on_disconnect_callback([&disconnects]() { disconnects++; });
  s.close_peer();
  s.loop();
  EXPECT_EQ(disconnects, 1);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.backoff_ms(), 0u);
  EXPECT_EQ(s.client.hold_ms(), 0u);

  const uint32_t dropped_at = now_ms();
  auto after = refused_attempts(s, 2);
  ASSERT_EQ(after.size(), 2u);
  // The link's own interval spaces the first retry; the backoff then starts over.
  EXPECT_EQ(after[0] - dropped_at, 5000u);
  EXPECT_EQ(after[1] - after[0], 5000u);
}

TEST(VncBackoff, ShortSessionKeepsEscalating) {
  FakeDisplay d;
  Session s(&d);
  auto times = refused_attempts(s, 3);
  ASSERT_EQ(s.client.backoff_ms(), 20000u);

  set_now(times.back() + 1000);
  s.start(64, 48);
  s.run_for(10000, 1000);
  s.close_peer();
  s.loop();
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 40000u);
}

TEST(VncBackoff, PeerCloseWhileRunningRetries) {
  FakeDisplay d;
  Session s(&d);
  int disconnects = 0;
  s.client.add_on_disconnect_callback([&disconnects]() { disconnects++; });
  s.start(64, 48);
  s.close_peer();
  s.loop();
  EXPECT_FALSE(s.client.is_connected());
  EXPECT_EQ(disconnects, 1);
  EXPECT_EQ(s.client.hold_ms(), 5000u);
  EXPECT_TRUE(s.client.status_has_warning());
  const uint32_t dropped_at = now_ms();
  auto after = refused_attempts(s, 1);
  ASSERT_EQ(after.size(), 1u);
  EXPECT_EQ(after[0] - dropped_at, 5000u);
}

TEST(VncBackoff, NextSessionStartsClean) {
  FakeDisplay d;
  Session s(&d);
  s.start(64, 48);
  set_now(now_ms() + 100);
  // Leave a frame half parsed and a refresh pending, then lose the link.
  s.deliver(Rfb().fbu(1).raw_rect(0, 0, 2, 1, {1, 2}));
  s.deliver(Rfb().fbu(1).rect(0, 0, 4, 4, 5).u8(0x02));
  ASSERT_TRUE(s.client.in_update());
  s.close_peer();
  s.loop();
  EXPECT_FALSE(s.client.in_update());
  EXPECT_FALSE(s.client.refresh_pending());

  s.attach();
  EXPECT_EQ(s.client.step(), VncStep::VNC_STEP_VERSION);
  EXPECT_EQ(s.client.rx_buffered(), 0);
  s.deliver(handshake_38(64, 48));
  ASSERT_TRUE(s.client.is_connected());
  s.take();
  set_now(now_ms() + 100);
  s.deliver(Rfb().fbu(1).raw_rect(3, 3, 1, 1, {0x7777}));
  EXPECT_EQ(d.pixel(3, 3), expect_px(0x7777, 2));
  EXPECT_EQ(hex(s.take()), hex(fbur(true, 64, 48)));
}

TEST(VncBackoff, ShutdownClosesTheLink) {
  FakeDisplay d;
  Session s(&d);
  s.start(64, 48);
  s.client.on_shutdown();
  EXPECT_FALSE(s.client.link().connected());
}

}  // namespace esphome::vnc::testing

#endif  // USE_HOST
