#include "../common.h"

#include <algorithm>

#include "esphome/core/hal.h"

namespace esphome::m5stack_unit_lcd::testing {

static constexpr size_t TX_MAX_BYTES = M5StackUnitLCD::TX_HEADER_LEN + CHUNK * M5StackUnitLCD::BYTES_PER_PIXEL;

// ---------------------------------------------------------------------------------------------
// Setup / identification
// ---------------------------------------------------------------------------------------------

TEST(M5StackUnitLCDSetup, ReadsIdThenConfiguresAndBlanksPanel) {
  Fixture f;
  f.display.set_brightness(0.8f);  // -> 204
  f.display.set_invert_colors(true);
  f.display.run_setup();
  ASSERT_FALSE(f.display.is_failed());
  EXPECT_EQ(f.bus.last_address, 0x3E);

  // First transaction on the bus must be the ID query (write 0x04, read 4 bytes).
  ASSERT_FALSE(f.bus.log.empty());
  EXPECT_EQ(f.bus.log[0].written, std::vector<uint8_t>{CMD_READ_ID});
  EXPECT_EQ(f.bus.log[0].read_len, 4u);

  const auto writes = f.bus.writes();
  ASSERT_EQ(writes.size(), 2u);
  // Wake, normal power, native rotation, brightness, inversion - concatenated fixed-length commands.
  const std::vector<uint8_t> expected_cfg = {CMD_SET_SLEEP,  0,   CMD_SET_POWER, 1, CMD_ROTATE, 0,
                                             CMD_BRIGHTNESS, 204, CMD_INVON};
  EXPECT_EQ(writes[0].written, expected_cfg);
  // Then a full-screen black fill so the panel matches the empty frame buffer.
  const std::vector<uint8_t> expected_blank = {CMD_FILLRECT_16, 0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1, 0, 0};
  EXPECT_EQ(writes[1].written, expected_blank);
}

TEST(M5StackUnitLCDSetup, DefaultsToFullBrightnessNotInverted) {
  Fixture f;
  f.display.run_setup();
  const auto writes = f.bus.writes();
  ASSERT_GE(writes.size(), 1u);
  const std::vector<uint8_t> expected_cfg = {CMD_SET_SLEEP,  0,   CMD_SET_POWER, 1, CMD_ROTATE, 0,
                                             CMD_BRIGHTNESS, 255, CMD_INVOFF};
  EXPECT_EQ(writes[0].written, expected_cfg);
}

TEST(M5StackUnitLCDSetup, FailsOnWrongIdBytes) {
  Fixture f;
  f.bus.id[0] = 0x00;
  f.display.run_setup();
  EXPECT_TRUE(f.display.is_failed());
  EXPECT_TRUE(f.bus.writes().empty()) << "must not configure a panel that did not identify";
}

TEST(M5StackUnitLCDSetup, FailsWhenIdReadErrors) {
  Fixture f;
  f.bus.fail_id_read = true;
  f.display.run_setup();
  EXPECT_TRUE(f.display.is_failed());
  EXPECT_TRUE(f.bus.writes().empty());
}

TEST(M5StackUnitLCDSetup, FailsWhenPanelRejectsConfiguration) {
  Fixture f;
  f.bus.fail_writes = true;
  f.display.run_setup();
  EXPECT_TRUE(f.display.is_failed());
}

TEST(M5StackUnitLCDSetup, UpdateIsIgnoredWhenSetupFailed) {
  Fixture f;
  f.bus.id[1] = 0x00;
  f.display.run_setup();
  ASSERT_TRUE(f.display.is_failed());
  f.bus.clear();
  f.display.set_writer([](display::Display &it) { it.draw_pixel_at(0, 0, Color(255, 255, 255)); });
  f.display.update();
  EXPECT_FALSE(f.display.is_transfer_pending());
  EXPECT_TRUE(f.bus.log.empty());
}

TEST(M5StackUnitLCDSetup, ReportsColorDisplayType) {
  Fixture f;
  EXPECT_EQ(f.display.get_display_type(), display::DisplayType::DISPLAY_TYPE_COLOR);
}

// ---------------------------------------------------------------------------------------------
// Geometry / rotation
// ---------------------------------------------------------------------------------------------

TEST(M5StackUnitLCDGeometry, NativeDimensionsArePortrait) {
  Fixture f;
  EXPECT_EQ(f.display.get_width(), PANEL_WIDTH);
  EXPECT_EQ(f.display.get_height(), PANEL_HEIGHT);
  EXPECT_EQ(f.display.get_native_width(), PANEL_WIDTH);
  EXPECT_EQ(f.display.get_native_height(), PANEL_HEIGHT);
}

TEST(M5StackUnitLCDGeometry, RotationSwapsLogicalDimensions) {
  Fixture f;
  f.display.set_rotation(display::DISPLAY_ROTATION_90_DEGREES);
  EXPECT_EQ(f.display.get_width(), PANEL_HEIGHT);
  EXPECT_EQ(f.display.get_height(), PANEL_WIDTH);
  f.display.set_rotation(display::DISPLAY_ROTATION_270_DEGREES);
  EXPECT_EQ(f.display.get_width(), PANEL_HEIGHT);
  EXPECT_EQ(f.display.get_height(), PANEL_WIDTH);
  f.display.set_rotation(display::DISPLAY_ROTATION_180_DEGREES);
  EXPECT_EQ(f.display.get_width(), PANEL_WIDTH);
  EXPECT_EQ(f.display.get_height(), PANEL_HEIGHT);
}

TEST(M5StackUnitLCDGeometry, RotatedPixelLandsAtNativeCoordinates) {
  Fixture f;
  f.boot();
  f.display.set_rotation(display::DISPLAY_ROTATION_90_DEGREES);
  // Logical (0, 0) in 90 degree rotation is native (width-1, 0): the panel stays portrait and
  // rotation is applied in software before the pixel reaches the frame buffer.
  f.display.set_writer([](display::Display &it) { it.draw_pixel_at(0, 0, Color(255, 255, 255)); });
  f.display.update();
  f.drain();

  const auto px = f.bus.pixel_writes();
  ASSERT_EQ(px.size(), 1u);
  const std::vector<uint8_t> expected = {
      CMD_CASET, PANEL_WIDTH - 1, PANEL_WIDTH - 1, CMD_RASET, 0, 0, CMD_WRITE_RAW_16, 0xFF, 0xFF};
  EXPECT_EQ(px[0].written, expected);
}

TEST(M5StackUnitLCDGeometry, Rotation180And270MapToNativeCorners) {
  Fixture f;
  f.boot();
  f.display.set_rotation(display::DISPLAY_ROTATION_180_DEGREES);
  f.display.set_writer([](display::Display &it) { it.draw_pixel_at(0, 0, Color(255, 255, 255)); });
  f.display.update();
  f.drain();
  {
    const auto px = f.bus.pixel_writes();
    ASSERT_EQ(px.size(), 1u);
    EXPECT_EQ(std::vector<uint8_t>(px[0].written.begin(), px[0].written.begin() + 6),
              (std::vector<uint8_t>{CMD_CASET, PANEL_WIDTH - 1, PANEL_WIDTH - 1, CMD_RASET, PANEL_HEIGHT - 1,
                                    PANEL_HEIGHT - 1}));
  }
  f.bus.clear();
  f.display.set_rotation(display::DISPLAY_ROTATION_270_DEGREES);
  f.display.set_writer([](display::Display &it) { it.draw_pixel_at(1, 0, Color(255, 255, 255)); });
  f.display.update();
  f.drain();
  {
    const auto px = f.bus.pixel_writes();
    ASSERT_EQ(px.size(), 1u);
    // 270: logical (1, 0) -> swap -> (0, 1) -> y = HEIGHT - 1 - 1 -> native (0, 238)
    EXPECT_EQ(std::vector<uint8_t>(px[0].written.begin(), px[0].written.begin() + 6),
              (std::vector<uint8_t>{CMD_CASET, 0, 0, CMD_RASET, PANEL_HEIGHT - 2, PANEL_HEIGHT - 2}));
  }
}

TEST(M5StackUnitLCDGeometry, OutOfBoundsPixelsAreDropped) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) {
    it.draw_pixel_at(-1, 0, Color(255, 255, 255));
    it.draw_pixel_at(0, -1, Color(255, 255, 255));
    it.draw_pixel_at(PANEL_WIDTH, 0, Color(255, 255, 255));
    it.draw_pixel_at(0, PANEL_HEIGHT, Color(255, 255, 255));
  });
  f.display.update();
  f.drain();
  EXPECT_TRUE(f.bus.pixel_writes().empty());
}

