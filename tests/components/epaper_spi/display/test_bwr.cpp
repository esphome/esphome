#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_bwr.h"

namespace esphome::epaper_spi::testing {
namespace {

class TestableBWR : public EPaperBWR {
 public:
  TestableBWR(uint16_t width, uint16_t height, bool invert_red, bool red_sets_black = false)
      : EPaperBWR("test", width, height, nullptr, 0, invert_red, red_sets_black) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  void draw(int x, int y, Color color) { this->draw_pixel_at(x, y, color); }
  uint8_t byte_at(size_t index) { return this->buffer_[index]; }
  void set_byte(size_t index, uint8_t value) { this->buffer_[index] = value; }
  size_t half_buffer() const { return this->buffer_length_ / 2; }

  bool send_range(size_t end, uint8_t invert_mask) { return this->send_buffer_range_(end, invert_mask, millis()); }
  size_t data_index() const { return this->current_data_index_; }

  /// Both planes of one push; returns how many calls it took.
  int run_push() {
    int calls = 1;
    while (!this->transfer_data())
      calls++;
    return calls;
  }

  /// Pretend nothing has changed since the last push.
  void clear_dirty() {
    this->x_low_ = this->width_;
    this->y_low_ = this->height_;
    this->x_high_ = 0;
    this->y_high_ = 0;
  }
  bool whole_screen_dirty() const {
    return this->x_low_ == 0 && this->y_low_ == 0 && this->x_high_ == this->width_ && this->y_high_ == this->height_;
  }

  RecordingPin dc;

 protected:
  void refresh_screen(bool partial) override {}
  void power_on() override {}
  void power_off() override {}
  void deep_sleep() override {}
};

/// Also counts chip select transactions and the size of each data write.
class CountingDelegate : public RecordingDelegate {
 public:
  using RecordingDelegate::RecordingDelegate;

  void begin_transaction() override { this->begin_count++; }
  void end_transaction() override { this->end_count++; }
  void write_array(const uint8_t *ptr, size_t length) override {
    if (this->dc_->level)
      this->data_writes.push_back(length);
    RecordingDelegate::write_array(ptr, length);
  }

  int begin_count{0};
  int end_count{0};
  std::vector<size_t> data_writes;
};

using Bytes = std::vector<uint8_t>;

const Color RED(255, 0, 0);

// draw_pixel_at()

/// The black/white plane holds 1=black and the red plane 1=red, MSB first.
TEST(EPaperBWR, DrawPixelSetsOneBitInEachPlane) {
  TestableBWR display(16, 1, false);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(Color::WHITE);

  display.draw(0, 0, Color::BLACK);
  display.draw(1, 0, RED);
  display.draw(15, 0, Color::BLACK);

  EXPECT_EQ(display.byte_at(0), 0x80);  // black/white plane
  EXPECT_EQ(display.byte_at(1), 0x01);
  EXPECT_EQ(display.byte_at(2), 0x40);  // red plane
  EXPECT_EQ(display.byte_at(3), 0x00);
}

/// Drawing over a pixel clears whatever the other color set.
TEST(EPaperBWR, DrawPixelReplacesThePreviousColor) {
  TestableBWR display(8, 1, false);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(Color::WHITE);

  display.draw(0, 0, RED);
  display.draw(0, 0, Color::BLACK);
  EXPECT_EQ(display.byte_at(0), 0x80);
  EXPECT_EQ(display.byte_at(1), 0x00);

  display.draw(0, 0, RED);
  EXPECT_EQ(display.byte_at(0), 0x00);
  EXPECT_EQ(display.byte_at(1), 0x80);

  display.draw(0, 0, Color::WHITE);
  EXPECT_EQ(display.byte_at(0), 0x00);
  EXPECT_EQ(display.byte_at(1), 0x00);
}

/// With red_sets_black, red pixels set their black/white bit too; black and white are unaffected.
TEST(EPaperBWR, DrawPixelRedSetsBlack) {
  TestableBWR display(8, 1, false, true);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(Color::WHITE);

  display.draw(0, 0, Color::BLACK);
  display.draw(1, 0, RED);
  display.draw(2, 0, Color::WHITE);

  EXPECT_EQ(display.byte_at(0), 0xC0);
  EXPECT_EQ(display.byte_at(1), 0x40);
}

TEST(EPaperBWR, DrawPixelIgnoresPointsOutsideTheScreen) {
  TestableBWR display(8, 1, false);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(Color::WHITE);

  display.draw(-1, 0, Color::BLACK);
  display.draw(8, 0, Color::BLACK);
  display.draw(0, 1, RED);

  EXPECT_EQ(display.byte_at(0), 0x00);
  EXPECT_EQ(display.byte_at(1), 0x00);
}

// fill()

TEST(EPaperBWR, FillSetsBothPlanes) {
  struct Case {
    Color color;
    bool red_sets_black;
    uint8_t bw_byte;
    uint8_t red_byte;
  };
  const Case cases[] = {
      {Color::WHITE, false, 0x00, 0x00}, {Color::BLACK, false, 0xFF, 0x00}, {RED, false, 0x00, 0xFF},
      {Color::WHITE, true, 0x00, 0x00},  {Color::BLACK, true, 0xFF, 0x00},  {RED, true, 0xFF, 0xFF},
  };
  for (const auto &c : cases) {
    TestableBWR display(16, 2, false, c.red_sets_black);
    RecordingDelegate bus(&display.dc);
    display.install(&bus);

    display.fill(c.color);

    for (size_t i = 0; i != display.half_buffer(); i++) {
      EXPECT_EQ(display.byte_at(i), c.bw_byte) << "black/white byte " << i << ", red_sets_black " << c.red_sets_black;
      EXPECT_EQ(display.byte_at(display.half_buffer() + i), c.red_byte)
          << "red byte " << i << ", red_sets_black " << c.red_sets_black;
    }
  }
}

TEST(EPaperBWR, FillMarksTheWholeScreenChanged) {
  TestableBWR display(16, 2, false);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.clear_dirty();
  display.fill(Color::BLACK);

  EXPECT_TRUE(display.whole_screen_dirty());
}

/// With a clipping area set, only the pixels inside it change.
TEST(EPaperBWR, FillWithClippingOnlyFillsTheClippedArea) {
  TestableBWR display(16, 1, false);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(Color::WHITE);

  display.start_clipping(0, 0, 8, 1);
  display.fill(Color::BLACK);
  display.end_clipping();

  EXPECT_EQ(display.byte_at(0), 0xFF);
  EXPECT_EQ(display.byte_at(1), 0x00);
}

// send_buffer_range_()

/// Bytes are XORed with the mask, sent in chunks of up to MAX_TRANSFER_SIZE, with chip select around each chunk.
TEST(EPaperBWR, SendBufferRangeSendsChunksWithChipSelectAroundEach) {
  TestableBWR display(128, 20, false);  // 320 bytes per plane
  CountingDelegate bus(&display.dc);
  display.install(&bus);
  for (size_t i = 0; i != display.half_buffer(); i++)
    display.set_byte(i, (uint8_t) (i * 7));

  EXPECT_TRUE(display.send_range(display.half_buffer(), 0xFF));

  EXPECT_EQ(display.data_index(), display.half_buffer());
  EXPECT_EQ(bus.data_writes, (std::vector<size_t>{MAX_TRANSFER_SIZE, MAX_TRANSFER_SIZE, 64}));
  EXPECT_EQ(bus.begin_count, 3);
  EXPECT_EQ(bus.end_count, 3);
  const Bytes &sent = bus.data[0];
  ASSERT_EQ(sent.size(), display.half_buffer());
  for (size_t i = 0; i != sent.size(); i++)
    EXPECT_EQ(sent[i], (uint8_t) ~(i * 7)) << "byte " << i;
}

/// After MAX_TRANSFER_TIME it stops at the end of a full chunk with chip select released, and the next call carries
/// on from there.
TEST(EPaperBWR, SendBufferRangeYieldsAndResumes) {
  TestableBWR display(128, 20, false);
  CountingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);
  display.install(&bus);
  for (size_t i = 0; i != display.half_buffer(); i++)
    display.set_byte(i, (uint8_t) i);

