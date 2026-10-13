#include "vnc_test_helpers.h"

#include <random>

#ifdef USE_HOST

namespace esphome::vnc::testing {

static constexpr int32_t RAW = 0;
static constexpr int32_t HEXTILE = 5;

static std::vector<uint16_t> pattern(size_t count, uint16_t seed) {
  std::vector<uint16_t> out(count);
  for (size_t i = 0; i < count; i++) {
    out[i] = static_cast<uint16_t>(seed + i * 0x0841u);
  }
  return out;
}

/// Checks that a w x h block at x,y shows the given wire pixels, row-major.
static void expect_block(const FakeDisplay &d, int x, int y, int w, int h, const std::vector<uint16_t> &px,
                         uint8_t bpp) {
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      ASSERT_EQ(d.pixel(x + col, y + row), expect_px(px[row * w + col], bpp)) << "at " << x + col << "," << y + row;
    }
  }
}

static void expect_solid(const FakeDisplay &d, int x, int y, int w, int h, uint16_t px, uint8_t bpp) {
  expect_block(d, x, y, w, h, std::vector<uint16_t>(static_cast<size_t>(w) * h, px), bpp);
}

class VncDecode : public ::testing::Test {
 protected:
  void start(FakeDisplay &display, uint16_t fb_w, uint16_t fb_h) {
    this->session_ = std::make_unique<Session>(&display);
    this->session_->start(fb_w, fb_h);
    // Past the 50 ms request spacing, so the next request goes out as soon as a frame ends.
    set_now(now_ms() + 100);
  }
  Session &s() { return *this->session_; }
  std::unique_ptr<Session> session_;
};

TEST_F(VncDecode, Raw16bpp) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  auto px = pattern(12, 0x1234);
  s().deliver(Rfb().fbu(1).raw_rect(3, 2, 4, 3, px));
  expect_block(d, 3, 2, 4, 3, px, 2);
  EXPECT_EQ(d.drawn_count(), 12u);
  EXPECT_EQ(hex(s().take()), hex(fbur(true, 64, 48)));
  EXPECT_EQ(d.updates(), 0);
  s().run_for(300);
  EXPECT_EQ(d.updates(), 1);
}

TEST_F(VncDecode, Raw8bpp) {
  FakeDisplay d(64, 48, display::DISPLAY_TYPE_BINARY);
  this->start(d, 64, 48);
  std::vector<uint16_t> px = {0x00, 0xFF, 0xE0, 0x1C, 0x03, 0x92, 0x49, 0x24, 0xB6, 0x6D};
  s().deliver(Rfb().fbu(1).raw_rect(10, 20, 5, 2, px, 1));
  expect_block(d, 10, 20, 5, 2, px, 1);
  EXPECT_EQ(d.drawn_count(), 10u);
  EXPECT_TRUE(s().client.is_connected());
}

TEST_F(VncDecode, RawSeveralRectsInOneUpdate) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  auto a = pattern(6, 0x0101);
  auto b = pattern(4, 0xF00F);
  s().deliver(Rfb().fbu(2).raw_rect(0, 0, 3, 2, a).raw_rect(60, 46, 4, 1, b));
  expect_block(d, 0, 0, 3, 2, a, 2);
  expect_block(d, 60, 46, 4, 1, b, 2);
  EXPECT_EQ(hex(s().take()), hex(fbur(true, 64, 48)));
}

TEST_F(VncDecode, EmptyUpdateRequestsAgainWithoutRefresh) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  s().deliver(Rfb().fbu(0));
  EXPECT_EQ(hex(s().take()), hex(fbur(true, 64, 48)));
  s().run_for(2000);
  EXPECT_EQ(d.updates(), 0);
}

TEST_F(VncDecode, HextileRawTile) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  auto px = pattern(256, 0x0F0F);
  Rfb f;
  f.fbu(1).rect(16, 16, 16, 16, HEXTILE).u8(0x01);
  for (auto p : px) {
    f.px(p);
  }
  s().deliver(f);
  expect_block(d, 16, 16, 16, 16, px, 2);
  EXPECT_EQ(d.drawn_count(), 256u);
}

TEST_F(VncDecode, HextileRawTileIgnoresOtherFlags) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  auto px = pattern(4 * 3, 0x7777);
  Rfb f;
  // 0x1F: Raw plus every other bit. Only the pixels follow.
  f.fbu(2).rect(0, 0, 4, 3, HEXTILE).u8(0x1F);
  for (auto p : px) {
    f.px(p);
  }
  f.raw_rect(10, 10, 1, 1, {0xABCD});
  s().deliver(f);
  expect_block(d, 0, 0, 4, 3, px, 2);
  EXPECT_EQ(d.pixel(10, 10), expect_px(0xABCD, 2));
  EXPECT_TRUE(s().client.is_connected());
  EXPECT_EQ(hex(s().take()), hex(fbur(true, 64, 48)));
}

