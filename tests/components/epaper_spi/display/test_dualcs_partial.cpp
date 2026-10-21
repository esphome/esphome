#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_t133a01.h"

namespace esphome::epaper_spi::testing {
// Other test files define fixtures with the same names; keep these to this file
namespace {

// A 32x16 panel: 16 bytes per row, 8 per chip
constexpr uint16_t PANEL_W = 32;
constexpr uint16_t PANEL_H = 16;
constexpr size_t ROW_BYTES = PANEL_W / 2;
constexpr size_t HALF_BYTES = ROW_BYTES / 2;

constexpr uint8_t REG_DTM = 0x10;
constexpr uint8_t REG_PTLW = 0x83;
constexpr uint8_t REG_CCSET = 0xE0;

/// Records every write with the chip selects that were asserted when it happened. A write of one byte is a command;
/// longer writes are its payload.
class ChipLogDelegate : public spi::SPIDelegate {
 public:
  struct Write {
    bool primary;
    bool secondary;
    bool dc;
    std::vector<uint8_t> bytes;
  };

  ChipLogDelegate(const RecordingPin *cs, const RecordingPin *cs1, const RecordingPin *dc)
      : cs_(cs), cs1_(cs1), dc_(dc) {}

  uint8_t transfer(uint8_t data) override {
    this->record_(&data, 1);
    return 0;
  }
  void write_array(const uint8_t *ptr, size_t length) override { this->record_(ptr, length); }
  void begin_transaction() override { this->begin_count++; }
  void end_transaction() override { this->end_count++; }

  /// What one chip alone received after the given command: the payload of REG_PTLW, or all the rows of REG_DTM
  std::vector<uint8_t> payload_after(uint8_t command, bool primary) const {
    std::vector<uint8_t> out;
    bool in_command = false;
    for (const auto &w : this->writes) {
      if (w.primary != primary || w.secondary == primary)
        continue;
      if (w.bytes.size() == 1) {
        in_command = w.bytes[0] == command;
      } else if (in_command) {
        out.insert(out.end(), w.bytes.begin(), w.bytes.end());
      }
    }
    return out;
  }

  bool saw_command(uint8_t command) const {
    for (const auto &w : this->writes) {
      if (w.bytes.size() == 1 && w.bytes[0] == command)
        return true;
    }
    return false;
  }

  /// How many times the command went to the given chip alone, and how many of those had D/C low
  std::pair<int, int> command_count(uint8_t command, bool primary) const {
    int sent = 0;
    int with_dc_low = 0;
    for (const auto &w : this->writes) {
      if (w.primary != primary || w.secondary == primary || w.bytes.size() != 1 || w.bytes[0] != command)
        continue;
      sent++;
      if (!w.dc)
        with_dc_low++;
    }
    return {sent, with_dc_low};
  }

  std::vector<Write> writes;
  int begin_count{0};
  int end_count{0};

 protected:
  void record_(const uint8_t *ptr, size_t length) {
    this->writes.push_back(
        {!this->cs_->level, !this->cs1_->level, this->dc_->level, std::vector<uint8_t>(ptr, ptr + length)});
  }

  const RecordingPin *cs_;
  const RecordingPin *cs1_;
  const RecordingPin *dc_;
};

/// A dual chip select panel small enough to check every byte that reaches each chip.
class TestableDualCS : public EPaperT133A01 {
 public:
  TestableDualCS() : EPaperT133A01("test", PANEL_W, PANEL_H, nullptr, 0), bus(&this->cs, &this->cs1, &this->dc) {
    this->delegate_ = &this->bus;
    this->set_dc_pin(&this->dc);
    this->set_primary_cs_pin(&this->cs);
    this->set_secondary_cs_pin(&this->cs1);
    this->set_busy_pin(&this->busy);
    EXPECT_TRUE(this->init_buffer_(this->buffer_length_));
    // A recognisable value in every byte, so the rows that are sent can be told apart
    for (size_t i = 0; i != this->buffer_length_; i++)
      this->buffer_[i] = (uint8_t) (i * 7 + 1);
    this->x_low_ = this->width_;
    this->y_low_ = this->height_;
    this->x_high_ = 0;
    this->y_high_ = 0;
  }

