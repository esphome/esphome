#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include "../common.h"

namespace esphome::gpio::rmt_one_wire::testing {

namespace {

/// Capture of a read slot as an RMT receiver delivers it: Low time, then the rest of the slot High.
Symbol read_slot(bool bit, uint16_t zero_low = 30, uint16_t one_low = 7) {
  uint16_t low = bit ? one_low : zero_low;
  return Symbol{low, 0, static_cast<uint16_t>(Timing{}.slot_us - low), 1};
}

/// Capture of a whole read section, the High of the last slot idles out and is stored as 0.
std::vector<Symbol> capture_of(const std::vector<bool> &bits) {
  std::vector<Symbol> capture;
  for (bool bit : bits)
    capture.push_back(read_slot(bit));
  if (!capture.empty())
    capture.back().duration1 = 0;
  return capture;
}

/// The decoder rule of section 7, as the test understands it. Levels of the second half are only
/// generated as Low for the last symbol. Point 4, "ends on Low": the last symbol has an empty Low
/// first half, whatever its duration1 and level1 say, or its level1 is 0.
bool capture_is_valid(const std::vector<Symbol> &capture, size_t expected) {
  if (capture.size() < expected)
    return false;
  if (!capture.empty()) {
    const Symbol &last = capture.back();
    if ((last.duration0 == 0 && last.level0 == 0) || last.level1 == 0)
      return false;
  }
  for (size_t i = 0; i < expected; i++) {
    if (capture[i].level0 != 0 || capture[i].duration0 == 0 || capture[i].duration0 >= Timing{}.slot_us)
      return false;
  }
  for (size_t i = expected; i < capture.size(); i++) {
    if (capture[i].duration0 != 0 || capture[i].duration1 != 0 || capture[i].level0 != 1)
      return false;
  }
  return true;
}

bool get_bit(const uint8_t *data, size_t bit) { return (data[bit / 8] >> (bit % 8)) & 1; }

}  // namespace

// ---- Limits of section 4.2 ------------------------------------------------------------------

TEST(RmtOneWireCodec, I4_SlotLimitsFollowTheMemoryBlock) {
  EXPECT_EQ(max_tx_slots(64), 63u);
  EXPECT_EQ(max_rx_slots(64), 62u);
  EXPECT_EQ(max_tx_slots(48), 47u);
  EXPECT_EQ(max_rx_slots(48), 46u);
}

// ---- I12: everything that is sent lies in the windows of section 3 --------------------------

TEST(RmtOneWireCodec, I12_ResetSymbolInWindows) {
  const Symbol reset = reset_symbol();
  EXPECT_EQ(reset.level0, 0);
  EXPECT_EQ(reset.level1, 1);
  EXPECT_TRUE(WINDOW_RESET_LOW.contains(reset.duration0)) << reset.duration0;
  EXPECT_TRUE(WINDOW_RESET_HIGH.contains(reset.duration1)) << reset.duration1;
}

TEST(RmtOneWireCodec, I12_WriteSlotsInWindows) {
  for (int value = 0; value < 256; value++) {
    const uint8_t byte = static_cast<uint8_t>(value);
    std::array<Symbol, 8> slots{};
    ASSERT_EQ(encode_write_slots(&byte, 0, 8, slots.data()), 8u);
    for (size_t bit = 0; bit < 8; bit++) {
      const Symbol &slot = slots[bit];
      EXPECT_EQ(slot.level0, 0);
      EXPECT_EQ(slot.level1, 1);
      const uint32_t total = slot.duration0 + slot.duration1;
      EXPECT_TRUE(WINDOW_SLOT.contains(total)) << total;
      if (get_bit(&byte, bit)) {
        EXPECT_TRUE(WINDOW_WRITE1_LOW.contains(slot.duration0)) << slot.duration0;
      } else {
        EXPECT_TRUE(WINDOW_WRITE0_LOW.contains(slot.duration0)) << slot.duration0;
      }
    }
  }
}

TEST(RmtOneWireCodec, I12_ReadSlotsInWindows) {
  std::array<Symbol, 8> slots{};
  ASSERT_EQ(encode_read_slots(8, slots.data()), 8u);
  for (const auto &slot : slots) {
    EXPECT_EQ(slot.level0, 0);
    EXPECT_EQ(slot.level1, 1);
    EXPECT_TRUE(WINDOW_READ_LOW.contains(slot.duration0)) << slot.duration0;
    EXPECT_TRUE(WINDOW_SLOT.contains(slot.duration0 + slot.duration1));
  }
}

TEST(RmtOneWireCodec, I12_SlotsKeepTheRiseTimeBudget) {
  // Section 3: a 1 must be High again before it is sampled, with tRC up to 9 us.
  const uint8_t ones = 0xFF;
  const uint8_t zeros = 0x00;
  Symbol one{};
  Symbol zero{};
  Symbol read{};
  encode_write_slots(&ones, 0, 1, &one);
  encode_write_slots(&zeros, 0, 1, &zero);
  encode_read_slots(1, &read);
  EXPECT_LE(one.duration0 + BUDGET_TRC_US, SAMPLE_WRITE1_US);
  EXPECT_LE(read.duration0 + BUDGET_TRC_US, SAMPLE_READ_US);
  // Recovery: after a 0 the line must rise and rest before the next slot
  EXPECT_GE(zero.duration1, BUDGET_TRC_US + MIN_RECOVERY_US);
}

TEST(RmtOneWireCodec, I12_ExactValuesOfSection43) {
  const uint8_t ones = 0xFF;
  const uint8_t zeros = 0x00;
  Symbol one{};
  Symbol zero{};
  Symbol read{};
  encode_write_slots(&ones, 0, 1, &one);
  encode_write_slots(&zeros, 0, 1, &zero);
  encode_read_slots(1, &read);
  const Symbol reset = reset_symbol();
  EXPECT_EQ(reset.duration0, 520);
  EXPECT_EQ(reset.duration1, 500);
  EXPECT_EQ(one.duration0, 6);
  EXPECT_EQ(one.duration1, 104);
  EXPECT_EQ(zero.duration0, 90);
  EXPECT_EQ(zero.duration1, 20);
  EXPECT_EQ(read.duration0, 4);
  EXPECT_EQ(read.duration1, 106);
}

// ---- Encoding -------------------------------------------------------------------------------

TEST(RmtOneWireCodec, EncodeWriteIsLsbFirstFromBitOffset) {
  const std::array<uint8_t, 3> data{0b10110010, 0b01000111, 0b11100001};
  for (size_t offset = 0; offset < 24; offset++) {
    for (size_t bits = 0; offset + bits <= 24; bits++) {
      std::array<Symbol, 26> out{};
      const Symbol guard{1234, 1, 4321, 0};
      out.fill(guard);
      ASSERT_EQ(encode_write_slots(data.data(), offset, bits, out.data()), bits);
      for (size_t i = 0; i < bits; i++) {
        const Slot expected = get_bit(data.data(), offset + i) ? Slot::WRITE_1 : Slot::WRITE_0;
        EXPECT_EQ(classify_slot(out[i]), expected) << "offset " << offset << " bit " << i;
      }
      EXPECT_EQ(out[bits].duration0, guard.duration0) << "wrote behind num_bits";
    }
  }
}

TEST(RmtOneWireCodec, EncodeReadWritesExactlyNumBits) {
  for (size_t bits = 0; bits <= 62; bits++) {
    std::array<Symbol, 64> out{};
    const Symbol guard{1234, 1, 4321, 0};
    out.fill(guard);
    ASSERT_EQ(encode_read_slots(bits, out.data()), bits);
    for (size_t i = 0; i < bits; i++)
      EXPECT_EQ(classify_slot(out[i]), Slot::READ);
    EXPECT_EQ(out[bits].duration0, guard.duration0) << "wrote behind num_bits";
  }
}

// ---- decode_reset, section 4.3 ---------------------------------------------------------------

struct ResetCase {
  const char *name;
  std::vector<Symbol> capture;
  TransactionStatus expected;
};

class RmtOneWireDecodeReset : public ::testing::TestWithParam<ResetCase> {};

TEST_P(RmtOneWireDecodeReset, Result) {
  const auto &param = GetParam();
  const Symbol dummy{};
  const Symbol *capture = param.capture.empty() ? &dummy : param.capture.data();
  EXPECT_EQ(static_cast<int>(decode_reset(capture, param.capture.size())), static_cast<int>(param.expected));
}

INSTANTIATE_TEST_SUITE_P(
    I5, RmtOneWireDecodeReset,
    ::testing::Values(
        ResetCase{"TypicalPresence", {{503, 0, 27, 1}, {120, 0, 0, 1}}, TransactionStatus::OK},
        ResetCase{
            "PresenceFollowedByEndMarker", {{503, 0, 27, 1}, {120, 0, 500, 1}, {0, 1, 0, 1}}, TransactionStatus::OK},
        ResetCase{
            "PresenceThenEndsOnLow", {{503, 0, 27, 1}, {120, 0, 500, 1}, {0, 0, 0, 1}}, TransactionStatus::BUS_LOW},
        ResetCase{"ShortestPresence", {{503, 0, 27, 1}, {15, 0, 0, 1}}, TransactionStatus::OK},
        ResetCase{"TooShortForPresence", {{503, 0, 27, 1}, {14, 0, 0, 1}}, TransactionStatus::NO_PRESENCE},
        ResetCase{"LatestPresence", {{503, 0, 300, 1}, {60, 0, 0, 1}}, TransactionStatus::OK},
        ResetCase{"TooLateForPresence", {{503, 0, 301, 1}, {60, 0, 0, 1}}, TransactionStatus::NO_PRESENCE},
        ResetCase{"GlitchThenPresence", {{503, 0, 20, 1}, {5, 0, 30, 1}, {100, 0, 0, 1}}, TransactionStatus::OK},
        // The presence begins 355 us after the release: 250 + 5 + 100
        ResetCase{
            "GlitchThenTooLate", {{503, 0, 250, 1}, {5, 0, 100, 1}, {60, 0, 0, 1}}, TransactionStatus::NO_PRESENCE},
        ResetCase{"NoDevice", {{503, 0, 0, 1}}, TransactionStatus::NO_PRESENCE},
        ResetCase{"LongestOwnPulseIsNoHang", {{599, 0, 0, 1}}, TransactionStatus::NO_PRESENCE},
        ResetCase{"FirstLowAtHangLimit", {{600, 0, 0, 1}}, TransactionStatus::NO_PRESENCE},
        ResetCase{"HeldLowUntilIdle", {{600, 0, 0, 0}}, TransactionStatus::BUS_LOW},
        ResetCase{"HeldLowLongNumbers", {{32767, 0, 0, 0}}, TransactionStatus::BUS_LOW},
        ResetCase{"EndsOnLowAfterLongPulse", {{503, 0, 27, 1}, {600, 0, 0, 0}}, TransactionStatus::BUS_LOW},
        ResetCase{"EndsOnShortLow", {{503, 0, 27, 1}, {120, 0, 0, 0}}, TransactionStatus::BUS_LOW},
        ResetCase{"EndsOnLowZeroDuration", {{503, 0, 27, 1}, {0, 0, 0, 0}}, TransactionStatus::BUS_LOW},
        // Section 7 point 4, as the S3 stores it: duration1 is not a time and is never evaluated
        ResetCase{"OnlySymbolEndsOnLowStale74", {{0, 0, 74, 1}}, TransactionStatus::BUS_LOW},
        ResetCase{"OnlySymbolEndsOnLowStale75", {{0, 0, 75, 1}}, TransactionStatus::BUS_LOW},
        ResetCase{"OnlySymbolEndsOnLowStale0", {{0, 0, 0, 1}}, TransactionStatus::BUS_LOW},
        ResetCase{"ResetThenEndsOnLowStale91", {{503, 0, 27, 1}, {0, 0, 91, 1}}, TransactionStatus::BUS_LOW},
        ResetCase{"EmptyCapture", {}, TransactionStatus::CAPTURE_ERROR},
        ResetCase{"StartsHigh", {{503, 1, 27, 0}, {120, 1, 0, 0}}, TransactionStatus::CAPTURE_ERROR},
        // Section 7: only T+ M* and T* L are allowed, a form error goes before BUS_LOW
        ResetCase{"OnlyEndMarker", {{0, 1, 0, 1}}, TransactionStatus::CAPTURE_ERROR},
        ResetCase{
            "EndsOnLowThenMarker", {{503, 0, 27, 1}, {0, 0, 74, 1}, {0, 1, 0, 1}}, TransactionStatus::CAPTURE_ERROR},
        ResetCase{"EndsOnLowThenPresence",
                  {{503, 0, 27, 1}, {0, 0, 74, 1}, {120, 0, 0, 1}},
                  TransactionStatus::CAPTURE_ERROR},
        ResetCase{
            "MarkerThenPresence", {{503, 0, 27, 1}, {0, 1, 0, 1}, {120, 0, 0, 1}}, TransactionStatus::CAPTURE_ERROR},
        ResetCase{"SecondHalfLowInTheMiddle", {{503, 0, 27, 0}, {120, 0, 0, 1}}, TransactionStatus::CAPTURE_ERROR},
        ResetCase{"LaterSymbolStartsHighEndsLow", {{503, 0, 27, 1}, {120, 1, 0, 0}}, TransactionStatus::CAPTURE_ERROR}),
    [](const ::testing::TestParamInfo<ResetCase> &info) { return info.param.name; });

// ---- decode_read_slots, section 4.3 and section 7 --------------------------------------------

TEST(RmtOneWireCodec, I5_ReadThresholdIs13Microseconds) {
  for (uint16_t low = 1; low < 80; low++) {
    const Symbol capture{low, 0, 0, 1};
    uint8_t out = low >= 13 ? 0xFF : 0x00;
    ASSERT_TRUE(decode_read_slots(&capture, 1, 1, &out, 0)) << low;
    EXPECT_EQ(out & 1, low >= 13 ? 0 : 1) << "Low time " << low;
  }
}

TEST(RmtOneWireCodec, I5_DecodeIsLsbFirstFromBitOffset) {
  const std::array<uint8_t, 3> expected{0b10110010, 0b01000111, 0b11100001};
  for (size_t offset = 0; offset < 24; offset += 3) {
    std::vector<bool> bits;
    for (size_t bit = offset; bit < 24; bit++)
      bits.push_back(get_bit(expected.data(), bit));
    const auto capture = capture_of(bits);
    std::array<uint8_t, 3> out{};
    ASSERT_TRUE(decode_read_slots(capture.data(), capture.size(), bits.size(), out.data(), offset));
    for (size_t bit = offset; bit < 24; bit++)
      EXPECT_EQ(get_bit(out.data(), bit), get_bit(expected.data(), bit)) << "bit " << bit;
  }
}

TEST(RmtOneWireCodec, I5_OnlyTheDecodedBitsChange) {
  std::mt19937 rng(1);
  for (int round = 0; round < 2000; round++) {
    const size_t offset = rng() % 40;
    const size_t count = rng() % 40;
    std::vector<bool> bits;
    for (size_t i = 0; i < count; i++)
      bits.push_back(rng() & 1);
    const auto capture = capture_of(bits);
    std::array<uint8_t, 12> before{};
    for (auto &byte : before)
      byte = static_cast<uint8_t>(rng());
    auto out = before;
    ASSERT_TRUE(decode_read_slots(capture.data(), capture.size(), count, out.data(), offset));
    for (size_t bit = 0; bit < before.size() * 8; bit++) {
      if (bit >= offset && bit < offset + count) {
        EXPECT_EQ(get_bit(out.data(), bit), bits[bit - offset]) << "bit " << bit;
      } else {
        EXPECT_EQ(get_bit(out.data(), bit), get_bit(before.data(), bit)) << "bit " << bit << " changed";
      }
    }
  }
}

TEST(RmtOneWireCodec, I5_CompleteCaptureShapesAreAccepted) {
  const std::vector<bool> bits{true, false, true};
  uint8_t out = 0;

  auto idle_end = capture_of(bits);  // last High stored as zero
  EXPECT_TRUE(decode_read_slots(idle_end.data(), idle_end.size(), 3, &out, 0));
  EXPECT_EQ(out, 0b101);

  std::vector<Symbol> full_last{read_slot(true), read_slot(false), read_slot(true)};
  out = 0;
  EXPECT_TRUE(decode_read_slots(full_last.data(), full_last.size(), 3, &out, 0));
  EXPECT_EQ(out, 0b101);

  const Symbol marker{0, 1, 0, 1};
  auto with_marker = full_last;
  with_marker.push_back(marker);
  with_marker.push_back(marker);
  out = 0;
  EXPECT_TRUE(decode_read_slots(with_marker.data(), with_marker.size(), 3, &out, 0));
  EXPECT_EQ(out, 0b101);

  // Section 7 points 4 and 5: starting Low it is no end marker, even with both durations 0
  const Symbol ends_on_low{0, 0, 0, 1};
  auto with_low = full_last;
  with_low.push_back(ends_on_low);
  EXPECT_FALSE(decode_read_slots(with_low.data(), with_low.size(), 3, &out, 0));
  with_low.push_back(ends_on_low);
  EXPECT_FALSE(decode_read_slots(with_low.data(), with_low.size(), 3, &out, 0));
  auto low_then_marker = full_last;
  low_then_marker.push_back(ends_on_low);
  low_then_marker.push_back(marker);
  EXPECT_FALSE(decode_read_slots(low_then_marker.data(), low_then_marker.size(), 3, &out, 0));
  auto marker_then_low = full_last;
  marker_then_low.push_back(marker);
  marker_then_low.push_back(ends_on_low);
  EXPECT_FALSE(decode_read_slots(marker_then_low.data(), marker_then_low.size(), 3, &out, 0));
}

TEST(RmtOneWireCodec, I5_LowTimeMustBeShorterThanASlot) {
  // Section 7 point 3, at every position: 109 us is a 0, from 110 us on someone holds the bus
  const std::vector<bool> bits{true, true, true, true};
  for (size_t position = 0; position < bits.size(); position++) {
    auto capture = capture_of(bits);
    capture[position].duration0 = 109;
    uint8_t out = 0;
    ASSERT_TRUE(decode_read_slots(capture.data(), capture.size(), bits.size(), &out, 0)) << "position " << position;
    EXPECT_EQ(out, 0x0F & ~(1u << position)) << "position " << position;

    for (uint16_t low : {uint16_t{110}, uint16_t{111}, uint16_t{600}, uint16_t{32767}}) {
      capture[position].duration0 = low;
      EXPECT_FALSE(decode_read_slots(capture.data(), capture.size(), bits.size(), &out, 0))
          << "position " << position << " low " << low;
    }
  }
}

TEST(RmtOneWireCodec, I5_LowInTheSecondHalfOfASymbolCountsToo) {
  auto capture = capture_of({true, true, true});
  uint8_t out = 0;
  capture[1].level1 = 0;
  // 76 is below a slot: point 3 only looks at duration0, point 2 rejects level1 == 0 at any position
  for (uint16_t duration : {uint16_t{80}, uint16_t{76}}) {
    capture[1].duration1 = duration;
    EXPECT_FALSE(decode_read_slots(capture.data(), capture.size(), 3, &out, 0)) << duration;
  }
}

TEST(RmtOneWireCodec, I5_CaptureEndingOnLowIsInvalid) {
  // Section 7 point 4: the line went to GND in the last slot, the count alone would pass
  const std::vector<bool> bits{true, false, true};
  uint8_t out = 0;
  for (const Symbol &last : {Symbol{30, 0, 0, 0}, Symbol{79, 0, 0, 0}, Symbol{100, 0, 0, 0}, Symbol{4, 0, 76, 0}}) {
    auto capture = capture_of(bits);
    capture.back() = last;
    EXPECT_FALSE(decode_read_slots(capture.data(), capture.size(), bits.size(), &out, 0)) << last.duration0;
    // A single slot, also as the only symbol of a section
    EXPECT_FALSE(decode_read_slots(&last, 1, 1, &out, 0)) << last.duration0;
  }
  // The same behind a complete capture: an end marker that stops on Low
  auto capture = capture_of(bits);
  capture.push_back(Symbol{0, 0, 0, 0});
  EXPECT_FALSE(decode_read_slots(capture.data(), capture.size(), bits.size(), &out, 0));

  // The form the S3 stores: Low first half with no time, duration1 is whatever was in the memory
  for (uint16_t stale : {uint16_t{74}, uint16_t{75}, uint16_t{91}, uint16_t{0}}) {
    const Symbol ends_on_low{0, 0, stale, 1};
    auto replaced = capture_of(bits);
    replaced.back() = ends_on_low;
    EXPECT_FALSE(decode_read_slots(replaced.data(), replaced.size(), bits.size(), &out, 0)) << stale;
    EXPECT_FALSE(decode_read_slots(&ends_on_low, 1, 1, &out, 0)) << stale;
    auto behind = capture_of(bits);
    behind.push_back(ends_on_low);
    EXPECT_FALSE(decode_read_slots(behind.data(), behind.size(), bits.size(), &out, 0)) << stale;
    behind = capture_of(bits);
    behind.push_back(Symbol{0, 1, 0, 1});
    behind.push_back(ends_on_low);
    EXPECT_FALSE(decode_read_slots(behind.data(), behind.size(), bits.size(), &out, 0)) << stale;
  }
}

TEST(RmtOneWireCodec, I5_ZeroBitsNeedNoSymbols) {
  uint8_t out = 0x5A;
  const Symbol unused{};
  EXPECT_TRUE(decode_read_slots(&unused, 0, 0, &out, 0));
  EXPECT_EQ(out, 0x5A);
}

TEST(RmtOneWireCodec, I5_TruncatedCaptureIsInvalid) {
  const std::vector<bool> bits(62, false);
  const auto complete = capture_of(bits);
  std::array<uint8_t, 8> out{};
  for (size_t kept = 0; kept < complete.size(); kept++)
    EXPECT_FALSE(decode_read_slots(complete.data(), kept, bits.size(), out.data(), 0)) << "kept " << kept;
}

TEST(RmtOneWireCodec, I5_EndMarkerDoesNotReplaceAMissingSlot) {
  // R1: on newer chips the last symbol may be missing. The end marker in its place is no slot,
  // otherwise a missing 0 would read as a 1 and the capture would pass.
  std::vector<Symbol> capture{read_slot(false), read_slot(false)};
  std::array<uint8_t, 2> out{};
  for (const Symbol &marker : {Symbol{0, 0, 0, 0}, Symbol{0, 0, 0, 1}, Symbol{0, 1, 0, 1}}) {
    capture.resize(2);
    capture.push_back(marker);
    EXPECT_FALSE(decode_read_slots(capture.data(), capture.size(), 3, out.data(), 0));
  }
}

TEST(RmtOneWireCodec, I5_WrongLevelAnywhereIsInvalid) {
  const std::vector<bool> bits{true, false, true, true, false, false, true, false};
  const auto good = capture_of(bits);
  std::array<uint8_t, 2> out{};
  for (size_t i = 0; i < good.size(); i++) {
    auto bad = good;
    bad[i].level0 = 1;
    EXPECT_FALSE(decode_read_slots(bad.data(), bad.size(), bits.size(), out.data(), 0)) << "symbol " << i;
  }
}

TEST(RmtOneWireCodec, I5_SurplusSymbolsWithDurationAreInvalid) {
  const std::vector<bool> bits{true, false, true};
  std::array<uint8_t, 2> out{};
  for (const Symbol &surplus : {Symbol{4, 0, 76, 1}, Symbol{1, 0, 0, 1}, Symbol{0, 0, 1, 1}, Symbol{1, 1, 0, 0},
                                Symbol{0, 1, 1, 1}, Symbol{0, 1, 0, 0}}) {
    auto capture = capture_of(bits);
    capture.push_back(surplus);
    EXPECT_FALSE(decode_read_slots(capture.data(), capture.size(), bits.size(), out.data(), 0))
        << surplus.duration0 << "/" << surplus.duration1;
    // Behind zero-length markers it is still surplus
    capture = capture_of(bits);
    capture.push_back(Symbol{0, 1, 0, 1});
    capture.push_back(surplus);
    EXPECT_FALSE(decode_read_slots(capture.data(), capture.size(), bits.size(), out.data(), 0));
  }
}

TEST(RmtOneWireCodec, I5_FuzzMutatedCaptures) {
  std::mt19937 rng(0x1E5);
  int accepted = 0;
  int rejected = 0;
  for (int round = 0; round < 50000; round++) {
    const size_t count = 1 + rng() % 62;
    std::vector<bool> bits;
    for (size_t i = 0; i < count; i++)
      bits.push_back(rng() & 1);
    auto capture = capture_of(bits);
    bool ends_on_low = false;
    switch (rng() % 11) {
      case 0:  // complete
        break;
      case 1:  // truncated
        capture.resize(rng() % capture.size());
        break;
      case 2:  // surplus with a duration
        capture.push_back(Symbol{static_cast<uint16_t>(rng() % 100), 0, static_cast<uint16_t>(1 + rng() % 100), 1});
        break;
      case 3:  // surplus end markers
        capture.insert(capture.end(), 1 + rng() % 3, Symbol{0, static_cast<uint16_t>(rng() & 1), 0, 1});
        break;
      case 4:  // wrong level in a slot
        capture[rng() % capture.size()].level0 = 1;
        break;
      case 5:  // truncated, then end markers
        capture.resize(rng() % capture.size());
        capture.insert(capture.end(), 1 + rng() % 3, Symbol{0, 0, 0, static_cast<uint16_t>(rng() & 1)});
        break;
      case 6:  // ends on Low
        capture.back().level1 = 0;
        break;
      case 7:  // Low of a whole slot or more
        capture[rng() % capture.size()].duration0 = Timing{}.slot_us + rng() % 200;
        break;
      case 8:  // longest allowed Low
        capture[rng() % capture.size()].duration0 = Timing{}.slot_us - 1;
        break;
      case 9:  // the last symbol ends on Low, duration1 is whatever was in the memory
        capture.back() = Symbol{0, 0, static_cast<uint16_t>(rng() % 3 == 0 ? 0 : rng() % 32768), 1};
        ends_on_low = true;
        break;
      case 10:  // ends on Low behind a complete capture
        capture.push_back(Symbol{0, 0, static_cast<uint16_t>(rng() % 3 == 0 ? 0 : rng() % 32768), 1});
        ends_on_low = true;
        break;
    }
    const bool valid = capture_is_valid(capture, count);
    std::array<uint8_t, 9> out{};
    const Symbol unused{};
    const bool decoded =
        decode_read_slots(capture.empty() ? &unused : capture.data(), capture.size(), count, out.data(), 0);
    ASSERT_EQ(decoded, valid) << "round " << round << " symbols " << capture.size() << " expected " << count;
    if (ends_on_low) {
      ASSERT_FALSE(decoded) << "round " << round << ": duration1 " << capture.back().duration1;
    }
    if (!decoded) {
      rejected++;
      continue;
    }
    accepted++;
    for (size_t i = 0; i < count; i++)
      ASSERT_EQ(get_bit(out.data(), i), capture[i].duration0 < 13) << "round " << round << " bit " << i;
  }
  // The fuzz must exercise both outcomes to mean anything
  EXPECT_GT(accepted, 1000);
  EXPECT_GT(rejected, 1000);
}

TEST(RmtOneWireCodec, I5_FuzzRandomGarbage) {
  std::mt19937 rng(0xBAD);
  for (int round = 0; round < 50000; round++) {
    const size_t expected = 1 + rng() % 62;
    std::vector<Symbol> capture(rng() % 70);
    for (auto &symbol : capture) {
      const uint32_t low = rng() % 8 == 0 ? rng() % 200 : 1 + rng() % (Timing{}.slot_us - 1);
      symbol = Symbol{static_cast<uint16_t>(low), static_cast<uint16_t>(rng() % 16 == 0),
                      static_cast<uint16_t>(rng() % 4 == 0 ? 0 : rng() % 200), 1};
    }
    if (!capture.empty()) {
      capture.back().level1 = rng() & 1;
      // A marker that starts High and carries Low has no defined meaning in section 7
      if (capture.back().duration0 == 0 && capture.back().duration1 == 0 && capture.back().level0 == 1)
        capture.back().level1 = 1;
    }
    std::array<uint8_t, 9> out{};
    const Symbol unused{};
    const bool decoded =
        decode_read_slots(capture.empty() ? &unused : capture.data(), capture.size(), expected, out.data(), 0);
    ASSERT_EQ(decoded, capture_is_valid(capture, expected)) << "round " << round;
    if (decoded) {
      for (size_t i = 0; i < expected; i++)
        ASSERT_EQ(get_bit(out.data(), i), capture[i].duration0 < 13) << "round " << round << " bit " << i;
    }
  }
}

// ---- Timing, section 4.6 and section 15 --------------------------------------------------------

namespace {

void expect_same_symbol(const Symbol &a, const Symbol &b) {
  EXPECT_EQ(a.duration0, b.duration0);
  EXPECT_EQ(a.level0, b.level0);
  EXPECT_EQ(a.duration1, b.duration1);
  EXPECT_EQ(a.level1, b.level1);
}

void expect_slot(const Symbol &slot, uint16_t low, uint16_t high) {
  EXPECT_EQ(slot.duration0, low);
  EXPECT_EQ(slot.level0, 0);
  EXPECT_EQ(slot.duration1, high);
  EXPECT_EQ(slot.level1, 1);
}

}  // namespace

TEST(RmtOneWireCodec, Timing_DefaultsAreTheSpecValues) {
  const Timing timing;
  EXPECT_EQ(timing.write0_low_us, 90);
  EXPECT_EQ(timing.slot_us, 110);
  EXPECT_EQ(timing.read_threshold_us, 13);
}

TEST(RmtOneWireCodec, Timing_NoArgumentEncodesLikeTheSpecValues) {
  const Timing spec_values{90, 110, 13};
  for (int value = 0; value < 256; value++) {
    const uint8_t byte = static_cast<uint8_t>(value);
    std::array<Symbol, 8> without{};
    std::array<Symbol, 8> with{};
    ASSERT_EQ(encode_write_slots(&byte, 0, 8, without.data()), 8u);
    ASSERT_EQ(encode_write_slots(&byte, 0, 8, with.data(), spec_values), 8u);
    for (size_t bit = 0; bit < 8; bit++)
      expect_same_symbol(without[bit], with[bit]);
  }
  std::array<Symbol, 8> without{};
  std::array<Symbol, 8> with{};
  ASSERT_EQ(encode_read_slots(8, without.data()), 8u);
  ASSERT_EQ(encode_read_slots(8, with.data(), spec_values), 8u);
  for (size_t bit = 0; bit < 8; bit++)
    expect_same_symbol(without[bit], with[bit]);
}

TEST(RmtOneWireCodec, Timing_NoArgumentDecodesLikeTheSpecValues) {
  const Timing spec_values{90, 110, 13};
  for (uint16_t low = 1; low < 100; low++) {
    const Symbol capture{low, 0, 0, 1};
    uint8_t without = 0x55;
    uint8_t with = 0x55;
    const bool result_without = decode_read_slots(&capture, 1, 1, &without, 0);
    const bool result_with = decode_read_slots(&capture, 1, 1, &with, 0, spec_values);
    EXPECT_EQ(result_without, result_with) << "Low time " << low;
    if (result_without) {
      EXPECT_EQ(without & 1, with & 1) << "Low time " << low;
    }
  }
}

TEST(RmtOneWireCodec, Timing_LongSlotChangesTheEncodedSymbols) {
  // Write 1 Low and Read Low stay fixed, the High times are the slot minus the Low time
  const Timing timing{90, 120, 13};
  const uint8_t ones = 0xFF;
  const uint8_t zeros = 0x00;
  Symbol one{};
  Symbol zero{};
  Symbol read{};
  ASSERT_EQ(encode_write_slots(&zeros, 0, 1, &zero, timing), 1u);
  ASSERT_EQ(encode_write_slots(&ones, 0, 1, &one, timing), 1u);
  ASSERT_EQ(encode_read_slots(1, &read, timing), 1u);
  expect_slot(zero, 90, 30);
  expect_slot(one, 6, 114);
  expect_slot(read, 4, 116);
}

TEST(RmtOneWireCodec, Timing_ThresholdDecidesTheBit) {
  const Timing timing{65, 80, 25};
  for (uint16_t low : {uint16_t{13}, uint16_t{24}}) {
    const Symbol capture{low, 0, 0, 1};
    uint8_t out = 0x00;
    ASSERT_TRUE(decode_read_slots(&capture, 1, 1, &out, 0, timing)) << low;
    EXPECT_EQ(out & 1, 1) << "Low time " << low;
  }
  for (uint16_t low : {uint16_t{25}, uint16_t{26}}) {
    const Symbol capture{low, 0, 0, 1};
    uint8_t out = 0xFF;
    ASSERT_TRUE(decode_read_slots(&capture, 1, 1, &out, 0, timing)) << low;
    EXPECT_EQ(out & 1, 0) << "Low time " << low;
  }
}

TEST(RmtOneWireCodec, Timing_SlotLimitsTheLowTime) {
  // Section 7 point 3 follows Timing::slot_us, at every position
  const Timing timing{90, 120, 13};
  const std::vector<bool> bits{true, true, true, true};
  for (size_t position = 0; position < bits.size(); position++) {
    auto capture = capture_of(bits);
    uint8_t out = 0;
    for (uint16_t low : {uint16_t{80}, uint16_t{119}}) {
      capture[position].duration0 = low;
      out = 0;
      ASSERT_TRUE(decode_read_slots(capture.data(), capture.size(), bits.size(), &out, 0, timing))
          << "position " << position << " low " << low;
      EXPECT_EQ(out, 0x0F & ~(1u << position)) << "position " << position << " low " << low;
    }
    capture[position].duration0 = 120;
    EXPECT_FALSE(decode_read_slots(capture.data(), capture.size(), bits.size(), &out, 0, timing))
        << "position " << position;
  }
}

}  // namespace esphome::gpio::rmt_one_wire::testing