// ---------------------------------------------------------------------------------------------
// Pixel format
// ---------------------------------------------------------------------------------------------

TEST(M5StackUnitLCDPixels, ColorsAreSentAsBigEndianRGB565) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) {
    it.draw_pixel_at(0, 0, Color(255, 0, 0));
    it.draw_pixel_at(1, 0, Color(0, 255, 0));
    it.draw_pixel_at(2, 0, Color(0, 0, 255));
    it.draw_pixel_at(3, 0, Color(255, 255, 255));
  });
  f.display.update();
  f.drain();

  const auto px = f.bus.pixel_writes();
  ASSERT_EQ(px.size(), 1u);
  EXPECT_TRUE(px[0].has_window());
  EXPECT_EQ(px[0].pixels(), (std::vector<uint8_t>{0xF8, 0x00, 0x07, 0xE0, 0x00, 0x1F, 0xFF, 0xFF}));
}

TEST(M5StackUnitLCDPixels, FillCoversWholeFrame) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.fill(Color(0, 0, 255)); });
  f.display.update();
  f.drain();

  size_t total = 0;
  bool first = true;
  for (const auto &t : f.bus.pixel_writes()) {
    total += t.pixel_count();
    if (first) {
      first = false;
      EXPECT_TRUE(t.has_window());
      const std::vector<uint8_t> window = {CMD_CASET, 0, PANEL_WIDTH - 1, CMD_RASET, 0, PANEL_HEIGHT - 1};
      EXPECT_EQ(std::vector<uint8_t>(t.written.begin(), t.written.begin() + 6), window);
    } else {
      EXPECT_FALSE(t.has_window()) << "continuation chunks reuse the window";
    }
    for (size_t i = 0; i < t.pixel_count(); i++) {
      EXPECT_EQ(t.pixels()[2 * i], 0x00);
      EXPECT_EQ(t.pixels()[2 * i + 1], 0x1F);
    }
  }
  EXPECT_EQ(total, static_cast<size_t>(PANEL_WIDTH * PANEL_HEIGHT));
}