  EXPECT_FALSE(display.send_range(display.half_buffer(), 0x00));
  EXPECT_EQ(display.data_index(), MAX_TRANSFER_SIZE);
  EXPECT_EQ(bus.begin_count, bus.end_count);

  int calls = 1;
  while (!display.send_range(display.half_buffer(), 0x00))
    calls++;

  EXPECT_EQ(calls, 2);  // the second full chunk yields; the short last chunk does not check the time
  EXPECT_EQ(display.data_index(), display.half_buffer());
  EXPECT_EQ(bus.begin_count, bus.end_count);
  const Bytes &sent = bus.data[0];
  ASSERT_EQ(sent.size(), display.half_buffer());
  for (size_t i = 0; i != sent.size(); i++)
    EXPECT_EQ(sent[i], (uint8_t) i) << "byte " << i;
}

// transfer_data()

/// The black/white plane is always sent inverted; the red plane only when invert_red is set.
TEST(EPaperBWR, TransferDataInvertsTheRedPlaneOnlyWhenAsked) {
  for (const bool invert_red : {false, true}) {
    TestableBWR display(16, 1, invert_red);
    RecordingDelegate bus(&display.dc);
    display.install(&bus);
    display.set_byte(0, 0x0F);
    display.set_byte(1, 0x3C);
    display.set_byte(2, 0x55);
    display.set_byte(3, 0x81);

    display.run_push();

    EXPECT_EQ(bus.commands, (Bytes{0x10, 0x13})) << "invert_red " << invert_red;
    EXPECT_EQ(bus.data[0x10], (Bytes{0xF0, 0xC3})) << "invert_red " << invert_red;
    EXPECT_EQ(bus.data[0x13], invert_red ? (Bytes{0xAA, 0x7E}) : (Bytes{0x55, 0x81})) << "invert_red " << invert_red;
  }
}

/// A finished push starts the next one from the beginning.
TEST(EPaperBWR, TransferDataStartsAgainAfterAPush) {
  TestableBWR display(16, 1, false);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(Color::BLACK);

  display.run_push();
  EXPECT_EQ(display.data_index(), 0u);
  bus.clear();
  display.run_push();

  EXPECT_EQ(bus.commands, (Bytes{0x10, 0x13}));
  EXPECT_EQ(bus.data[0x10], (Bytes{0x00, 0x00}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0x00, 0x00}));
}

/// A push that yields in either plane sends each command once and every byte once.
TEST(EPaperBWR, TransferDataResumesWithoutRepeatingCommands) {
  TestableBWR display(128, 20, false);
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);
  display.install(&bus);
  display.fill(RED);

  EXPECT_GT(display.run_push(), 2);

  EXPECT_EQ(bus.commands, (Bytes{0x10, 0x13}));
  EXPECT_EQ(bus.data[0x10], Bytes(320, 0xFF));
  EXPECT_EQ(bus.data[0x13], Bytes(320, 0xFF));
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
