#include "vnc_test_helpers.h"

#include "esphome/components/touchscreen/touchscreen.h"

#ifdef USE_HOST

namespace esphome::vnc::testing {

static constexpr uint32_t KEYSYM_RETURN = 0xFF0D;
static constexpr uint32_t KEYSYM_SHIFT_L = 0xFFE1;
static constexpr uint32_t KEYSYM_CONTROL_L = 0xFFE3;

class VncInput : public ::testing::Test {
 protected:
  void SetUp() override { this->begin(64, 48, 64, 48); }
  void begin(int disp_w, int disp_h, uint16_t fb_w, uint16_t fb_h) {
    this->s_.reset();
    this->d_ = std::make_unique<FakeDisplay>(disp_w, disp_h);
    this->s_ = std::make_unique<Session>(this->d_.get());
    this->s_->start(fb_w, fb_h);
  }
  Session &s() { return *this->s_; }
  TestVncClient &c() { return this->s_->client; }
  std::unique_ptr<FakeDisplay> d_;
  std::unique_ptr<Session> s_;
};

TEST_F(VncInput, PointerEventBytes) {
  c().send_pointer(10, 20, 1);
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 10, 20)));
  c().send_pointer(63, 47, 8 | 16);
  EXPECT_EQ(hex(s().take()), hex(pointer(24, 63, 47)));
  c().send_pointer(1000, 1000, 0);
  EXPECT_EQ(hex(s().take()), hex(pointer(0, 63, 47)));
}

TEST_F(VncInput, KeyEventBytes) {
  c().send_key(KEYSYM_RETURN, VncKeyAction::VNC_KEY_ACTION_DOWN);
  EXPECT_EQ(hex(s().take()), hex({4, 1, 0, 0, 0, 0, 0xFF, 0x0D}));
  c().send_key(KEYSYM_RETURN, VncKeyAction::VNC_KEY_ACTION_UP);
  EXPECT_EQ(hex(s().take()), hex({4, 0, 0, 0, 0, 0, 0xFF, 0x0D}));
  c().send_key(0x01000000 | 0x20AC, VncKeyAction::VNC_KEY_ACTION_PRESS);
  EXPECT_EQ(hex(s().take()), hex(press(0x010020AC)));
}

TEST_F(VncInput, TypeText) {
  c().type_text("aB\n", 3);
  EXPECT_EQ(hex(s().take()), hex(cat({press('a'), press('B'), press(KEYSYM_RETURN)})));
  c().type_text(std::string("\t\b"));
  EXPECT_EQ(hex(s().take()), hex(cat({press(0xFF09), press(0xFF08)})));
  // Other control characters have no key.
  c().type_text(std::string("x\x01y\x7F"));
  EXPECT_EQ(hex(s().take()), hex(cat({press('x'), press('y')})));
}

TEST_F(VncInput, TypeTextUtf8) {
  c().type_text(std::string("\xC3\xA9\xE2\x82\xAC"));
  EXPECT_EQ(hex(s().take()), hex(cat({press(0xE9), press(0x010020AC)})));
  c().type_text(std::string("\xF0\x9F\x98\x80"));
  EXPECT_EQ(hex(s().take()), hex(press(0x0101F600)));
}

TEST_F(VncInput, TypeTextSkipsBrokenUtf8) {
  // A lead byte without its continuation, a stray continuation, and a cut-off sequence.
  c().type_text(std::string("\xC3(\x80"
                            "a\xE2\x82"));
  EXPECT_EQ(hex(s().take()), hex(cat({press('('), press('a')})));
}

// A cut-off sequence ends at the first byte that is not a continuation byte, so "\xE2a" still types the 'a'.
TEST_F(VncInput, TruncatedUtf8KeepsTheNextCharacter) {
  c().type_text(std::string("\xE2"
                            "a"));
  EXPECT_EQ(hex(s().take()), hex(press('a')));
  c().type_text(std::string("x\xF0"
                            "yz"));
  EXPECT_EQ(hex(s().take()), hex(cat({press('x'), press('y'), press('z')})));
}

