#include <gtest/gtest.h>

#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_t133a01.h"

namespace esphome::epaper_spi::testing {

/// Exposes the protected transfer machinery so the yield behaviour can be driven directly.
class TestableT133A01 : public EPaperT133A01 {
 public:
  TestableT133A01(uint16_t width, uint16_t height, const uint8_t *init_sequence = nullptr,
                  size_t init_sequence_length = 0)
      : EPaperT133A01("test", width, height, init_sequence, init_sequence_length) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_cs_pins(&this->cs, &this->cs1);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  using EPaperT133A01::initialise;
  using EPaperT133A01::transfer_data;

  RecordingPin dc, cs, cs1;
};

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

/// Records the first byte of each transaction (the command) with the chip-select levels at that moment.
class RoutingDelegate : public spi::SPIDelegate {
 public:
  struct Command {
    uint8_t command;
    bool cs;
    bool cs1;
  };

  RoutingDelegate(const RecordingPin *cs, const RecordingPin *cs1) : cs_(cs), cs1_(cs1) {}

  uint8_t transfer(uint8_t data) override { return 0; }
  void begin_transaction() override { this->first_ = true; }
  void write_array(const uint8_t *ptr, size_t length) override {
    if (this->first_ && length != 0) {
      this->commands.push_back({ptr[0], !this->cs_->level, !this->cs1_->level});
      this->first_ = false;
    }
  }

  std::vector<Command> commands;

 protected:
  const RecordingPin *cs_;
  const RecordingPin *cs1_;
  bool first_{false};
};

/// The init sequence is sent to both controllers, except the analog and power setup commands,
/// which go to the CS controller only. This matches the E1004 init the driver used to hard code.
TEST(EPaperT133A01, InitSequenceRoutesCommandsByOpcode) {
  static constexpr uint8_t SEQUENCE[] = {
      0x74, 2, 0x00, 0x0C,  // CS only
      0xF0, 1, 0x49,        // both
      0x00, 1, 0xDF,        // both
      0xA5, 1, 0x44,        // CS only
      0x01, 1, 0x0F,        // CS only
      0x61, 1, 0x04,        // both
      0xB6, 1, 0x07,        // CS only
      0x06, 1, 0xE0,        // CS only
      0xB7, 1, 0x01,        // CS only
      0x05, 1, 0xE0,        // CS only
      0xB0, 1, 0x01,        // CS only
      0xB1, 1, 0x02,        // CS only
  };
  TestableT133A01 display(8, 2, SEQUENCE, sizeof(SEQUENCE));
  RoutingDelegate delegate(&display.cs, &display.cs1);
  display.install(&delegate);

  ASSERT_TRUE(display.initialise(false));

  static constexpr uint8_t SENT_TO_BOTH[] = {0xF0, 0x00, 0x61};
  ASSERT_EQ(delegate.commands.size(), 12u);
  for (const auto &cmd : delegate.commands) {
    bool to_both = false;
    for (uint8_t command : SENT_TO_BOTH)
      to_both |= cmd.command == command;
    EXPECT_TRUE(cmd.cs) << "command 0x" << std::hex << +cmd.command << " not sent to CS";
    EXPECT_EQ(cmd.cs1, to_both) << "command 0x" << std::hex << +cmd.command << " routed wrongly to CS1";
  }
  EXPECT_TRUE(display.cs.level);
  EXPECT_TRUE(display.cs1.level);
}

/// A command whose argument count runs past the end of the sequence fails the component instead of
/// reading past the array.
TEST(EPaperT133A01, MalformedInitSequenceMarksFailed) {
  static constexpr uint8_t SEQUENCE[] = {0xF0, 1, 0x49, 0x00, 3, 0xDF};
  TestableT133A01 display(8, 2, SEQUENCE, sizeof(SEQUENCE));
  RoutingDelegate delegate(&display.cs, &display.cs1);
  display.install(&delegate);

  display.initialise(false);

  EXPECT_TRUE(display.is_failed());
  ASSERT_EQ(delegate.commands.size(), 1u);
  EXPECT_EQ(delegate.commands[0].command, 0xF0);
}

}  // namespace esphome::epaper_spi::testing