  /// Draws logical pixels, which sets the dirty box exactly as a user lambda would
  void draw(std::initializer_list<std::pair<int, int>> points) {
    for (const auto &p : points)
      this->draw_pixel_at(p.first, p.second, Color(0, 0, 0));
  }

  /// Runs one partial transfer to completion. The busy pin is idle throughout.
  void run_partial() {
    this->partial_ = true;
    this->run_to_completion();
  }

  /// Runs one transfer with partial_ as set to completion. A full transfer may yield on its time limit, so it can take
  /// more than one call.
  void run_to_completion() {
    int calls = 0;
    while (!this->transfer_data())
      ASSERT_LT(++calls, 50) << "transfer never completed";
  }

  /// Calls transfer_data() until it reaches the given step for the given chip (0 primary, 1 secondary)
  void run_until(TransferSub step, uint8_t chip) {
    int calls = 0;
    while (this->transfer_sub_ != step || this->transfer_chip_ != chip) {
      ASSERT_FALSE(this->transfer_data());
      ASSERT_LT(++calls, 50) << "never reached the step";
    }
  }

  /// The bytes buffer_ holds for the given rows and byte range of one row
  std::vector<uint8_t> rows(size_t first_row, size_t last_row, size_t first_byte, size_t count) const {
    std::vector<uint8_t> out;
    for (size_t r = first_row; r <= last_row; r++) {
      for (size_t j = 0; j != count; j++)
        out.push_back(this->buffer_[r * ROW_BYTES + first_byte + j]);
    }
    return out;
  }

  using EPaperT133A01::abort_transfer;
  using EPaperT133A01::draw_pixel_at;
  using EPaperT133A01::partial_;
  using EPaperT133A01::transfer_chip_;
  using EPaperT133A01::transfer_data;
  using EPaperT133A01::transfer_sub_;
  using EPaperT133A01::TransferSub;
  using EPaperT133A01::TRF_PARTIAL_DATA;
  using EPaperT133A01::TRF_PARTIAL_WAIT;
  using EPaperT133A01::TRF_START;
  using EPaperT133A01::waiting_for_idle_;