TEST_F(VncInput, InvalidUtf8ValuesAreSkipped) {
  // Overlong 'A', a surrogate, and a value above U+10FFFF; U+10FFFF itself is kept.
  c().type_text(std::string("\xC1\x81"
                            "a\xED\xA0\x80"
                            "b\xF4\x90\x80\x80"
                            "c\xF4\x8F\xBF\xBF"));
  EXPECT_EQ(hex(s().take()), hex(cat({press('a'), press('b'), press('c'), press(0x0110FFFF)})));
}

TEST_F(VncInput, LengthCutsAMultiByteCharacter) {
  c().type_text("a\xC3\xA9", 2);
  EXPECT_EQ(hex(s().take()), hex(press('a')));
  c().type_text("\xC3\xA9", 1);
  EXPECT_EQ(s().take().size(), 0u);
}

TEST_F(VncInput, EmbeddedNulIsSkipped) {
  c().type_text("a\0b", 3);
  EXPECT_EQ(hex(s().take()), hex(cat({press('a'), press('b')})));
}

// --- A full send buffer never gets a partial message.

class VncInputFull : public VncInput {
 protected:
  /// Fills the socket, then the client's buffer up to `room` free bytes.
  void fill(size_t room) {
    uint8_t junk[1024];
    std::memset(junk, 0xEE, sizeof(junk));
    for (;;) {
      ssize_t n = ::write(s().client_fd, junk, sizeof(junk));
      if (n <= 0) {
        break;
      }
      this->expected_.insert(this->expected_.end(), junk, junk + n);
    }
    while (::write(s().client_fd, junk, 1) == 1) {
      this->expected_.push_back(0xEE);
    }
    ASSERT_GE(c().link().tx_free(), room);
    Bytes filler(c().link().tx_free() - room, 0xDD);
    ASSERT_EQ(c().link().queue(filler.data(), filler.size()), filler.size());
    this->expected_.insert(this->expected_.end(), filler.begin(), filler.end());
    ASSERT_EQ(c().link().tx_free(), room);
  }
  /// Everything that reaches the server once it reads again.
  Bytes drain() {
    Bytes all;
    for (int idle = 0; idle < 3;) {
      Bytes more = s().take();
      all.insert(all.end(), more.begin(), more.end());
      idle = more.empty() ? idle + 1 : 0;
    }
    return all;
  }
  Bytes expected_;
};

TEST_F(VncInputFull, PointerDroppedWhole) {
  this->fill(5);
  c().send_pointer(1, 2, 1);
  c().send_key('a', VncKeyAction::VNC_KEY_ACTION_DOWN);
  EXPECT_EQ(hex(this->drain()), hex(this->expected_));
}

TEST_F(VncInputFull, PressNeedsRoomForBothHalves) {
  this->fill(15);
  c().send_key('a', VncKeyAction::VNC_KEY_ACTION_PRESS);
  c().send_key('b', VncKeyAction::VNC_KEY_ACTION_DOWN);
  EXPECT_EQ(hex(this->drain()), hex(cat({this->expected_, key(true, 'b')})));
}

TEST_F(VncInputFull, DroppedKeyUpIsSentOnceThereIsRoom) {
  this->fill(5);
  c().send_key(KEYSYM_SHIFT_L, VncKeyAction::VNC_KEY_ACTION_UP);
  EXPECT_EQ(hex(this->drain()), hex(this->expected_));
  s().loop();
  EXPECT_EQ(hex(s().take()), hex(key(false, KEYSYM_SHIFT_L)));
}