TEST_F(VncDecode, HextileBackgroundOnly) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  s().deliver(Rfb().fbu(1).rect(0, 0, 16, 16, HEXTILE).u8(0x02).px(0x1234));
  expect_solid(d, 0, 0, 16, 16, 0x1234, 2);
  EXPECT_EQ(d.drawn_count(), 256u);
}

TEST_F(VncDecode, HextileForegroundSubrects) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  // bg 0x1111, fg 0x2222, two subrects: (1,2) 3x4 and (10,0) 6x1.
  s().deliver(Rfb()
                  .fbu(1)
                  .rect(0, 0, 16, 16, HEXTILE)
                  .u8(0x02 | 0x04 | 0x08)
                  .px(0x1111)
                  .px(0x2222)
                  .u8(2)
                  .u8(0x12)
                  .u8(0x23)
                  .u8(0xA0)
                  .u8(0x50));
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      const bool fg = (x >= 1 && x < 4 && y >= 2 && y < 6) || (x >= 10 && y == 0);
      ASSERT_EQ(d.pixel(x, y), expect_px(fg ? 0x2222 : 0x1111, 2)) << x << "," << y;
    }
  }
}

TEST_F(VncDecode, HextileColouredSubrects) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  s().deliver(Rfb()
                  .fbu(1)
                  .rect(16, 0, 16, 16, HEXTILE)
                  .u8(0x02 | 0x08 | 0x10)
                  .px(0x0001)
                  .u8(2)
                  .px(0xF800)
                  .u8(0x00)
                  .u8(0x11)
                  .px(0x07E0)
                  .u8(0xFF)
                  .u8(0x00));
  expect_solid(d, 16, 0, 2, 2, 0xF800, 2);
  EXPECT_EQ(d.pixel(31, 15), expect_px(0x07E0, 2));
  EXPECT_EQ(d.pixel(18, 0), expect_px(0x0001, 2));
  EXPECT_EQ(d.pixel(30, 15), expect_px(0x0001, 2));
}

TEST_F(VncDecode, HextileCarriesColoursBetweenTiles) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  s().deliver(Rfb()
                  .fbu(1)
                  .rect(0, 0, 48, 16, HEXTILE)
                  // Tile 1: bg A, fg B, one subrect.
                  .u8(0x02 | 0x04 | 0x08)
                  .px(0xAAAA)
                  .px(0xBBBB)
                  .u8(1)
                  .u8(0x00)
                  .u8(0x00)
                  // Tile 2: nothing specified, so solid A.
                  .u8(0x00)
                  // Tile 3: subrects in the carried fg B on the carried bg A.
                  .u8(0x08)
                  .u8(1)
                  .u8(0xFF)
                  .u8(0x00));
  EXPECT_EQ(d.pixel(0, 0), expect_px(0xBBBB, 2));
  expect_solid(d, 1, 1, 15, 15, 0xAAAA, 2);
  expect_solid(d, 16, 0, 16, 16, 0xAAAA, 2);
  EXPECT_EQ(d.pixel(47, 15), expect_px(0xBBBB, 2));
  expect_solid(d, 32, 0, 15, 15, 0xAAAA, 2);
}

TEST_F(VncDecode, HextileCarriesColoursAcrossRawTile) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  auto raw = pattern(256, 0x0400);
  Rfb f;
  f.fbu(1).rect(0, 16, 48, 16, HEXTILE).u8(0x02 | 0x04).px(0xCCCC).px(0xDDDD).u8(0x01);
  for (auto p : raw) {
    f.px(p);
  }
  f.u8(0x08).u8(1).u8(0x00).u8(0x00);
  s().deliver(f);
  expect_solid(d, 0, 16, 16, 16, 0xCCCC, 2);
  expect_block(d, 16, 16, 16, 16, raw, 2);
  EXPECT_EQ(d.pixel(32, 16), expect_px(0xDDDD, 2));
  EXPECT_EQ(d.pixel(33, 16), expect_px(0xCCCC, 2));
  EXPECT_EQ(d.pixel(47, 31), expect_px(0xCCCC, 2));
}

TEST_F(VncDecode, HextileSingleEdgeTile) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  auto px = pattern(13 * 7, 0x3000);
  Rfb f;
  f.fbu(1).rect(5, 3, 13, 7, HEXTILE).u8(0x01);
  for (auto p : px) {
    f.px(p);
  }
  s().deliver(f);
  expect_block(d, 5, 3, 13, 7, px, 2);
  EXPECT_EQ(d.drawn_count(), 13u * 7u);
}