TEST(M5StackUnitLCDPixels, FillWithSameHighAndLowBytesUsesFastPath) {
  // White (0xFFFF) and black (0x0000) hit the memset path; verify the result is identical.
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.fill(Color(255, 255, 255)); });
  f.display.update();
  f.drain();
  size_t total = 0;
  for (const auto &t : f.bus.pixel_writes()) {
    total += t.pixel_count();
    for (uint8_t b : t.pixels())
      EXPECT_EQ(b, 0xFF);
  }
  EXPECT_EQ(total, static_cast<size_t>(PANEL_WIDTH * PANEL_HEIGHT));
}

TEST(M5StackUnitLCDPixels, FillHonoursTheClippingRectangle) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) {
    it.start_clipping(display::Rect(10, 20, 5, 2));
    it.fill(Color(255, 255, 255));
    it.end_clipping();
  });
  f.display.update();
  f.drain();

  const auto px = f.bus.pixel_writes();
  ASSERT_EQ(px.size(), 1u);
  EXPECT_EQ(std::vector<uint8_t>(px[0].written.begin(), px[0].written.begin() + 6),
            (std::vector<uint8_t>{CMD_CASET, 10, 14, CMD_RASET, 20, 21}));
  EXPECT_EQ(px[0].pixel_count(), 10u);
  EXPECT_EQ(px[0].pixels(), rgb565_bytes(255, 255, 255, 10));
}

// ---------------------------------------------------------------------------------------------
// Differential updates
// ---------------------------------------------------------------------------------------------

TEST(M5StackUnitLCDDiff, UnchangedFrameSendsNothing) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(10, 10, 20, 5, Color(255, 255, 255)); });
  f.display.update();
  f.drain();
  ASSERT_FALSE(f.bus.pixel_writes().empty());

  f.bus.clear();
  f.display.update();
  f.drain();
  EXPECT_TRUE(f.bus.pixel_writes().empty());
}

