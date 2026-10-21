#include <gtest/gtest.h>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_t133a01.h"

namespace esphome::epaper_spi::testing {

/// Exposes the protected transfer machinery so the yield behaviour can be driven directly.
class TestableT133A01 : public EPaperT133A01 {
 public:
  TestableT133A01(uint16_t width, uint16_t height) : EPaperT133A01("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_primary_cs_pin(&this->cs);
    this->set_secondary_cs_pin(&this->cs1);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  using EPaperT133A01::transfer_data;

  using EPaperT133A01::color_to_native;
  using EPaperT133A01::deep_sleep;
  using EPaperT133A01::power_off;
  using EPaperT133A01::power_on;
  using EPaperT133A01::refresh_screen;

  RecordingPin dc, cs, cs1;
};

/// The power, refresh and sleep commands come from EPaperDualCS and go to both chips.
TEST(EPaperT133A01, PowerRefreshAndSleepCommands) {
  TestableT133A01 display(8, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.power_on();
  display.refresh_screen(false);
  display.power_off();
  display.deep_sleep();

  EXPECT_EQ(bus.commands, (std::vector<uint8_t>{0x04, 0x12, 0x02, 0x07}));
  EXPECT_EQ(bus.data[0x12], std::vector<uint8_t>{0x01});
  EXPECT_EQ(bus.data[0x02], std::vector<uint8_t>{0x00});
  EXPECT_EQ(bus.data[0x07], std::vector<uint8_t>{0xA5});
  EXPECT_TRUE(display.cs.level && display.cs1.level) << "a chip was left selected";
}

/// Spectra 6 color codes: black 0, white 1, yellow 2, red 3, blue 5, green 6.
TEST(EPaperT133A01, ColorCodes) {
  TestableT133A01 display(8, 2);
  EXPECT_EQ(display.color_to_native(Color(0, 0, 0)), 0);
  EXPECT_EQ(display.color_to_native(Color(255, 255, 255)), 1);
  EXPECT_EQ(display.color_to_native(Color(255, 255, 0)), 2);
  EXPECT_EQ(display.color_to_native(Color(255, 0, 0)), 3);
  EXPECT_EQ(display.color_to_native(Color(0, 0, 255)), 5);
  EXPECT_EQ(display.color_to_native(Color(0, 255, 0)), 6);
}

/// Regression test for the T133A01 transfer deadlock (issue #17668).
///
/// `transfer_data()` evaluates its yield deadline *after* incrementing the row counter, so the
/// deadline can expire on a phase's final row. The phase is then complete but the function
/// reports "not done"; on the next call the phase guard is false, so the `disable()` /
/// CS-deassert epilogue is skipped permanently. The SPI transaction is never closed and the
/// next `enable()` blocks forever, tripping the task watchdog.
///
/// Here the CS phase is two rows and every row write overruns the deadline, so the second call
/// completes the phase exactly as the deadline expires, which is the failing alignment. The completed
/// phase must still run its epilogue: end the transaction and deassert CS.
TEST(EPaperT133A01, CompletedPhaseRunsEpilogueWhenDeadlineExpiresOnFinalRow) {
  // width 8 -> 4 bytes per row, 2 per half-row; height 2 -> a two-row CS phase
  TestableT133A01 display(8, 2);
  TimedSPIDelegate delegate(MAX_TRANSFER_TIME + 5);
  display.install(&delegate);

  // First call performs the one-off CCSET setup (which opens and closes a transaction of its
  // own) and then writes row 0 of the CS phase before yielding on the deadline.
  ASSERT_FALSE(display.transfer_data()) << "transfer should have yielded after the first row";
  ASSERT_FALSE(display.cs.level) << "CS must stay asserted across a yield mid-phase";
  const int closed_after_setup = delegate.end_count;

  // Second call writes the final row of the CS phase; the deadline expires as it lands.
  display.transfer_data();

  EXPECT_EQ(delegate.end_count, closed_after_setup + 1)
      << "completed CS phase skipped disable() -- SPI transaction left open";
  EXPECT_TRUE(display.cs.level) << "completed CS phase left CS asserted";
}

/// The CS1 phase has the same off-by-one, but fails worse: after the skipped epilogue the
/// function falls through to `return true`, reporting the transfer complete while the SPI
/// transaction is still open and CS1 is still asserted. The next command's `enable()` then
/// blocks forever. A transfer that reports done must have released the bus.
TEST(EPaperT133A01, TransferReportsDoneOnlyAfterReleasingTheBus) {
  TestableT133A01 display(8, 2);
  TimedSPIDelegate delegate(MAX_TRANSFER_TIME + 5);
  display.install(&delegate);

  // Both phases are two rows each and every row overruns the deadline, so the transfer needs
  // one call per row plus the setup call. Bound the loop so a regression fails rather than hangs.
  int calls = 0;
  while (!display.transfer_data()) {
    ASSERT_LT(++calls, 10) << "transfer never reported completion";
  }

  EXPECT_TRUE(display.cs1.level) << "transfer reported done with CS1 still asserted";
  EXPECT_EQ(delegate.begin_count, delegate.end_count)
      << "transfer reported done with an SPI transaction still open -- the next enable() would deadlock";
}

}  // namespace esphome::epaper_spi::testing