TEST_F(VncDecode, HextileTileGrid) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  // 37x21 is 3x2 tiles: 16, 16, 5 wide and 16, 5 tall.
  Rfb f;
  f.fbu(1).rect(2, 1, 37, 21, HEXTILE);
  for (int i = 0; i < 6; i++) {
    f.u8(0x02).px(0x1000 + i);
  }
  s().deliver(f);
  const int xs[] = {2, 18, 34};
  const int ws[] = {16, 16, 5};
  const int ys[] = {1, 17};
  const int hs[] = {16, 5};
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 3; col++) {
      expect_solid(d, xs[col], ys[row], ws[col], hs[row], 0x1000 + row * 3 + col, 2);
    }
  }
  EXPECT_EQ(d.drawn_count(), 37u * 21u);
}

TEST_F(VncDecode, HextileSubrectClippedToTile) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  // A 5x5 edge tile with a subrect at 3,3 claiming 16x16, and one starting outside the tile.
  s().deliver(Rfb()
                  .fbu(1)
                  .rect(59, 43, 5, 5, HEXTILE)
                  .u8(0x02 | 0x04 | 0x08)
                  .px(0x0101)
                  .px(0xFFFF)
                  .u8(2)
                  .u8(0x33)
                  .u8(0xFF)
                  .u8(0x90)
                  .u8(0x00));
  for (int y = 0; y < 5; y++) {
    for (int x = 0; x < 5; x++) {
      const bool fg = x >= 3 && y >= 3;
      ASSERT_EQ(d.pixel(59 + x, 43 + y), expect_px(fg ? 0xFFFF : 0x0101, 2)) << x << "," << y;
    }
  }
  EXPECT_EQ(d.drawn_count(), 25u);
  EXPECT_TRUE(s().client.is_connected());
}

TEST_F(VncDecode, Hextile8bpp) {
  FakeDisplay d(64, 48, display::DISPLAY_TYPE_BINARY);
  this->start(d, 64, 48);
  auto raw = pattern(4, 0x00E3);
  Rfb f;
  f.fbu(1).rect(0, 0, 18, 2, HEXTILE).u8(0x02 | 0x04 | 0x08).px(0x00, 1).px(0xFF, 1).u8(1).u8(0x10).u8(0x00).u8(0x01);
  for (auto p : raw) {
    f.px(p & 0xFF, 1);
  }
  s().deliver(f);
  EXPECT_EQ(d.pixel(0, 0), expect_px(0x00, 1));
  EXPECT_EQ(d.pixel(1, 0), expect_px(0xFF, 1));
  EXPECT_EQ(d.pixel(1, 1), expect_px(0x00, 1));
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(d.pixel(16 + i % 2, i / 2), expect_px(raw[i] & 0xFF, 1));
  }
}

TEST_F(VncDecode, RichCursorIsSkippedAndNeverRefreshes) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  s().deliver(Rfb().fbu(1).cursor(3, 4, 5, 4));
  EXPECT_EQ(d.draw_calls(), 0);
  EXPECT_EQ(hex(s().take()), hex(fbur(true, 64, 48)));
  s().run_for(3000);
  EXPECT_EQ(d.updates(), 0);
  EXPECT_TRUE(s().client.is_connected());
}

class VncCursorSkip : public ::testing::TestWithParam<display::DisplayType> {};

TEST_P(VncCursorSkip, PayloadLengthMatchesThePixelFormat) {
  FakeDisplay d(64, 48, GetParam());
  Session s(&d);
  s.start(64, 48);
  const uint8_t bpp = s.client.bpp();
  // 9 wide needs a 2-byte mask row; the raw rect after it only lands right if the skip was exact.
  s.deliver(Rfb().fbu(3).cursor(0, 0, 9, 3, bpp).raw_rect(7, 7, 2, 1, {0x00E0, 0x001C}, bpp).cursor(1, 1, 0, 0, bpp));
  EXPECT_EQ(d.pixel(7, 7), expect_px(0x00E0, bpp));
  EXPECT_EQ(d.pixel(8, 7), expect_px(0x001C, bpp));
  EXPECT_EQ(d.drawn_count(), 2u);
  EXPECT_TRUE(s.client.is_connected());
  EXPECT_FALSE(s.client.in_update());
}

INSTANTIATE_TEST_SUITE_P(Formats, VncCursorSkip,
                         ::testing::Values(display::DISPLAY_TYPE_COLOR, display::DISPLAY_TYPE_BINARY));

TEST_F(VncDecode, ZeroSizeCursorEndsTheFrame) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  s().deliver(Rfb().fbu(1).cursor(0, 0, 0, 0));
  EXPECT_FALSE(s().client.in_update());
  EXPECT_EQ(hex(s().take()), hex(fbur(true, 64, 48)));
}

TEST_F(VncDecode, OversizedCursorReconnects) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  s().deliver(Rfb().fbu(1).rect(0, 0, 1025, 1, -239));
  EXPECT_EQ(s().client.state(), VncState::VNC_STATE_IDLE);
}