TEST(M5StackUnitLCDDiff, ClearAndRedrawOfSameContentSendsNothing) {
  // This is the normal ESPHome flow (auto_clear + lambda redraw every update). The diff against
  // the shadow buffer must recognise that nothing actually changed on screen.
  Fixture f;
  f.display.set_auto_clear(true);
  f.boot();
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 0, 30, 30, Color(255, 255, 255)); });
  f.display.update();
  f.drain();
  ASSERT_FALSE(f.bus.pixel_writes().empty());

  f.bus.clear();
  f.display.update();
  f.drain();
  EXPECT_TRUE(f.bus.pixel_writes().empty());
}

TEST(M5StackUnitLCDDiff, OnlyChangedSpanOfARowIsSent) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.horizontal_line(10, 5, 11, Color(255, 255, 255)); });
  f.display.update();
  f.drain();
  {
    const auto px = f.bus.pixel_writes();
    ASSERT_EQ(px.size(), 1u);
    const std::vector<uint8_t> window = {CMD_CASET, 10, 20, CMD_RASET, 5, 5, CMD_WRITE_RAW_16};
    EXPECT_EQ(std::vector<uint8_t>(px[0].written.begin(), px[0].written.begin() + 7), window);
    EXPECT_EQ(px[0].pixel_count(), 11u);
  }

  // Change a single pixel in the middle of that line: only that pixel goes out.
  f.bus.clear();
  f.display.set_writer([](display::Display &it) { it.draw_pixel_at(15, 5, Color(255, 0, 0)); });
  f.display.update();
  f.drain();
  {
    const auto px = f.bus.pixel_writes();
    ASSERT_EQ(px.size(), 1u);
    const std::vector<uint8_t> expected = {CMD_CASET, 15, 15, CMD_RASET, 5, 5, CMD_WRITE_RAW_16, 0xF8, 0x00};
    EXPECT_EQ(px[0].written, expected);
  }
}

TEST(M5StackUnitLCDDiff, ConsecutiveDirtyRowsCoalesceIntoOneWindow) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(10, 10, 5, 3, Color(255, 255, 255)); });
  f.display.update();
  f.drain();

  const auto px = f.bus.pixel_writes();
  ASSERT_EQ(px.size(), 1u);
  const std::vector<uint8_t> window = {CMD_CASET, 10, 14, CMD_RASET, 10, 12, CMD_WRITE_RAW_16};
  EXPECT_EQ(std::vector<uint8_t>(px[0].written.begin(), px[0].written.begin() + 7), window);
  EXPECT_EQ(px[0].pixel_count(), 15u);
  EXPECT_EQ(px[0].pixels(), rgb565_bytes(255, 255, 255, 15));
}

TEST(M5StackUnitLCDDiff, SeparatedDirtyRowsGetSeparateWindows) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) {
    it.horizontal_line(0, 0, 5, Color(255, 255, 255));
    it.horizontal_line(100, 100, 5, Color(255, 255, 255));
  });
  f.display.update();
  f.drain();

  const auto px = f.bus.pixel_writes();
  ASSERT_EQ(px.size(), 2u);
  EXPECT_EQ(std::vector<uint8_t>(px[0].written.begin(), px[0].written.begin() + 6),
            (std::vector<uint8_t>{CMD_CASET, 0, 4, CMD_RASET, 0, 0}));
  EXPECT_EQ(std::vector<uint8_t>(px[1].written.begin(), px[1].written.begin() + 6),
            (std::vector<uint8_t>{CMD_CASET, 100, 104, CMD_RASET, 100, 100}));
}

TEST(M5StackUnitLCDDiff, RegionSpanIsUnionOfRowSpans) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) {
    it.draw_pixel_at(2, 20, Color(255, 255, 255));
    it.draw_pixel_at(50, 21, Color(255, 255, 255));
  });
  f.display.update();
  f.drain();

  const auto px = f.bus.pixel_writes();
  ASSERT_EQ(px.size(), 1u);
  EXPECT_EQ(std::vector<uint8_t>(px[0].written.begin(), px[0].written.begin() + 6),
            (std::vector<uint8_t>{CMD_CASET, 2, 50, CMD_RASET, 20, 21}));
  EXPECT_EQ(px[0].pixel_count(), 49u * 2);
}