  RecordingPin dc, cs, cs1;
  SwitchableBusyPin busy;
  ChipLogDelegate bus;
};

const std::vector<uint8_t> NULL_WINDOW = {0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x01};

/// What each chip must have received for a window: the PTLW payload and the rows
void expect_chip(const TestableDualCS &d, bool primary, const std::vector<uint8_t> &ptlw,
                 const std::vector<uint8_t> &data) {
  EXPECT_EQ(d.bus.payload_after(REG_PTLW, primary), ptlw) << (primary ? "primary" : "secondary") << " PTLW";
  EXPECT_EQ(d.bus.payload_after(REG_DTM, primary), data) << (primary ? "primary" : "secondary") << " data";
}

/// A chip the window does not touch gets four rows of its first two bytes
std::vector<uint8_t> null_window_data(const TestableDualCS &d, size_t first_byte) {
  return d.rows(0, 3, first_byte, 2);
}

TEST(EPaperDualCSPartial, RegionOnlyOnPrimaryHalf) {
  TestableDualCS d;
  d.draw({{5, 3}});
  d.run_partial();
  ASSERT_EQ(d.transfer_sub_, TestableDualCS::TRF_START);

  // Columns 4..7 and rows 2..3 -> half-columns 8..15, half-row 1
  expect_chip(d, true, {0x00, 0x08, 0x00, 0x0F, 0x00, 0x01, 0x00, 0x01, 0x01}, d.rows(2, 3, 2, 2));
  expect_chip(d, false, NULL_WINDOW, null_window_data(d, HALF_BYTES));
}

TEST(EPaperDualCSPartial, RegionOnlyOnSecondaryHalf) {
  TestableDualCS d;
  d.draw({{20, 10}});
  d.run_partial();

  // Columns 20..23 are columns 4..7 of the secondary chip; rows 10..11 -> half-row 5
  expect_chip(d, true, NULL_WINDOW, null_window_data(d, 0));
  expect_chip(d, false, {0x00, 0x08, 0x00, 0x0F, 0x00, 0x05, 0x00, 0x05, 0x01}, d.rows(10, 11, HALF_BYTES + 2, 2));
}

TEST(EPaperDualCSPartial, RegionSpanningTheMiddle) {
  TestableDualCS d;
  d.draw({{13, 4}, {18, 7}});
  d.run_partial();

  // Columns 12..19: 12..15 on the primary chip, 16..19 (0..3) on the secondary; rows 4..7
  expect_chip(d, true, {0x00, 0x18, 0x00, 0x1F, 0x00, 0x02, 0x00, 0x03, 0x01}, d.rows(4, 7, 6, 2));
  expect_chip(d, false, {0x00, 0x00, 0x00, 0x07, 0x00, 0x02, 0x00, 0x03, 0x01}, d.rows(4, 7, HALF_BYTES, 2));
}

/// Rows start on an even row and end on an odd one, columns start on a multiple of 4 and end one short of the next.
TEST(EPaperDualCSPartial, WindowIsAlignedToTheControllerGranularity) {
  TestableDualCS d;
  // Rows 5..6 start on an odd row and end on an even one; column 1 is not on a multiple of 4
  d.draw({{1, 5}, {2, 6}});
  d.run_partial();

  // Columns 0..3, rows 4..7
  expect_chip(d, true, {0x00, 0x00, 0x00, 0x07, 0x00, 0x02, 0x00, 0x03, 0x01}, d.rows(4, 7, 0, 2));
  expect_chip(d, false, NULL_WINDOW, null_window_data(d, HALF_BYTES));
}

TEST(EPaperDualCSPartial, ColumnEndRoundsUpToTheNextGroupOfFour) {
  TestableDualCS d;
  d.draw({{8, 0}});  // a single column that is the first of its group
  d.run_partial();

  // Columns 8..11
  expect_chip(d, true, {0x00, 0x10, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00, 0x01}, d.rows(0, 1, 4, 2));
}

/// Rotation moves the window with the pixels: logical (2, 20) at 90 degrees is physical (11, 2).
TEST(EPaperDualCSPartial, WindowFollowsRotation) {
  TestableDualCS d;
  d.set_rotation(display::DISPLAY_ROTATION_90_DEGREES);
  d.draw({{2, 20}});
  d.run_partial();

  // Physical columns 8..11, rows 2..3
  expect_chip(d, true, {0x00, 0x10, 0x00, 0x17, 0x00, 0x01, 0x00, 0x01, 0x01}, d.rows(2, 3, 4, 2));
  expect_chip(d, false, NULL_WINDOW, null_window_data(d, HALF_BYTES));
}

/// A static mirror moves the window too: logical (1, 1) with both mirrors is physical (30, 14).
TEST(EPaperDualCSPartial, WindowFollowsMirroring) {
  TestableDualCS d;
  d.set_transform(MIRROR_X | MIRROR_Y);
  d.draw({{1, 1}});
  d.run_partial();

  // Physical columns 28..31 (12..15 on the secondary chip), rows 14..15
  expect_chip(d, true, NULL_WINDOW, null_window_data(d, 0));
  expect_chip(d, false, {0x00, 0x18, 0x00, 0x1F, 0x00, 0x07, 0x00, 0x07, 0x01}, d.rows(14, 15, HALF_BYTES + 6, 2));
}

/// More rows than one chunk, so the data step runs over several calls.
TEST(EPaperDualCSPartial, TallRegionIsSentInChunks) {
  TestableDualCS d;
  d.draw({{5, 0}, {5, 15}});
  d.run_partial();

  expect_chip(d, true, {0x00, 0x08, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x07, 0x01}, d.rows(0, 15, 2, 2));
}

/// Every data stream starts with DTM sent as a command (D/C low), on both paths and for both chips. The partial
/// path used to leave D/C high from the PTLW payload, so a panel that reads D/C took DTM for data.
TEST(EPaperDualCSPartial, DataStreamCommandIsSentWithDcLow) {
  for (bool partial : {true, false}) {
    TestableDualCS d;
    d.draw({{5, 3}});  // primary only, so the secondary gets the null window
    d.partial_ = partial;
    d.run_to_completion();

    for (bool primary : {true, false}) {
      auto [sent, with_dc_low] = d.bus.command_count(REG_DTM, primary);
      EXPECT_EQ(sent, 1) << (partial ? "partial" : "full") << (primary ? " primary" : " secondary");
      EXPECT_EQ(with_dc_low, sent) << (partial ? "partial" : "full") << (primary ? " primary" : " secondary")
                                   << ": DTM went out with D/C high";
    }
  }
}

/// A full update (partial_ is false) takes the full path even when something was drawn.
TEST(EPaperDualCSPartial, FullUpdateUsesTheFullPath) {
  TestableDualCS d;
  d.draw({{5, 3}});
  d.partial_ = false;

  d.run_to_completion();
  EXPECT_TRUE(d.bus.saw_command(REG_CCSET));
  EXPECT_FALSE(d.bus.saw_command(REG_PTLW));
  EXPECT_EQ(d.transfer_sub_, TestableDualCS::TRF_START) << "a finished transfer must let the next one choose again";
}

/// A partial update with nothing drawn has no window to send, so it falls back to the full path.
TEST(EPaperDualCSPartial, EmptyDirtyBoxUsesTheFullPath) {
  TestableDualCS d;
  d.partial_ = true;

  d.run_to_completion();
  EXPECT_TRUE(d.bus.saw_command(REG_CCSET));
  EXPECT_FALSE(d.bus.saw_command(REG_PTLW));
}

/// The path is chosen again for every transfer.
TEST(EPaperDualCSPartial, NextTransferChoosesItsPathAgain) {
  TestableDualCS d;
  d.partial_ = false;
  d.run_to_completion();

  d.bus.writes.clear();
  d.draw({{5, 3}});
  d.run_partial();
  EXPECT_TRUE(d.bus.saw_command(REG_PTLW));
  EXPECT_FALSE(d.bus.saw_command(REG_CCSET));
}

/// Abandoning a partial update with its data stream open closes the stream, and the next transfer picks its path
/// afresh: here a full one.
TEST(EPaperDualCSPartial, AbortMidPartialResetsThePath) {
  TestableDualCS d;
  d.draw({{5, 3}});
  d.partial_ = true;

  ASSERT_FALSE(d.transfer_data());  // PTLW sent, data stream open
  ASSERT_EQ(d.transfer_sub_, TestableDualCS::TRF_PARTIAL_DATA);
  ASSERT_EQ(d.transfer_chip_, 0);
  ASSERT_FALSE(d.cs.level);

  d.abort_transfer();

  EXPECT_EQ(d.transfer_sub_, TestableDualCS::TRF_START);
  EXPECT_TRUE(d.cs.level);
  EXPECT_TRUE(d.cs1.level);

  d.partial_ = false;
  d.bus.writes.clear();
  d.run_to_completion();
  EXPECT_TRUE(d.bus.saw_command(REG_CCSET));
  EXPECT_FALSE(d.bus.saw_command(REG_PTLW));
}

/// Abandoning an update that has no stream open leaves the chip selects alone but still resets the path.
TEST(EPaperDualCSPartial, AbortWithNoStreamOpenResetsThePath) {
  TestableDualCS d;
  d.transfer_sub_ = TestableDualCS::TRF_PARTIAL_WAIT;

  d.abort_transfer();

  EXPECT_EQ(d.transfer_sub_, TestableDualCS::TRF_START);
  EXPECT_EQ(d.bus.begin_count, 0);
}

/// Each half waits for the busy pin after its data; the transfer does not go on until the panel is ready.
TEST(EPaperDualCSPartial, WaitsForTheBusyPinBetweenHalves) {
  TestableDualCS d;
  d.draw({{5, 3}});
  d.partial_ = true;

  d.run_until(TestableDualCS::TRF_PARTIAL_WAIT, 0);
  d.busy.busy = true;
  EXPECT_FALSE(d.transfer_data());
  EXPECT_EQ(d.transfer_sub_, TestableDualCS::TRF_PARTIAL_WAIT) << "moved on while the panel was busy";
  EXPECT_EQ(d.transfer_chip_, 0) << "moved on while the panel was busy";
  EXPECT_TRUE(d.waiting_for_idle_);
  d.busy.busy = false;
  EXPECT_FALSE(d.transfer_data());
  EXPECT_EQ(d.transfer_chip_, 1);

  d.run_until(TestableDualCS::TRF_PARTIAL_WAIT, 1);
  d.busy.busy = true;
  EXPECT_FALSE(d.transfer_data());
  EXPECT_EQ(d.transfer_sub_, TestableDualCS::TRF_PARTIAL_WAIT) << "finished while the panel was busy";
  d.busy.busy = false;
  EXPECT_TRUE(d.transfer_data());
  EXPECT_EQ(d.transfer_sub_, TestableDualCS::TRF_START);
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