TEST_F(VncDecode, LargeCutTextIsSkipped) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  s().deliver(Rfb().cut_text(200 * 1024).fbu(1).raw_rect(1, 1, 1, 1, {0x4321}));
  EXPECT_TRUE(s().client.is_connected());
  EXPECT_EQ(d.pixel(1, 1), expect_px(0x4321, 2));
  EXPECT_EQ(d.drawn_count(), 1u);
}

TEST_F(VncDecode, BellAndColourMapAreSkipped) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  s().deliver(Rfb().bell().colour_map(5, 3).bell().colour_map(0, 0).fbu(1).raw_rect(2, 2, 1, 1, {0x0F0F}));
  EXPECT_TRUE(s().client.is_connected());
  EXPECT_EQ(d.pixel(2, 2), expect_px(0x0F0F, 2));
  EXPECT_EQ(d.drawn_count(), 1u);
}

TEST(VncDecodeClip, ServerLargerThanDisplay) {
  FakeDisplay d(64, 48);
  Session s(&d);
  s.attach();
  s.deliver(handshake_38(100, 80));
  ASSERT_TRUE(s.client.is_connected());
  Bytes tx = s.take();
  EXPECT_EQ(hex(Bytes(tx.end() - 10, tx.end())), hex(fbur(false, 64, 48)));
  set_now(now_ms() + 100);

  auto raw = pattern(90 * 70, 0x0021);
  Rfb f;
  f.fbu(3).raw_rect(5, 5, 90, 70, raw);
  // A hextile rect straddling both edges, and one entirely off screen.
  f.rect(56, 40, 20, 20, HEXTILE);
  for (int i = 0; i < 4; i++) {
    f.u8(0x02).px(0x5000 + i);
  }
  f.rect(70, 50, 16, 16, HEXTILE).u8(0x01);
  for (auto p : pattern(256, 1)) {
    f.px(p);
  }
  s.deliver(f);
  EXPECT_EQ(d.out_of_bounds(), 0);
  for (int y = 5; y < 40; y++) {
    for (int x = 5; x < 56; x++) {
      ASSERT_EQ(d.pixel(x, y), expect_px(raw[(y - 5) * 90 + (x - 5)], 2)) << x << "," << y;
    }
  }
  expect_solid(d, 56, 40, 8, 8, 0x5000, 2);
  EXPECT_EQ(d.pixel(4, 4), UNSET);
  EXPECT_TRUE(s.client.is_connected());
  EXPECT_EQ(hex(s.take()), hex(fbur(true, 64, 48)));
}

TEST(VncDecodeClip, ServerSmallerThanDisplay) {
  FakeDisplay d(64, 48);
  Session s(&d);
  s.start(20, 10);
  EXPECT_EQ(s.client.vis_w(), 20);
  EXPECT_EQ(s.client.vis_h(), 10);
  s.deliver(Rfb().fbu(1).raw_rect(19, 9, 1, 1, {0x1111}));
  EXPECT_EQ(d.pixel(19, 9), expect_px(0x1111, 2));
  s.deliver(Rfb().fbu(1).raw_rect(20, 0, 1, 1, {0x1111}));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
}

TEST_F(VncDecode, UnknownMessageTypeReconnects) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  int disconnects = 0;
  s().client.add_on_disconnect_callback([&disconnects]() { disconnects++; });
  s().deliver(Rfb().u8(7));
  EXPECT_EQ(s().client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_FALSE(s().client.link().connected());
  EXPECT_EQ(s().client.hold_ms(), 5000u);
  EXPECT_EQ(disconnects, 1);
}

TEST_F(VncDecode, UnsupportedEncodingReconnects) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  // CopyRect, which the client never asked for.
  s().deliver(Rfb().fbu(1).rect(0, 0, 4, 4, 1).u16(0).u16(0));
  EXPECT_EQ(s().client.state(), VncState::VNC_STATE_IDLE);
}

struct OutsideCase {
  uint16_t x, y, w, h;
  int32_t encoding;
};

class VncRectOutside : public ::testing::TestWithParam<OutsideCase> {};

TEST_P(VncRectOutside, Reconnects) {
  FakeDisplay d(64, 48);
  Session s(&d);
  s.start(64, 48);
  const auto &c = GetParam();
  s.deliver(Rfb().fbu(1).rect(c.x, c.y, c.w, c.h, c.encoding).fill(64, 0));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(d.draw_calls(), 0);
}

INSTANTIATE_TEST_SUITE_P(Rects, VncRectOutside,
                         ::testing::Values(OutsideCase{60, 0, 5, 1, RAW}, OutsideCase{0, 47, 1, 2, RAW},
                                           OutsideCase{65535, 0, 2, 1, RAW}, OutsideCase{0, 65535, 1, 2, HEXTILE},
                                           OutsideCase{64, 0, 1, 1, HEXTILE}, OutsideCase{0, 0, 65, 1, HEXTILE}));