TEST(M5StackUnitLCDDiff, RedrawingAPixelWithItsCurrentColorSendsNothing) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.draw_pixel_at(5, 5, Color(0, 255, 0)); });
  f.display.update();
  f.drain();
  ASSERT_EQ(f.bus.pixel_writes().size(), 1u);

  // Same pixel, same colour, and a black pixel on an already-black row: nothing changed.
  f.bus.clear();
  f.display.set_writer([](display::Display &it) {
    it.draw_pixel_at(5, 5, Color(0, 255, 0));
    it.draw_pixel_at(50, 50, Color(0, 0, 0));
  });
  f.display.update();
  f.drain();
  EXPECT_TRUE(f.bus.pixel_writes().empty());
}

// ---------------------------------------------------------------------------------------------
// Chunking and time slicing
// ---------------------------------------------------------------------------------------------

TEST(M5StackUnitLCDTransfer, LargeRegionIsSplitIntoBoundedChunks) {
  Fixture f;
  f.boot();
  constexpr size_t total = PANEL_WIDTH * 4;  // 540 px, several chunks whatever the chunk size
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 0, PANEL_WIDTH, 4, Color(0, 255, 0)); });
  f.display.update();
  f.drain();

  const auto px = f.bus.pixel_writes();
  const size_t expected_chunks = (total + CHUNK - 1) / CHUNK;
  ASSERT_EQ(px.size(), expected_chunks);
  EXPECT_TRUE(px[0].has_window());
  size_t sent = 0;
  for (size_t i = 0; i < px.size(); i++) {
    if (i > 0) {
      EXPECT_FALSE(px[i].has_window()) << "continuation chunk " << i << " must not resend the window";
      EXPECT_EQ(px[i].written[0], CMD_WRITE_RAW_16);
    }
    EXPECT_LE(px[i].written.size(), TX_MAX_BYTES);
    EXPECT_EQ(px[i].pixel_count(), std::min(CHUNK, total - sent));
    EXPECT_EQ(px[i].pixels(), rgb565_bytes(0, 255, 0, px[i].pixel_count()));
    sent += px[i].pixel_count();
  }
  EXPECT_EQ(sent, total);
}

TEST(M5StackUnitLCDTransfer, TransactionsNeverExceedTheChunkSize) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.fill(Color(255, 0, 0)); });
  f.display.update();
  f.drain();
  for (const auto &t : f.bus.writes()) {
    EXPECT_LE(t.written.size(), TX_MAX_BYTES);
  }
}

TEST(M5StackUnitLCDTransfer, TransferRunsFromLoopNotUpdate) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 0, PANEL_WIDTH, 4, Color(0, 255, 0)); });
  f.display.update();
  EXPECT_TRUE(f.display.is_transfer_pending());
  EXPECT_TRUE(f.bus.pixel_writes().empty()) << "update() must only render; loop() pushes pixels";

  f.display.loop();
  EXPECT_FALSE(f.bus.pixel_writes().empty());
  f.drain();
  EXPECT_FALSE(f.display.is_transfer_pending());
}

TEST(M5StackUnitLCDTransfer, LoopSendsSeveralChunksWithinItsTimeBudgetOnAFastBus) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 0, PANEL_WIDTH, 4, Color(0, 255, 0)); });
  f.display.update();
  // With a bus that costs no time, everything fits in one loop() call.
  f.display.loop();
  EXPECT_FALSE(f.display.is_transfer_pending());
  EXPECT_EQ(f.bus.pixel_writes().size(), (PANEL_WIDTH * 4 + CHUNK - 1) / CHUNK);
}

TEST(M5StackUnitLCDTransfer, LoopYieldsAfterItsTimeBudgetOnASlowBus) {
  Fixture f;
  f.boot();
  f.bus.delay_per_write_ms = 12;
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 0, PANEL_WIDTH, 4, Color(0, 255, 0)); });
  f.display.update();
  f.display.loop();
  EXPECT_TRUE(f.display.is_transfer_pending());
  EXPECT_EQ(f.bus.pixel_writes().size(), 1u) << "a chunk that overruns the budget must end the slice";
  f.bus.delay_per_write_ms = 0;
  f.drain();
  EXPECT_FALSE(f.display.is_transfer_pending());
}

TEST(M5StackUnitLCDTransfer, LoopIsIdleWhenNothingPending) {
  Fixture f;
  f.boot();
  f.display.loop();
  f.display.loop();
  EXPECT_TRUE(f.bus.log.empty());
}

// ---------------------------------------------------------------------------------------------
// Command-buffer flow control
// ---------------------------------------------------------------------------------------------