TEST_F(VncInputFull, LaterInputWaitsForTheDroppedKeyUp) {
  this->fill(5);
  c().send_key(KEYSYM_SHIFT_L, VncKeyAction::VNC_KEY_ACTION_UP);
  EXPECT_EQ(hex(this->drain()), hex(this->expected_));
  c().send_key('a', VncKeyAction::VNC_KEY_ACTION_PRESS);
  c().send_pointer(3, 4, 0);
  EXPECT_EQ(hex(s().take()), hex(cat({key(false, KEYSYM_SHIFT_L), press('a'), pointer(0, 3, 4)})));
}

TEST_F(VncInputFull, ClickReleaseWaitsForRoom) {
  this->fill(6);
  c().send_pointer(5, 5, 1);
  c().send_pointer(5, 5, 0);
  EXPECT_EQ(hex(this->drain()), hex(cat({this->expected_, pointer(1, 5, 5)})));
  s().loop();
  EXPECT_EQ(hex(s().take()), hex(pointer(0, 5, 5)));
}

TEST_F(VncInputFull, BothHeldModifiersAreReleased) {
  this->fill(5);
  c().send_key(KEYSYM_SHIFT_L, VncKeyAction::VNC_KEY_ACTION_UP);
  c().send_key(KEYSYM_CONTROL_L, VncKeyAction::VNC_KEY_ACTION_UP);
  EXPECT_EQ(hex(this->drain()), hex(this->expected_));
  s().loop();
  EXPECT_EQ(hex(s().take()), hex(cat({key(false, KEYSYM_SHIFT_L), key(false, KEYSYM_CONTROL_L)})));
}

TEST_F(VncInputFull, ReleasesBeyondTheQueueAreDropped) {
  this->fill(5);
  for (uint32_t keysym = 'a'; keysym <= 'e'; keysym++) {
    c().send_key(keysym, VncKeyAction::VNC_KEY_ACTION_UP);
  }
  EXPECT_EQ(hex(this->drain()), hex(this->expected_));
  s().loop();
  EXPECT_EQ(hex(s().take()), hex(cat({key(false, 'a'), key(false, 'b'), key(false, 'c'), key(false, 'd')})));
}

TEST_F(VncInputFull, TextStopsAtAWholeCharacter) {
  this->fill(40);
  c().type_text(std::string("abcd"));
  EXPECT_EQ(hex(this->drain()), hex(cat({this->expected_, press('a'), press('b')})));
}

TEST_F(VncInputFull, DroppedReleaseIsSentOnceThereIsRoom) {
  c().handle_touch_(10, 10);
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 10, 10)));
  this->fill(3);
  c().handle_release_();
  EXPECT_EQ(hex(this->drain()), hex(this->expected_));
  s().loop();
  EXPECT_EQ(hex(s().take()), hex(pointer(0, 10, 10)));
}

TEST_F(VncInputFull, TouchPressRetriedWhenDropped) {
  this->fill(3);
  c().handle_touch_(10, 10);
  Bytes all = this->drain();
  EXPECT_EQ(hex(all), hex(this->expected_));
  c().handle_touch_(11, 11);
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 11, 11)));
}

TEST(VncInputState, DroppedWhenNotRunning) {
  FakeDisplay d;
  Session s(&d);
  s.attach();
  s.deliver(Rfb().version());
  s.take();
  s.client.send_pointer(1, 1, 1);
  s.client.send_key('a', VncKeyAction::VNC_KEY_ACTION_PRESS);
  s.client.type_text(std::string("abc"));
  s.client.handle_touch_(5, 5);
  s.client.handle_release_();
  s.loop();
  EXPECT_EQ(s.take().size(), 0u);
}

TEST(VncInputState, DroppedAfterTheSessionEnds) {
  FakeDisplay d;
  Session s(&d);
  s.start(64, 48);
  s.client.handle_touch_(5, 5);
  s.take();
  s.close_peer();
  s.loop();
  s.client.handle_release_();
  s.client.send_pointer(1, 1, 1);

  // The next session starts with no button held: the first touch is a press again.
  s.start(64, 48);
  s.client.handle_touch_(7, 8);
  EXPECT_EQ(hex(s.take()), hex(pointer(1, 7, 8)));
}