// --- A frame whose last step needs no bytes must still finish when its last byte arrives.

class VncZeroByteEnd : public VncDecode {
 protected:
  void deliver_bytewise(const Bytes &stream, size_t frame_end) {
    for (size_t i = 0; i < stream.size(); i++) {
      s().deliver(Bytes{stream[i]});
      Bytes tx = s().take();
      if (i + 1 == frame_end) {
        EXPECT_EQ(hex(tx), hex(fbur(true, 64, 48))) << "no request after the final byte";
      } else {
        EXPECT_EQ(tx.size(), 0u) << "unexpected bytes after byte " << i;
      }
    }
    EXPECT_FALSE(s().client.in_update());
    EXPECT_EQ(s().client.step(), VncStep::VNC_STEP_MESSAGE);
    EXPECT_EQ(s().client.rx_buffered(), 0);
  }
};

TEST_F(VncZeroByteEnd, SolidHextileTile) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  Rfb f;
  f.fbu(1).rect(0, 0, 32, 16, HEXTILE).u8(0x01);
  for (auto p : pattern(256, 3)) {
    f.px(p);
  }
  f.u8(0x02).px(0x4444);
  this->deliver_bytewise(f.bytes, f.bytes.size());
  expect_solid(d, 16, 0, 16, 16, 0x4444, 2);
  s().run_for(300);
  EXPECT_EQ(d.updates(), 1);
}

TEST_F(VncZeroByteEnd, CarriedBackgroundTile) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  Rfb f;
  f.fbu(1).rect(0, 0, 32, 16, HEXTILE).u8(0x02).px(0x5555).u8(0x00);
  this->deliver_bytewise(f.bytes, f.bytes.size());
  expect_solid(d, 0, 0, 32, 16, 0x5555, 2);
  s().run_for(300);
  EXPECT_EQ(d.updates(), 1);
}

TEST_F(VncZeroByteEnd, TileWithZeroSubrects) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  Rfb f;
  f.fbu(1).rect(0, 0, 16, 16, HEXTILE).u8(0x02 | 0x08).px(0x6666).u8(0);
  this->deliver_bytewise(f.bytes, f.bytes.size());
  expect_solid(d, 0, 0, 16, 16, 0x6666, 2);
  s().run_for(300);
  EXPECT_EQ(d.updates(), 1);
}

TEST_F(VncZeroByteEnd, ZeroSizeCursor) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  Rfb f;
  f.fbu(2).raw_rect(0, 0, 2, 1, {0x7001, 0x7002}).cursor(4, 4, 0, 0);
  this->deliver_bytewise(f.bytes, f.bytes.size());
  s().run_for(300);
  EXPECT_EQ(d.updates(), 1);
}

TEST_F(VncZeroByteEnd, ZeroSizeRawRect) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  Rfb f;
  f.fbu(2).raw_rect(0, 0, 1, 1, {0x7001}).rect(64, 48, 0, 0, RAW);
  this->deliver_bytewise(f.bytes, f.bytes.size());
  s().run_for(300);
  EXPECT_EQ(d.updates(), 1);
}

TEST_F(VncZeroByteEnd, EmptyCutTextAfterFrame) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  Rfb f;
  f.fbu(1).raw_rect(0, 0, 1, 1, {0x7001});
  const size_t frame_end = f.bytes.size();
  f.cut_text(0).colour_map(0, 0);
  this->deliver_bytewise(f.bytes, frame_end);
  s().run_for(300);
  EXPECT_EQ(d.updates(), 1);
  s().deliver(Rfb().fbu(1).raw_rect(1, 0, 1, 1, {0x7002}));
  EXPECT_EQ(d.pixel(1, 0), expect_px(0x7002, 2));
}

// --- The 20 ms budget: with a slow display, one tile per pass.

TEST_F(VncDecode, BudgetPausesBetweenTilesAndFinishesOnTheLastOne) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  d.set_draw_delay_us(25000);
  Rfb f;
  f.fbu(1).rect(0, 0, 48, 16, HEXTILE).u8(0x02).px(0x0A0A).u8(0x02).px(0x0B0B).u8(0x02).px(0x0C0C);
  s().send(f);
  s().loop();
  EXPECT_EQ(d.draw_calls(), 1);
  EXPECT_TRUE(s().client.in_update());
  EXPECT_TRUE(s().client.rx_pending());
  EXPECT_TRUE(HighFrequencyLoopRequester::is_high_frequency());
  s().loop();
  EXPECT_EQ(d.draw_calls(), 2);
  s().loop();
  EXPECT_EQ(d.draw_calls(), 3);
  EXPECT_FALSE(s().client.in_update());
  EXPECT_EQ(hex(s().take()), hex(fbur(true, 64, 48)));
  d.set_draw_delay_us(0);
  s().loop(2);
  EXPECT_FALSE(s().client.rx_pending());
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
  expect_solid(d, 32, 0, 16, 16, 0x0C0C, 2);
}