TEST(M5StackUnitLCDFlowControl, WaitsWhilePanelBufferIsFull) {
  Fixture f;
  f.bus.bufcount = 10;  // panel reports almost no free slots, from boot onwards
  f.boot();
  f.display.set_writer([](display::Display &it) { it.draw_pixel_at(0, 0, Color(255, 255, 255)); });
  f.display.update();
  f.display.loop();
  f.display.loop();
  EXPECT_TRUE(f.bus.pixel_writes().empty());
  EXPECT_GE(f.bus.query_count(CMD_READ_BUFCOUNT), 2u) << "each loop() re-checks the panel";
  EXPECT_TRUE(f.display.is_transfer_pending());

  f.bus.bufcount = 255;  // panel drained
  f.display.loop();
  EXPECT_EQ(f.bus.pixel_writes().size(), 1u);
  f.drain();
  EXPECT_FALSE(f.display.is_transfer_pending());
}

TEST(M5StackUnitLCDFlowControl, DoesNotQueryPanelWhileCreditRemains) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 0, 8, 8, Color(255, 255, 255)); });
  f.bus.clear();
  f.display.update();
  f.drain();
  // 64 px + window is a handful of slots; one refill query at most.
  EXPECT_LE(f.bus.query_count(CMD_READ_BUFCOUNT), 1u);
}

TEST(M5StackUnitLCDFlowControl, CreditIsConsumedByPixelDataAndRefilled) {
  Fixture f;
  f.boot();
  // A full-frame fill is ~32k px; at ~1 slot per 4 px that is far more than the 255 slots the
  // panel can report, so the driver must query READ_BUFCOUNT repeatedly during the transfer.
  f.display.set_writer([](display::Display &it) { it.fill(Color(255, 255, 255)); });
  f.display.update();
  f.drain();
  EXPECT_GE(f.bus.query_count(CMD_READ_BUFCOUNT), 20u);
}

TEST(M5StackUnitLCDFlowControl, GivesUpWaitingAfterTimeoutAndContinues) {
  Fixture f;
  f.bus.bufcount = 0;  // panel never reports space
  f.boot();
  f.display.set_writer([](display::Display &it) { it.draw_pixel_at(0, 0, Color(255, 255, 255)); });
  f.display.update();
  f.display.loop();
  EXPECT_TRUE(f.bus.pixel_writes().empty());
  delay(600);  // NOLINT - longer than the driver's 500 ms credit timeout
  f.display.loop();
  EXPECT_EQ(f.bus.pixel_writes().size(), 1u) << "after the timeout the driver must not stall forever";
}

// ---------------------------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------------------------

TEST(M5StackUnitLCDErrors, FailedWriteIsRetriedWithFreshWindow) {
  Fixture f;
  f.boot();
  f.display.set_writer([](display::Display &it) { it.horizontal_line(0, 3, 4, Color(255, 255, 255)); });
  f.bus.fail_writes = true;
  f.display.update();
  f.display.loop();
  EXPECT_TRUE(f.display.status_has_warning());
  EXPECT_TRUE(f.display.is_transfer_pending());
  const auto attempts = f.bus.pixel_writes();
  ASSERT_EQ(attempts.size(), 1u);

  f.bus.fail_writes = false;
  f.bus.clear();
  f.drain();
  EXPECT_FALSE(f.display.status_has_warning());
  const auto retries = f.bus.pixel_writes();
  ASSERT_EQ(retries.size(), 1u);
  EXPECT_TRUE(retries[0].has_window()) << "the panel's write pointer is unknown after a failure";
  EXPECT_EQ(retries[0].written, attempts[0].written) << "the same pixels must be resent";

  // And once it succeeded, nothing is pending or resent.
  f.bus.clear();
  f.display.update();
  f.drain();
  EXPECT_TRUE(f.bus.pixel_writes().empty());
}

