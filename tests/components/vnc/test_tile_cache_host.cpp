#include "vnc_test_helpers.h"

#ifdef USE_HOST

namespace esphome::vnc::testing {

struct Tile {
  uint16_t x;
  uint16_t y;
  uint16_t colour;
};

/// One update with a 16x16 hextile rect per tile, each a solid background.
static Bytes tiles(std::initializer_list<Tile> list) {
  Rfb f;
  f.fbu(static_cast<uint16_t>(list.size()));
  for (const auto &t : list) {
    f.rect(t.x, t.y, 16, 16, HEXTILE).u8(0x02).px(t.colour);
  }
  return f.bytes;
}

/// One update covering the whole w x h screen with solid tiles.
static Bytes screen(uint16_t w, uint16_t h, uint16_t seed) {
  Rfb f;
  f.fbu(1).rect(0, 0, w, h, HEXTILE);
  const int count = ((w + 15) / 16) * ((h + 15) / 16);
  for (int i = 0; i < count; i++) {
    f.u8(0x02).px(static_cast<uint16_t>(seed + i * 0x0841));
  }
  return f.bytes;
}

static Bytes off_grid(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour) {
  return Rfb().fbu(1).rect(x, y, w, h, HEXTILE).u8(0x02).px(colour).bytes;
}

static int count_draws(Session &s, const FakeDisplay &d, const Bytes &bytes) {
  const int before = d.draw_calls();
  s.deliver(bytes);
  return d.draw_calls() - before;
}

class VncTileCache : public VncSessionTest {
 protected:
  void SetUp() override { this->start(this->d_, 64, 48); }
  int draws(const Bytes &bytes) { return count_draws(s(), this->d_, bytes); }
  void reconnect() {
    s().close_peer();
    s().loop();
    ASSERT_FALSE(s().client.is_connected());
    s().start(64, 48);
    set_now(now_ms() + 100);
  }
  FakeDisplay d_;
};

TEST_F(VncTileCache, UnchangedTileIsNotDrawnAgain) {
  const Bytes f = tiles({{16, 16, 0x1234}});
  EXPECT_EQ(this->draws(f), 1);
  s().run_for(300);
  ASSERT_EQ(d_.updates(), 1);
  s().take();
  EXPECT_EQ(this->draws(f), 0);
  EXPECT_FALSE(s().client.refresh_pending());
  EXPECT_EQ(hex(s().take()), hex(fbur(true, 64, 48)));
  s().run_for(2000);
  EXPECT_EQ(d_.updates(), 1);
  EXPECT_TRUE(solid(d_, 16, 16, 16, 16, 0x1234));
}

TEST_F(VncTileCache, OnlyChangedTilesAreDrawn) {
  EXPECT_EQ(this->draws(tiles({{0, 0, 0x1111}, {16, 0, 0x2222}, {32, 0, 0x3333}})), 3);
  s().run_for(300);
  EXPECT_EQ(this->draws(tiles({{0, 0, 0x1111}, {16, 0, 0x4444}, {32, 0, 0x3333}})), 1);
  EXPECT_TRUE(solid(d_, 16, 0, 16, 16, 0x4444));
  s().run_for(300);
  EXPECT_EQ(d_.updates(), 2);
}

TEST_F(VncTileCache, SkippedFrameDoesNotPostponeThePendingRefresh) {
  const Bytes f = tiles({{0, 0, 0x1111}});
  set_now(1200);
  ASSERT_EQ(this->draws(f), 1);
  set_now(1350);
  EXPECT_EQ(this->draws(f), 0);
  s().at(1399);
  EXPECT_EQ(d_.updates(), 0);
  s().at(1400);
  EXPECT_EQ(d_.updates(), 1);
}

TEST_F(VncTileCache, SameBytesInAnotherShapeAreDrawn) {
  const Bytes wide = Rfb().fbu(1).rect(0, 0, 16, 8, HEXTILE).u8(0x02).px(0x1234).bytes;
  const Bytes tall = Rfb().fbu(1).rect(0, 0, 8, 16, HEXTILE).u8(0x02).px(0x1234).bytes;
  EXPECT_EQ(this->draws(wide), 1);
  EXPECT_EQ(this->draws(tall), 1);
  EXPECT_TRUE(solid(d_, 0, 8, 8, 8, 0x1234));
  EXPECT_EQ(this->draws(tall), 0);
}

TEST_F(VncTileCache, RawRectForgetsOnlyTheCellsItCovers) {
  const Bytes f = tiles({{0, 16, 0x1111}, {16, 16, 0x2222}, {32, 32, 0x3333}});
  ASSERT_EQ(this->draws(f), 3);
  // Ends on the last column of the cell at 0,16.
  s().deliver(Rfb().fbu(1).raw_rect(0, 20, 16, 1, std::vector<uint16_t>(16, 0xF800)));
  ASSERT_EQ(d_.pixel(15, 20), expect_px(0xF800, 2));
  EXPECT_EQ(this->draws(f), 1);
  EXPECT_TRUE(solid(d_, 0, 16, 16, 16, 0x1111));

  s().deliver(Rfb().fbu(1).raw_rect(20, 20, 1, 1, {0xF800}));
  ASSERT_EQ(d_.pixel(20, 20), expect_px(0xF800, 2));
  EXPECT_EQ(this->draws(f), 1);
  EXPECT_TRUE(solid(d_, 16, 16, 16, 16, 0x2222));
}

TEST_F(VncTileCache, OffGridTileForgetsTheCellsItCovers) {
  const Bytes f = tiles({{0, 0, 0x1111}, {16, 0, 0x2222}, {0, 16, 0x3333}, {16, 16, 0x4444}, {32, 0, 0x5555}});
  ASSERT_EQ(this->draws(f), 5);
  const Bytes across = off_grid(8, 8, 9, 9, 0xF800);
  // Never skipped itself.
  EXPECT_EQ(this->draws(across), 1);
  EXPECT_EQ(this->draws(across), 1);
  EXPECT_EQ(this->draws(f), 4);
  EXPECT_TRUE(solid(d_, 0, 0, 16, 16, 0x1111));
  EXPECT_TRUE(solid(d_, 16, 16, 16, 16, 0x4444));

  // Ends on the last pixel of the first cell.
  EXPECT_EQ(this->draws(off_grid(8, 8, 8, 8, 0xF800)), 1);
  EXPECT_EQ(this->draws(f), 1);
  EXPECT_TRUE(solid(d_, 0, 0, 16, 16, 0x1111));
}

TEST_F(VncTileCache, RefreshForgetsEveryTile) {
  const Bytes f = tiles({{16, 16, 0x1234}});
  ASSERT_EQ(this->draws(f), 1);
  s().run_for(300);
  ASSERT_EQ(d_.updates(), 1);
  s().take();
  s().client.refresh();
  s().loop();
  EXPECT_EQ(hex(s().take()), hex(fbur(false, 64, 48)));
  EXPECT_EQ(this->draws(f), 1);
  s().run_for(300);
  EXPECT_EQ(d_.updates(), 2);
}

TEST_F(VncTileCache, RefreshedTilesStayCachedInTheNextSession) {
  const Bytes full = screen(64, 48, 0x1000);
  ASSERT_EQ(this->draws(full), 12);
  s().run_for(300);
  ASSERT_EQ(d_.updates(), 1);
  this->reconnect();
  EXPECT_EQ(this->draws(full), 0);
  s().run_for(300);
  EXPECT_EQ(d_.updates(), 1);
}

TEST_F(VncTileCache, DropBeforeTheRefreshRedrawsInTheNextSession) {
  const Bytes full = screen(64, 48, 0x1000);
  ASSERT_EQ(this->draws(full), 12);
  s().tick(100);
  ASSERT_TRUE(s().client.refresh_pending());
  this->reconnect();
  EXPECT_EQ(d_.updates(), 0);
  EXPECT_EQ(this->draws(full), 12);
  s().run_for(300);
  EXPECT_EQ(d_.updates(), 1);
}

TEST_F(VncTileCache, DropMidFrameRedrawsInTheNextSession) {
  ASSERT_EQ(this->draws(screen(64, 48, 0x1000)), 12);
  s().run_for(300);
  ASSERT_EQ(d_.updates(), 1);
  const Bytes next = screen(64, 48, 0x2000);
  // The update and rect headers, then six of the twelve tiles.
  ASSERT_EQ(this->draws(Bytes(next.begin(), next.begin() + 4 + 12 + 6 * 3)), 6);
  ASSERT_TRUE(s().client.in_update());
  ASSERT_FALSE(s().client.refresh_pending());
  this->reconnect();
  EXPECT_EQ(this->draws(next), 12);
  s().run_for(300);
  EXPECT_EQ(d_.updates(), 2);
}

enum class SessionEnd { PEER_CLOSED, PROTOCOL_ERROR };

class VncTileCacheSessionEnd : public VncTileCache, public ::testing::WithParamInterface<SessionEnd> {};

TEST_P(VncTileCacheSessionEnd, PixelsNotYetShownAreDrawnAgain) {
  const Bytes full = screen(64, 48, 0x1000);
  ASSERT_EQ(this->draws(full), 12);
  switch (GetParam()) {
    case SessionEnd::PEER_CLOSED:
      s().close_peer();
      s().loop();
      break;
    case SessionEnd::PROTOCOL_ERROR:
      s().deliver(Rfb().u8(7));
      break;
  }
  ASSERT_FALSE(s().client.is_connected());
  s().start(64, 48);
  set_now(now_ms() + 100);
  EXPECT_EQ(this->draws(full), 12);
  s().run_for(300);
  EXPECT_EQ(d_.updates(), 1);
}

INSTANTIATE_TEST_SUITE_P(Ends, VncTileCacheSessionEnd,
                         ::testing::Values(SessionEnd::PEER_CLOSED, SessionEnd::PROTOCOL_ERROR));

// Bit 7 of every fourth byte only reached bit 31 of the old word-wise hash, so an even number of flips cancelled.
TEST(VncTileCacheHash, Rgb332TilesDifferingInEveryFourthByteAreBothDrawn) {
  FakeDisplay d(64, 48, display::DISPLAY_TYPE_BINARY);
  Session s(&d);
  s.start(64, 48);
  ASSERT_EQ(s.client.bpp(), 1);
  set_now(now_ms() + 100);
  const Bytes a(256, 0x00);
  Bytes b = a;
  const int flips[][2] = {{3, 0}, {7, 2}, {11, 4}, {15, 6}};
  for (const auto &f : flips) {
    b[f[1] * 16 + f[0]] ^= 0x80;
  }
  auto raw_tile = [](const Bytes &px) { return Rfb().fbu(1).rect(16, 16, 16, 16, HEXTILE).u8(0x01).raw(px).bytes; };
  EXPECT_EQ(count_draws(s, d, raw_tile(a)), 1);
  EXPECT_EQ(count_draws(s, d, raw_tile(b)), 1);
  for (const auto &f : flips) {
    EXPECT_EQ(d.pixel(16 + f[0], 16 + f[1]), expect_px(0x80, 1)) << f[0] << "," << f[1];
  }
  EXPECT_EQ(count_draws(s, d, raw_tile(a)), 1);
  for (const auto &f : flips) {
    EXPECT_EQ(d.pixel(16 + f[0], 16 + f[1]), expect_px(0x00, 1)) << f[0] << "," << f[1];
  }
}

// --- The display size can change between sessions; the cache was sized for 64x48 (12 cells) at setup.

class VncTileCacheLayout : public VncSessionTest {
 protected:
  void SetUp() override {
    this->session_ = std::make_unique<Session>(&this->d_);
    ASSERT_EQ(c().hash_capacity(), 12u);
  }
  TestVncClient &c() { return s().client; }
  int draws(const Bytes &bytes) { return count_draws(s(), this->d_, bytes); }
  /// Ends any session, resizes the display and starts a session with a server of the same size.
  void session(int w, int h) {
    if (c().is_connected()) {
      s().close_peer();
      s().loop();
    }
    this->d_.resize(w, h);
    s().start(w, h);
    set_now(now_ms() + 100);
  }
  FakeDisplay d_{64, 48};
};

TEST_F(VncTileCacheLayout, EveryLayoutWithinTheCellsUsesTheCache) {
  const int sizes[][2] = {{64, 48}, {48, 64}, {96, 32}, {32, 96}, {192, 16}, {16, 192}, {50, 40}};
  for (const auto &size : sizes) {
    const int w = size[0];
    const int h = size[1];
    this->session(w, h);
    EXPECT_EQ(c().hash_cols(), (w + 15) / 16) << w << "x" << h;
    EXPECT_EQ(c().hash_rows(), (h + 15) / 16) << w << "x" << h;
    const Bytes full = screen(w, h, 0x1000);
    EXPECT_EQ(this->draws(full), 12) << w << "x" << h;
    EXPECT_EQ(this->draws(full), 0) << w << "x" << h;
    s().run_for(300);
  }
  EXPECT_EQ(d_.out_of_bounds(), 0);
}

TEST_F(VncTileCacheLayout, RotationForgetsTheTiles) {
  this->session(64, 48);
  const Bytes tile = tiles({{16, 0, 0x1234}});
  ASSERT_EQ(this->draws(tile), 1);
  s().run_for(300);
  this->session(48, 64);
  // Same place, same pixels, but the panel turned underneath them.
  EXPECT_EQ(this->draws(tile), 1);
  EXPECT_EQ(this->draws(tile), 0);
}

TEST_F(VncTileCacheLayout, HalfTurnForgetsTheTiles) {
  this->session(64, 48);
  const Bytes full = screen(64, 48, 0x1000);
  ASSERT_EQ(this->draws(full), 12);
  s().run_for(300);
  // A half turn keeps the cell grid but puts every tile somewhere else on the panel.
  d_.set_rotation(display::DISPLAY_ROTATION_180_DEGREES);
  this->session(64, 48);
  EXPECT_EQ(this->draws(full), 12);
  EXPECT_EQ(this->draws(full), 0);
}

TEST_F(VncTileCacheLayout, RotationDuringASessionReconnects) {
  this->session(64, 48);
  ASSERT_EQ(this->draws(screen(64, 48, 0x1000)), 12);
  s().run_for(300);
  d_.set_rotation(display::DISPLAY_ROTATION_180_DEGREES);
  s().tick(10);
  EXPECT_EQ(c().state(), VncState::VNC_STATE_IDLE);
  EXPECT_FALSE(c().link().connected());
}

TEST_F(VncTileCacheLayout, TooManyCellsTurnsTheCacheOff) {
  this->session(64, 48);
  const Bytes tile = tiles({{0, 0, 0x1234}});
  ASSERT_EQ(this->draws(tile), 1);
  s().run_for(300);

  this->session(80, 64);
  EXPECT_EQ(c().hash_cols(), 0);
  const Bytes full = screen(80, 64, 0x2000);
  EXPECT_EQ(this->draws(full), 20);
  EXPECT_EQ(this->draws(full), 20);
  s().deliver(Rfb()
                  .fbu(2)
                  .raw_rect(70, 60, 10, 4, std::vector<uint16_t>(40, 0x1111))
                  .rect(72, 56, 8, 8, HEXTILE)
                  .u8(0x02)
                  .px(0x2222));
  EXPECT_TRUE(c().is_connected());
  s().run_for(300);

  // The cache saw nothing of the last session, so it starts empty.
  this->session(64, 48);
  EXPECT_EQ(c().hash_cols(), 4);
  EXPECT_EQ(this->draws(tile), 1);
  EXPECT_EQ(this->draws(tile), 0);
  EXPECT_EQ(d_.out_of_bounds(), 0);
}

TEST(VncTileCacheClip, ServerLargerThanTheDisplayUsesTheVisibleCells) {
  FakeDisplay d(72, 48);
  Session s(&d);
  s.start(100, 80);
  ASSERT_EQ(s.client.hash_capacity(), 15u);
  EXPECT_EQ(s.client.hash_cols(), 5);
  EXPECT_EQ(s.client.hash_rows(), 3);
  set_now(now_ms() + 100);
  // Six tiles: 48,32 is fully visible, 64,32 is cut at the right edge, the rest are off screen.
  Rfb f;
  f.fbu(1).rect(48, 32, 48, 32, HEXTILE);
  for (int i = 0; i < 6; i++) {
    f.u8(0x02).px(0x1000 + i);
  }
  EXPECT_EQ(count_draws(s, d, f.bytes), 2);
  EXPECT_EQ(count_draws(s, d, f.bytes), 0);
  EXPECT_TRUE(solid(d, 64, 32, 8, 16, 0x1001));
  EXPECT_EQ(d.out_of_bounds(), 0);
}

}  // namespace esphome::vnc::testing

#endif  // USE_HOST
