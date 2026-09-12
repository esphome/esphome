#include <gtest/gtest.h>

#include <map>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_gray4.h"

namespace esphome::epaper_spi::testing {

/// Records what reaches the bus, split by the command each payload followed.
/// Commands are the bytes written while D/C is low.
class RecordingDelegate : public spi::SPIDelegate {
 public:
  explicit RecordingDelegate(const RecordingPin *dc) : dc_(dc) {}

  uint8_t transfer(uint8_t data) override {
    this->record_(&data, 1);
    return 0;
  }
  void write_array(const uint8_t *ptr, size_t length) override { this->record_(ptr, length); }

  void clear() {
    this->commands.clear();
    this->data.clear();
  }

  std::vector<uint8_t> commands;
  std::map<uint8_t, std::vector<uint8_t>> data;

 protected:
  void record_(const uint8_t *ptr, size_t length) {
    if (!this->dc_->level) {  // D/C low: command byte(s)
      for (size_t i = 0; i != length; i++) {
        this->commands.push_back(ptr[i]);
        this->last_command_ = ptr[i];
      }
      return;
    }
    auto &payload = this->data[this->last_command_];
    payload.insert(payload.end(), ptr, ptr + length);
  }

  const RecordingPin *dc_;
  uint8_t last_command_{0};
};

class TestableGray4 : public EPaperStickyGray4 {
 public:
  TestableGray4(uint16_t width, uint16_t height) : EPaperStickyGray4("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->set_full_update_every(5);  // partials enabled
    this->init_shadow_();
  }

  bool has_shadow() const { return this->shadow_.is_valid(); }

  /// Both planes of one push. transfer_data() reports false between them.
  void run_push() {
    while (!this->transfer_data()) {
    }
  }

  /// What the base class would decide; 0 means the next push is a full one.
  void set_update_count(uint8_t count) { this->update_count_ = count; }

  using EPaperStickyGray4::deep_sleep;
  using EPaperStickyGray4::init_shadow_;

  void drop_shadow() { this->shadow_.free(); }
  using EPaperStickyGray4::draw_pixel_at;