TEST_F(VncDecode, BudgetPausesInsideARawRect) {
  FakeDisplay d(64, 48);
  this->start(d, 64, 48);
  d.set_draw_delay_us(25000);
  auto px = pattern(4 * 3, 0x2000);
  s().send(Rfb().fbu(1).raw_rect(0, 0, 4, 3, px));
  s().loop();
  EXPECT_EQ(d.draw_calls(), 1);
  EXPECT_TRUE(s().client.in_update());
  s().loop(2);
  EXPECT_EQ(d.draw_calls(), 3);
  EXPECT_FALSE(s().client.in_update());
  d.set_draw_delay_us(0);
  expect_block(d, 0, 0, 4, 3, px, 2);
}

// --- Split-feed property: however the bytes are cut, the result is the same and matches a model.

struct Model {
  int fb_w{0};
  int fb_h{0};
  uint8_t bpp{2};
  std::vector<int32_t> px;
  void set(int x, int y, uint16_t v) { this->px[static_cast<size_t>(y) * this->fb_w + x] = v; }
};

struct Generated {
  int disp_w{0};
  int disp_h{0};
  display::DisplayType type{display::DISPLAY_TYPE_COLOR};
  uint16_t vis_w{0};
  uint16_t vis_h{0};
  Bytes handshake;
  Bytes handshake_tx;
  std::vector<Bytes> frames;
  Model model;
};