TEST(M5StackUnitLCDErrors, MidRegionFailureResendsOnlyUnsentPixels) {
  Fixture f;
  f.boot();
  // Two full rows in one region (270 px). Let the first chunks land, then fail one mid-row.
  constexpr size_t total = PANEL_WIDTH * 2;
  const int ok_chunks = static_cast<int>((PANEL_WIDTH + CHUNK) / CHUNK);  // enough to get into row 1
  const size_t sent_ok = ok_chunks * CHUNK;
  ASSERT_LT(sent_ok, total);
  ASSERT_GT(sent_ok, static_cast<size_t>(PANEL_WIDTH));
  f.bus.fail_after_pixel_writes = ok_chunks;
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 0, PANEL_WIDTH, 2, Color(255, 255, 255)); });
  f.display.update();
  f.drain(ok_chunks + 1);
  ASSERT_TRUE(f.display.is_transfer_pending());
  EXPECT_TRUE(f.display.status_has_warning());

  f.bus.fail_after_pixel_writes = -1;
  f.bus.clear();
  f.drain();
  EXPECT_FALSE(f.display.is_transfer_pending());

  // Row 0 and the start of row 1 were mirrored as they were sent; only the pixels that never
  // made it are dirty again, and they go out with a fresh window.
  const auto px = f.bus.pixel_writes();
  ASSERT_EQ(px.size(), 1u);
  const auto resend_x = static_cast<uint8_t>(sent_ok - PANEL_WIDTH);
  EXPECT_EQ(std::vector<uint8_t>(px[0].written.begin(), px[0].written.begin() + 6),
            (std::vector<uint8_t>{CMD_CASET, resend_x, PANEL_WIDTH - 1, CMD_RASET, 1, 1}));
  EXPECT_EQ(px[0].pixel_count(), total - sent_ok);
  EXPECT_EQ(px[0].pixels(), rgb565_bytes(255, 255, 255, total - sent_ok));
}

// ---------------------------------------------------------------------------------------------
// Update while a transfer is in flight
// ---------------------------------------------------------------------------------------------

TEST(M5StackUnitLCDRescan, UpdateDuringTransferIsPickedUpBeforeFinishing) {
  Fixture f;
  f.boot();
  f.bus.delay_per_write_ms = 12;  // slower than the loop() time budget: one chunk per loop()
  // Big region near the bottom so the first chunk leaves plenty pending.
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 200, PANEL_WIDTH, 4, Color(255, 255, 255)); });
  f.display.update();
  f.display.loop();
  ASSERT_TRUE(f.display.is_transfer_pending());
  EXPECT_EQ(f.bus.pixel_writes().size(), 1u);
  f.bus.delay_per_write_ms = 0;

  // A new frame arrives that also touches row 0, which the scan has already passed.
  f.display.set_writer([](display::Display &it) {
    it.filled_rectangle(0, 200, PANEL_WIDTH, 4, Color(255, 255, 255));
    it.draw_pixel_at(7, 0, Color(255, 0, 0));
  });
  f.display.update();
  f.drain();

  bool row0_sent = false;
  for (const auto &t : f.bus.pixel_writes()) {
    if (t.has_window() && t.written[1] == 7 && t.written[2] == 7 && t.written[4] == 0 && t.written[5] == 0) {
      row0_sent = true;
      EXPECT_EQ(t.pixels(), rgb565_bytes(255, 0, 0));
    }
  }
  EXPECT_TRUE(row0_sent);
  EXPECT_FALSE(f.display.is_transfer_pending());
}