// --- Touch

TEST_F(VncInput, TouchPressIsSentAtOnce) {
  c().handle_touch_(10, 12);
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 10, 12)));
}

TEST_F(VncInput, TouchMovesInsideTheSlopAreSuppressed) {
  c().handle_touch_(20, 20);
  s().take();
  c().handle_touch_(25, 24);
  c().handle_touch_(28, 12);
  c().handle_touch_(12, 28);
  EXPECT_EQ(s().take().size(), 0u);
  c().handle_touch_(29, 20);
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 29, 20)));
  // Once dragging, every change goes out, even back inside the slop.
  c().handle_touch_(21, 20);
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 21, 20)));
  c().handle_touch_(21, 20);
  EXPECT_EQ(s().take().size(), 0u);
}

TEST_F(VncInput, TapReleasesWhereItPressed) {
  c().handle_touch_(10, 10);
  c().handle_touch_(13, 12);
  c().handle_release_();
  EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, 10, 10), pointer(0, 10, 10)})));
  c().handle_release_();
  EXPECT_EQ(s().take().size(), 0u);
}

TEST_F(VncInput, DragReleasesAtTheLastPosition) {
  c().handle_touch_(10, 10);
  c().handle_touch_(30, 30);
  c().handle_touch_(40, 35);
  c().handle_release_();
  EXPECT_EQ(hex(s().take()),
            hex(cat({pointer(1, 10, 10), pointer(1, 30, 30), pointer(1, 40, 35), pointer(0, 40, 35)})));
}

TEST_F(VncInput, EachTapStartsANewSlop) {
  c().handle_touch_(10, 10);
  c().handle_touch_(40, 10);
  c().handle_release_();
  s().take();
  c().handle_touch_(50, 10);
  c().handle_touch_(52, 10);
  c().handle_release_();
  EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, 50, 10), pointer(0, 50, 10)})));
}

TEST_F(VncInput, TouchIgnoresButtonsLeftByAnAutomation) {
  c().send_pointer(5, 5, 8 | 4);
  s().take();
  c().handle_touch_(10, 10);
  c().handle_release_();
  EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, 10, 10), pointer(0, 10, 10)})));
}

TEST_F(VncInput, TouchPressesEvenWhileAnAutomationHoldsLeft) {
  c().send_pointer(5, 5, 1);
  s().take();
  c().handle_touch_(3, 3);
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 3, 3)));
}

TEST_F(VncInput, PressOutsideTheVisibleAreaIsIgnored) {
  this->begin(64, 48, 100, 80);
  c().handle_touch_(64, 10);
  c().handle_touch_(10, 48);
  c().handle_release_();
  EXPECT_EQ(s().take().size(), 0u);
  c().handle_touch_(63, 47);
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 63, 47)));
}

TEST_F(VncInput, PressOutsideASmallerServerScreenIsIgnored) {
  this->begin(64, 48, 32, 20);
  c().handle_touch_(40, 10);
  EXPECT_EQ(s().take().size(), 0u);
  c().handle_touch_(31, 19);
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 31, 19)));
}

TEST_F(VncInput, DragIsClampedToTheVisibleArea) {
  this->begin(64, 48, 100, 80);
  c().handle_touch_(50, 30);
  c().handle_touch_(200, 300);
  c().handle_release_();
  EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, 50, 30), pointer(1, 63, 47), pointer(0, 63, 47)})));
}

TEST_F(VncInput, ClampedMoveInsideTheSlopStaysATap) {
  this->begin(64, 48, 100, 80);
  c().handle_touch_(60, 40);
  c().handle_touch_(200, 300);
  c().handle_release_();
  EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, 60, 40), pointer(0, 60, 40)})));
}