static Generated generate(uint32_t seed) {
  std::mt19937 rng(seed);
  auto rnd = [&rng](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
  Generated g;
  g.disp_w = rnd(4, 70);
  g.disp_h = rnd(4, 50);
  const int type = rnd(0, 2);
  g.type = type == 0 ? display::DISPLAY_TYPE_COLOR
                     : (type == 1 ? display::DISPLAY_TYPE_GRAYSCALE : display::DISPLAY_TYPE_BINARY);
  const uint8_t bpp = g.type == display::DISPLAY_TYPE_BINARY ? 1 : 2;
  const int fb_w = rnd(1, 90);
  const int fb_h = rnd(1, 64);
  g.vis_w = std::min(fb_w, g.disp_w);
  g.vis_h = std::min(fb_h, g.disp_h);
  g.model.fb_w = fb_w;
  g.model.fb_h = fb_h;
  g.model.bpp = bpp;
  g.model.px.assign(static_cast<size_t>(fb_w) * fb_h, -1);
  auto pixel = [&]() { return static_cast<uint16_t>(bpp == 2 ? rnd(0, 0xFFFF) : rnd(0, 0xFF)); };

  Rfb hs;
  switch (rnd(0, 2)) {
    case 0:
      hs.version("RFB 003.003\n").security_33(1);
      g.handshake_tx = cat({version_reply('3'), {1}});
      break;
    case 1:
      hs.version("RFB 003.007\n").security_list({1});
      g.handshake_tx = cat({version_reply('7'), {1}, {1}});
      break;
    default:
      hs.version().security_list({16, 1}).security_result(0);
      g.handshake_tx = cat({version_reply('8'), {1}, {1}});
      break;
  }
  hs.server_init(fb_w, fb_h, std::string(rnd(0, 80), 'n'));
  g.handshake = hs.bytes;
  g.handshake_tx = cat({g.handshake_tx, bpp == 2 ? set_pixel_format_565() : set_pixel_format_332(), set_encodings(),
                        fbur(false, g.vis_w, g.vis_h)});

  uint16_t bg = 0;
  uint16_t fg = 0;
  const int frames = rnd(3, 8);
  for (int frame = 0; frame < frames; frame++) {
    Rfb f;
    for (int extra = rnd(0, 2); extra > 0; extra--) {
      switch (rnd(0, 2)) {
        case 0:
          f.bell();
          break;
        case 1:
          f.cut_text(rnd(0, 700));
          break;
        default:
          f.colour_map(rnd(0, 255), rnd(0, 4));
          break;
      }
    }
    const int rects = rnd(0, 5);
    f.fbu(rects);
    for (int r = 0; r < rects; r++) {
      const int kind = rnd(0, 9);
      if (kind == 8) {
        f.cursor(rnd(0, 100), rnd(0, 100), rnd(0, 9), rnd(0, 9), bpp);
        continue;
      }
      if (kind == 9) {
        const int x = rnd(0, fb_w);
        const int y = rnd(0, fb_h);
        f.rect(x, y, rnd(0, 1) ? 0 : rnd(0, fb_w - x), 0, rnd(0, 1) ? RAW : HEXTILE);
        continue;
      }
      const int x = rnd(0, fb_w - 1);
      const int y = rnd(0, fb_h - 1);
      const int w = rnd(1, std::min(fb_w - x, 48));
      const int h = rnd(1, std::min(fb_h - y, 40));
      if (kind < 4) {
        f.rect(x, y, w, h, RAW);
        for (int yy = 0; yy < h; yy++) {
          for (int xx = 0; xx < w; xx++) {
            uint16_t p = pixel();
            f.px(p, bpp);
            g.model.set(x + xx, y + yy, p);
          }
        }
        continue;
      }
      f.rect(x, y, w, h, HEXTILE);
      bool bg_valid = false;
      bool fg_valid = false;
      for (int ty = y; ty < y + h; ty += 16) {
        for (int tx = x; tx < x + w; tx += 16) {
          const int tw = std::min(16, x + w - tx);
          const int th = std::min(16, y + h - ty);
          const int choice = rnd(0, 5);
          if (choice == 0) {
            f.u8(static_cast<uint8_t>(0x01 | (rnd(0, 15) << 1)));
            for (int yy = 0; yy < th; yy++) {
              for (int xx = 0; xx < tw; xx++) {
                uint16_t p = pixel();
                f.px(p, bpp);
                g.model.set(tx + xx, ty + yy, p);
              }
            }
            continue;
          }
          uint8_t flags = 0;
          const bool set_bg = !bg_valid || choice == 1 || rnd(0, 3) == 0;
          const bool subrects = choice >= 3;
          const bool coloured = choice == 5;
          const bool set_fg = subrects && !coloured && (!fg_valid || choice == 3);
          const bool extra_fg = coloured && rnd(0, 1);
          if (set_bg) {
            flags |= 0x02;
            bg = pixel();
            bg_valid = true;
          }
          if (set_fg || extra_fg) {
            flags |= 0x04;
            fg = pixel();
            fg_valid = true;
          }
          if (subrects) {
            flags |= 0x08;
          }
          if (coloured) {
            flags |= 0x10;
          }
          f.u8(flags);
          if (flags & 0x02) {
            f.px(bg, bpp);
          }
          if (flags & 0x04) {
            f.px(fg, bpp);
          }
          std::vector<uint16_t> tile(static_cast<size_t>(tw) * th, bg);
          if (subrects) {
            const int count = rnd(0, 6);
            f.u8(count);
            for (int i = 0; i < count; i++) {
              uint16_t colour = fg;
              if (coloured) {
                colour = pixel();
                f.px(colour, bpp);
              }
              const int sx = rnd(0, 15);
              const int sy = rnd(0, 15);
              const int sw = rnd(1, 16);
              const int sh = rnd(1, 16);
              f.u8(static_cast<uint8_t>((sx << 4) | sy)).u8(static_cast<uint8_t>(((sw - 1) << 4) | (sh - 1)));
              for (int yy = sy; yy < std::min(sy + sh, th); yy++) {
                for (int xx = sx; xx < std::min(sx + sw, tw); xx++) {
                  tile[yy * tw + xx] = colour;
                }
              }
            }
          }
          for (int yy = 0; yy < th; yy++) {
            for (int xx = 0; xx < tw; xx++) {
              g.model.set(tx + xx, ty + yy, tile[yy * tw + xx]);
            }
          }
        }
      }
    }
    g.frames.push_back(f.bytes);
  }
  return g;
}

static void expect_matches_model(const FakeDisplay &d, const Generated &g, uint32_t seed) {
  for (int y = 0; y < g.disp_h; y++) {
    for (int x = 0; x < g.disp_w; x++) {
      uint32_t expected = UNSET;
      if (x < g.vis_w && y < g.vis_h) {
        const int32_t v = g.model.px[static_cast<size_t>(y) * g.model.fb_w + x];
        if (v >= 0) {
          expected = expect_px(static_cast<uint16_t>(v), g.model.bpp);
        }
      }
      ASSERT_EQ(d.pixel(x, y), expected) << "seed " << seed << " at " << x << "," << y;
    }
  }
}

enum class Cut { WHOLE, BYTES, RANDOM };

static std::vector<Bytes> cut(const Bytes &stream, Cut how, std::mt19937 &rng) {
  std::vector<Bytes> chunks;
  size_t pos = 0;
  while (pos < stream.size()) {
    size_t len = stream.size() - pos;
    if (how == Cut::BYTES) {
      len = 1;
    } else if (how == Cut::RANDOM) {
      len = std::min<size_t>(len, std::uniform_int_distribution<size_t>(1, 700)(rng));
    }
    chunks.emplace_back(stream.begin() + pos, stream.begin() + pos + len);
    pos += len;
  }
  return chunks;
}

struct FeedResult {
  std::vector<uint32_t> pixels;
  Bytes tx;
};

static FeedResult run_parser(const Generated &g, Cut how, uint32_t seed) {
  FakeDisplay d(g.disp_w, g.disp_h, g.type);
  Session s(&d);
  s.attach();
  Bytes stream = g.handshake;
  for (const auto &frame : g.frames) {
    stream.insert(stream.end(), frame.begin(), frame.end());
  }
  std::mt19937 rng(seed * 7919u + static_cast<uint32_t>(how));
  for (const auto &chunk : cut(stream, how, rng)) {
    EXPECT_EQ(s.client.feed(chunk), VncParse::VNC_PARSE_MORE) << "seed " << seed;
  }
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_RUNNING) << "seed " << seed;
  EXPECT_EQ(s.client.step(), VncStep::VNC_STEP_MESSAGE) << "seed " << seed;
  EXPECT_EQ(s.client.rx_buffered(), 0) << "seed " << seed;
  EXPECT_FALSE(s.client.in_update()) << "seed " << seed;
  EXPECT_EQ(d.out_of_bounds(), 0);
  expect_matches_model(d, g, seed);
  return {d.pixels(), s.take()};
}