TEST(M5StackUnitLCDRescan, RowChangedBetweenItsOwnChunksIsResentInFull) {
  // A row wider than one chunk is sent in two pieces. If the frame changes between the two
  // pieces, the first piece on the panel is stale even though the row "completed" afterwards.
  // The rescan pass must catch that; the panel has to be in sync once the transfer finishes.
  Fixture f;
  f.boot();
  f.bus.delay_per_write_ms = 12;  // one chunk per loop()
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 0, PANEL_WIDTH, 2, Color(255, 0, 0)); });
  f.display.update();
  f.display.loop();  // first CHUNK px of row 0 go out red
  ASSERT_TRUE(f.display.is_transfer_pending());
  ASSERT_EQ(f.bus.pixel_writes().size(), 1u);

  // The frame changes to green before row 0 has been completed.
  f.display.set_writer([](display::Display &it) { it.filled_rectangle(0, 0, PANEL_WIDTH, 2, Color(0, 255, 0)); });
  f.display.update();
  f.bus.delay_per_write_ms = 0;
  f.drain();
  EXPECT_FALSE(f.display.is_transfer_pending());

  // Everything the panel holds for those two rows must now be green: the stale red start of
  // row 0 has to have been resent within the same transfer, not left for a later update.
  std::vector<uint8_t> row0(PANEL_WIDTH * 2, 0), row1(PANEL_WIDTH * 2, 0);
  for (const auto &t : f.bus.pixel_writes()) {
    // Replay the writes onto a model of the panel (window per write, continuation follows on).
    static int xs = 0, xe = 0, ys = 0, ye = 0, cx = 0, cy = 0;
    if (t.has_window()) {
      xs = t.written[1];
      xe = t.written[2];
      ys = t.written[4];
      ye = t.written[5];
      cx = xs;
      cy = ys;
    }
    const auto px = t.pixels();
    for (size_t i = 0; i + 1 < px.size(); i += 2) {
      auto &row = cy == 0 ? row0 : row1;
      if (cy <= 1) {
        row[cx * 2] = px[i];
        row[cx * 2 + 1] = px[i + 1];
      }
      if (++cx > xe) {
        cx = xs;
        if (++cy > ye)
          cy = ys;
      }
    }
  }
  EXPECT_EQ(row0, rgb565_bytes(0, 255, 0, PANEL_WIDTH));
  EXPECT_EQ(row1, rgb565_bytes(0, 255, 0, PANEL_WIDTH));

  // And a further update with the same frame has nothing left to send.
  f.bus.clear();
  f.display.update();
  f.drain();
  EXPECT_TRUE(f.bus.pixel_writes().empty()) << "panel was left out of sync after the mid-transfer update";
}

// ---------------------------------------------------------------------------------------------
// Runtime controls
// ---------------------------------------------------------------------------------------------

TEST(M5StackUnitLCDControls, BrightnessInvertAndSleepSendCommands) {
  Fixture f;
  f.boot();

  f.display.set_brightness(0.25f);  // -> 64
  f.display.set_invert_colors(true);
  f.display.set_sleep(true);
  f.display.set_sleep(false);
  f.display.set_invert_colors(false);

  const auto w = f.bus.writes();
  ASSERT_EQ(w.size(), 5u);
  EXPECT_EQ(w[0].written, (std::vector<uint8_t>{CMD_BRIGHTNESS, 64}));
  EXPECT_EQ(w[1].written, std::vector<uint8_t>{CMD_INVON});
  EXPECT_EQ(w[2].written, (std::vector<uint8_t>{CMD_SET_SLEEP, 1}));
  EXPECT_EQ(w[3].written, (std::vector<uint8_t>{CMD_SET_SLEEP, 0}));
  EXPECT_EQ(w[4].written, std::vector<uint8_t>{CMD_INVOFF});
}

TEST(M5StackUnitLCDControls, SettersBeforeSetupOnlyRecordState) {
  Fixture f;
  f.display.set_brightness(0.04f);  // -> 10
  f.display.set_invert_colors(true);
  f.display.set_sleep(true);
  EXPECT_TRUE(f.bus.log.empty());
  f.display.run_setup();
  const auto w = f.bus.writes();
  ASSERT_GE(w.size(), 1u);
  EXPECT_EQ(w[0].written,
            (std::vector<uint8_t>{CMD_SET_SLEEP, 0, CMD_SET_POWER, 1, CMD_ROTATE, 0, CMD_BRIGHTNESS, 10, CMD_INVON}));
}

TEST(M5StackUnitLCDControls, BrightnessIsClampedToRange) {
  Fixture f;
  f.boot();
  f.display.set_brightness(2.0f);
  f.display.set_brightness(-1.0f);
  f.display.set_brightness(1.0f);
  const auto w = f.bus.writes();
  ASSERT_EQ(w.size(), 3u);
  EXPECT_EQ(w[0].written, (std::vector<uint8_t>{CMD_BRIGHTNESS, 255}));
  EXPECT_EQ(w[1].written, (std::vector<uint8_t>{CMD_BRIGHTNESS, 0}));
  EXPECT_EQ(w[2].written, (std::vector<uint8_t>{CMD_BRIGHTNESS, 255}));
}

TEST(M5StackUnitLCDControls, PowerdownPutsThePanelToSleep) {
  Fixture f;
  f.boot();
  f.display.on_powerdown();
  const auto w = f.bus.writes();
  ASSERT_EQ(w.size(), 1u);
  EXPECT_EQ(w[0].written, (std::vector<uint8_t>{CMD_SET_SLEEP, 1}));
}

}  // namespace esphome::m5stack_unit_lcd::testing