// The client never scales: a touch at display x,y is a pointer event at framebuffer x,y.
TEST_F(VncInput, TouchCoordinatesAreNotScaled) {
  this->begin(960, 540, 960, 540);
  const uint16_t points[][2] = {{0, 0}, {192, 108}, {480, 270}, {767, 431}, {900, 500}, {959, 539}, {959, 0}, {0, 539}};
  for (const auto &p : points) {
    c().handle_touch_(p[0], p[1]);
    c().handle_release_();
    EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, p[0], p[1]), pointer(0, p[0], p[1])}))) << p[0] << "," << p[1];
  }
}

TEST_F(VncInput, TouchCoordinatesAreNotScaledOnALargerServer) {
  this->begin(960, 540, 1280, 720);
  c().handle_touch_(900, 500);
  c().handle_touch_(950, 530);
  c().handle_release_();
  EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, 900, 500), pointer(1, 950, 530), pointer(0, 950, 530)})));
}

static touchscreen::TouchPoint point(uint8_t id, uint16_t x, uint16_t y, uint8_t state) {
  touchscreen::TouchPoint tp{};
  tp.id = id;
  tp.x = x;
  tp.y = y;
  tp.state = static_cast<int8_t>(state);
  return tp;
}

TEST_F(VncInput, TouchListenerSkipsStalePoints) {
  // Finger 0 was lifted earlier and lingers as RELEASED.
  c().update({point(0, 1, 1, touchscreen::STATE_RELEASED), point(1, 20, 21, touchscreen::STATE_PRESSED)});
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 20, 21)));
  // Finger 0 is being lifted now (still flagged PRESSED as well).
  c().update({point(0, 5, 5, touchscreen::STATE_PRESSED | touchscreen::STATE_RELEASING),
              point(1, 40, 21, touchscreen::STATE_UPDATED)});
  EXPECT_EQ(hex(s().take()), hex(pointer(1, 40, 21)));
  // A point from an uncalibrated touchscreen.
  c().update({point(0, 3, 3, touchscreen::STATE_CALIBRATE)});
  EXPECT_EQ(s().take().size(), 0u);
  c().update({});
  EXPECT_EQ(s().take().size(), 0u);
  c().release();
  EXPECT_EQ(hex(s().take()), hex(pointer(0, 40, 21)));
}

TEST_F(VncInput, TouchListenerUsesTheFirstActivePoint) {
  c().update({point(0, 10, 10, touchscreen::STATE_PRESSED), point(1, 50, 40, touchscreen::STATE_PRESSED)});
  c().update({point(0, 10, 11, touchscreen::STATE_UPDATED), point(1, 60, 40, touchscreen::STATE_UPDATED)});
  c().release();
  EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, 10, 10), pointer(0, 10, 10)})));
}

TEST_F(VncInput, TouchListenerPassesCoordinatesThrough) {
  this->begin(960, 540, 960, 540);
  c().update({point(0, 767, 431, touchscreen::STATE_PRESSED)});
  c().update({point(0, 192, 108, touchscreen::STATE_UPDATED)});
  c().release();
  EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, 767, 431), pointer(1, 192, 108), pointer(0, 192, 108)})));
}

// --- Through the real touchscreen state machine: raw controller points to pointer events.

class FakeTouchscreen : public touchscreen::Touchscreen {
 public:
  struct Raw {
    uint8_t id;
    int16_t x;
    int16_t y;
  };
  void attach(display::Display *display) {
    this->set_display(display);
    // What Touchscreen::call_setup() does.
    this->display_width_ = display->get_width();
    this->display_height_ = display->get_height();
  }
  /// One controller read with these fingers down, then the deferred listener calls.
  void report(std::vector<Raw> points) {
    this->points_ = std::move(points);
    this->store_.touched = true;
    this->loop();
    App.scheduler.call(millis());
  }