  RecordingPin dc;
};

namespace {

/// A colour landing squarely in each of the four luminance quarters.
Color color_for_level(uint8_t level) {
  static const uint8_t GRAYS[4] = {0, 96, 160, 255};
  const uint8_t v = GRAYS[level];
  return Color(v, v, v);
}

void draw_levels(TestableGray4 &display, const uint8_t (&levels)[8]) {
  for (int x = 0; x != 8; x++)
    display.draw_pixel_at(x, 0, color_for_level(levels[x]));
}

}  // namespace

/// A four-level push splits each pixel across both RAM planes: the high bit of
/// the level into the image plane, the low bit into the comparison plane, both
/// inverted because the factory waveform reads 1 as white.
TEST(EPaperGray4, FullPushSplitsLevelsAcrossBothPlanes) {
  TestableGray4 display(8, 1);
  RecordingDelegate delegate(&display.dc);
  display.install(&delegate);

  const uint8_t levels[8] = {0, 1, 2, 3, 0, 1, 2, 3};
  draw_levels(display, levels);
  display.set_update_count(0);  // full
  display.run_push();

  //          levels  0 1 2 3 0 1 2 3
  // high bit         0 0 1 1 0 0 1 1 = 0x33, inverted 0xCC
  // low  bit         0 1 0 1 0 1 0 1 = 0x55, inverted 0xAA
  ASSERT_EQ(delegate.data[0x24].size(), 1u) << "image plane not written once";
  ASSERT_EQ(delegate.data[0x26].size(), 1u) << "comparison plane not written once";
  EXPECT_EQ(delegate.data[0x24][0], 0xCC);
  EXPECT_EQ(delegate.data[0x26][0], 0xAA);
}

/// Regression test.
///
/// A partial is driven by a waveform indexed on (old bit, new bit), whose
/// 0->0 and 1->1 entries do not drive. That is what leaves unchanged pixels -
/// including gray ones - alone. It only works if the comparison plane really
/// holds the frame ON THE GLASS.
///
/// Writing the new frame first and advancing the shadow in the same pass makes
/// the second pass read a shadow that has already become the new frame, so both
/// planes go out identical. Every pixel then reads as unchanged: newly black
/// ones are driven black by the 1->1 entry and appear, while ones that should
/// have cleared are never driven and stay on the glass. The symptom is the old
/// value showing through underneath the new one.
TEST(EPaperGray4, PartialPushComparesAgainstTheFrameOnTheGlass) {
  TestableGray4 display(8, 1);
  RecordingDelegate delegate(&display.dc);
  display.install(&delegate);

  // A full push puts a known frame on the glass and records it.
  const uint8_t on_glass[8] = {3, 3, 3, 3, 0, 0, 0, 0};  // mono 0xF0
  draw_levels(display, on_glass);
  display.set_update_count(0);
  display.run_push();

  // The very next partial compares against it - no warm-up push needed.
  delegate.clear();
  const uint8_t wanted[8] = {0, 0, 3, 3, 3, 3, 0, 0};  // mono 0x3C
  draw_levels(display, wanted);
  display.set_update_count(1);
  display.run_push();

  ASSERT_EQ(delegate.data[0x26].size(), 1u) << "comparison plane not written";
  ASSERT_EQ(delegate.data[0x24].size(), 1u) << "image plane not written";
  EXPECT_EQ(delegate.data[0x26][0], 0xF0) << "comparison plane must hold the frame on the glass";
  EXPECT_EQ(delegate.data[0x24][0], 0x3C) << "image plane must hold the new frame";
  EXPECT_NE(delegate.data[0x26][0], delegate.data[0x24][0]) << "identical planes leave cleared pixels undriven";
}

/// The shadow has to track each push, or the partial after next compares
/// against a stale frame.
TEST(EPaperGray4, ShadowFollowsThePushThatWasSent) {
  TestableGray4 display(8, 1);
  RecordingDelegate delegate(&display.dc);
  display.install(&delegate);

  const uint8_t first[8] = {3, 3, 3, 3, 0, 0, 0, 0};  // 0xF0
  draw_levels(display, first);
  display.set_update_count(0);  // full push, seeds the shadow with 0xF0
  display.run_push();

  const uint8_t second[8] = {0, 0, 3, 3, 3, 3, 0, 0};  // 0x3C
  draw_levels(display, second);
  display.set_update_count(1);
  display.run_push();

  // A third push must compare against the SECOND frame, not the first.
  delegate.clear();
  const uint8_t third[8] = {3, 0, 3, 0, 3, 0, 3, 0};  // 0xAA
  draw_levels(display, third);
  display.set_update_count(1);
  display.run_push();

  EXPECT_EQ(delegate.data[0x26][0], 0x3C) << "shadow did not follow the previous push";
  EXPECT_EQ(delegate.data[0x24][0], 0xAA);
}

/// The comparison frame is only wanted where partial refresh is, so a display
/// set to refresh fully every time never allocates it.
TEST(EPaperGray4, NoComparisonFrameWhenEveryUpdateIsFull) {
  TestableGray4 display(8, 1);
  RecordingDelegate delegate(&display.dc);
  display.install(&delegate);

  display.drop_shadow();
  display.set_full_update_every(1);  // no partials, so nothing to compare
  display.init_shadow_();

  EXPECT_FALSE(display.has_shadow());
}

/// The panel sleeps between updates like a monochrome one does, but in the
/// mode that keeps RAM - the deeper mode is reserved for the host going to
/// sleep, where the rail goes with it.
TEST(EPaperGray4, SleepsBetweenUpdatesInTheRamRetainingMode) {
  TestableGray4 display(8, 1);
  RecordingDelegate delegate(&display.dc);
  display.install(&delegate);

  display.deep_sleep();

  ASSERT_EQ(delegate.data[0x10].size(), 1u) << "the panel was not put to sleep between updates";
  EXPECT_EQ(delegate.data[0x10][0], 0x01);
}

TEST(EPaperGray4, SleepsDeeplyWhenTheHostIsAboutToSleep) {
  TestableGray4 display(8, 1);
  RecordingDelegate delegate(&display.dc);
  display.install(&delegate);

  display.sleep_panel_deeply_after_next_push();
  display.deep_sleep();
  EXPECT_EQ(delegate.data[0x10][0], 0x03);

  // One push only: the next one is back to keeping RAM.
  delegate.clear();
  display.deep_sleep();
  EXPECT_EQ(delegate.data[0x10][0], 0x01);
}

}  // namespace esphome::epaper_spi::testing