TEST(VncSplitFeed, ParserGivesTheSameResultHoweverTheBytesAreCut) {
  for (uint32_t seed = 1; seed <= 50; seed++) {
    const Generated g = generate(seed);
    const FeedResult whole = run_parser(g, Cut::WHOLE, seed);
    EXPECT_EQ(hex(whole.tx), hex(g.handshake_tx)) << "seed " << seed;
    const FeedResult bytes = run_parser(g, Cut::BYTES, seed);
    const FeedResult random = run_parser(g, Cut::RANDOM, seed);
    EXPECT_EQ(whole.pixels, bytes.pixels) << "seed " << seed;
    EXPECT_EQ(whole.pixels, random.pixels) << "seed " << seed;
    EXPECT_EQ(hex(whole.tx), hex(bytes.tx)) << "seed " << seed;
    EXPECT_EQ(hex(whole.tx), hex(random.tx)) << "seed " << seed;
    if (::testing::Test::HasFailure()) {
      break;
    }
  }
}

/// End to end over the socket: the server sends each frame only after the client asked for it.
static FeedResult run_socket(const Generated &g, Cut how, uint32_t seed) {
  FakeDisplay d(g.disp_w, g.disp_h, g.type);
  Session s(&d);
  s.attach();
  s.deliver(g.handshake);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_RUNNING) << "seed " << seed;
  EXPECT_EQ(hex(s.take()), hex(g.handshake_tx)) << "seed " << seed;
  std::mt19937 rng(seed * 104729u + static_cast<uint32_t>(how));
  Bytes tx;
  auto collect = [&]() {
    Bytes more = s.take();
    tx.insert(tx.end(), more.begin(), more.end());
  };
  auto wait_for_requests = [&](int count) {
    for (int i = 0; i < 100 && count_fburs(tx, true) < count; i++) {
      s.tick(10);
      collect();
    }
    EXPECT_EQ(count_fburs(tx, true), count) << "seed " << seed;
  };
  for (size_t k = 0; k < g.frames.size(); k++) {
    if (k > 0) {
      wait_for_requests(static_cast<int>(k));
    }
    for (const auto &chunk : cut(g.frames[k], how, rng)) {
      s.send(chunk);
      for (int i = 0; i < 1000 && (s.server_bytes_waiting() || s.unread() != 0 || s.client.rx_pending()); i++) {
        s.tick(1);
      }
      collect();
    }
  }
  wait_for_requests(static_cast<int>(g.frames.size()));
  s.run_for(2000);
  collect();
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_RUNNING) << "seed " << seed;
  EXPECT_EQ(d.out_of_bounds(), 0);
  expect_matches_model(d, g, seed);
  if (d.drawn_count() != 0) {
    EXPECT_GE(d.updates(), 1) << "seed " << seed;
  }
  Bytes expected_tx;
  for (size_t i = 0; i < g.frames.size(); i++) {
    expected_tx = cat({expected_tx, fbur(true, g.vis_w, g.vis_h)});
  }
  EXPECT_EQ(hex(tx), hex(expected_tx)) << "seed " << seed;
  return {d.pixels(), tx};
}

TEST(VncSplitFeed, SocketGivesTheSameResultHoweverTheBytesAreCut) {
  for (uint32_t seed = 101; seed <= 130; seed++) {
    const Generated g = generate(seed);
    const FeedResult whole = run_socket(g, Cut::WHOLE, seed);
    const FeedResult bytes = run_socket(g, Cut::BYTES, seed);
    const FeedResult random = run_socket(g, Cut::RANDOM, seed);
    EXPECT_EQ(whole.pixels, bytes.pixels) << "seed " << seed;
    EXPECT_EQ(whole.pixels, random.pixels) << "seed " << seed;
    if (::testing::Test::HasFailure()) {
      break;
    }
  }
}

}  // namespace esphome::vnc::testing

#endif  // USE_HOST