 protected:
  void update_touches() override {
    this->skip_update_ = false;
    for (const auto &p : this->points_) {
      this->add_raw_touch_position_(p.id, p.x, p.y);
    }
  }
  std::vector<Raw> points_;
};

/// The display position the touchscreen component computes for a raw value.
static uint16_t mapped(int raw, int raw_max, int size) {
  const int normalized = raw <= 0 ? 0 : (raw >= raw_max ? 0xfff : 0xfff * raw / raw_max);
  return static_cast<uint16_t>(normalized * size / 0x1000);
}

class VncTouchscreen : public VncInput {
 protected:
  void SetUp() override {
    // The test main does not construct App; the touchscreen defers its listener calls to the scheduler.
    static const bool APP_CONSTRUCTED = (new (&App) Application(), true);
    (void) APP_CONSTRUCTED;
    this->begin(960, 540, 960, 540);
    this->ts_.set_calibration(0, 960, 0, 540);
    this->ts_.attach(this->d_.get());
    this->c().register_touchscreen(&this->ts_);
  }
  FakeTouchscreen ts_;
};

TEST_F(VncTouchscreen, TapsLandWhereTheFingerIs) {
  const int16_t taps[][2] = {{10, 10}, {100, 50}, {480, 270}, {700, 400}, {900, 500}, {955, 535}};
  for (const auto &t : taps) {
    this->ts_.report({{0, t[0], t[1]}});
    this->ts_.report({});
    const uint16_t x = mapped(t[0], 960, 960);
    const uint16_t y = mapped(t[1], 540, 540);
    EXPECT_LE(std::abs(x - t[0]), 1);
    EXPECT_LE(std::abs(y - t[1]), 1);
    EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, x, y), pointer(0, x, y)}))) << t[0] << "," << t[1];
  }
}

TEST_F(VncTouchscreen, TapAtTheOrigin) {
  this->ts_.report({{0, 0, 0}});
  this->ts_.report({});
  EXPECT_EQ(hex(s().take()), hex(cat({pointer(1, 0, 0), pointer(0, 0, 0)})));
}

TEST_F(VncTouchscreen, DragFollowsTheFinger) {
  this->ts_.report({{0, 100, 100}});
  this->ts_.report({{0, 104, 103}});
  this->ts_.report({{0, 300, 200}});
  this->ts_.report({{0, 600, 400}});
  this->ts_.report({});
  const uint16_t x0 = mapped(100, 960, 960), y0 = mapped(100, 540, 540);
  const uint16_t x1 = mapped(300, 960, 960), y1 = mapped(200, 540, 540);
  const uint16_t x2 = mapped(600, 960, 960), y2 = mapped(400, 540, 540);
  EXPECT_EQ(hex(s().take()),
            hex(cat({pointer(1, x0, y0), pointer(1, x1, y1), pointer(1, x2, y2), pointer(0, x2, y2)})));
}

TEST_F(VncTouchscreen, LiftedFirstFingerDoesNotPullThePointerBack) {
  this->ts_.report({{0, 100, 100}});
  this->ts_.report({{0, 100, 100}, {1, 500, 300}});
  // Finger 0 lifts; it lingers in the list as RELEASING, then RELEASED.
  this->ts_.report({{1, 510, 300}});
  this->ts_.report({{1, 520, 300}});
  this->ts_.report({});
  Bytes tx = s().take();
  ASSERT_GE(tx.size(), 12u);
  const Bytes release(tx.end() - 6, tx.end());
  EXPECT_EQ(hex(release), hex(pointer(0, mapped(520, 960, 960), mapped(300, 540, 540))));
  for (size_t i = 0; i + 6 <= tx.size(); i += 6) {
    const uint16_t x = (tx[i + 2] << 8) | tx[i + 3];
    EXPECT_TRUE(x == mapped(100, 960, 960) || x >= mapped(500, 960, 960)) << "event at x=" << x;
  }
}

}  // namespace esphome::vnc::testing

#endif  // USE_HOST
