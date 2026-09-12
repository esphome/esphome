// USE_RS485_FRAME_SNIFFER_STATS is normally emitted by to_code only when a hub's YAML
// config has a sniffer_stats: block. rs485_frame is MULTI_CONF, so the unit-test harness's
// synthesized empty-list config never runs codegen for it at all (see
// script/build_helpers.py's populate_dependency_config), and the harness suppresses to_code
// by default anyway -- there's no per-instance call for a manifest override to hook, and no
// route to a real-config codegen path without also standing up a full uart+hub fixture that
// the harness can compile on host. Defining the macro directly (as
// tests/components/time/posix_tz_parser.cpp does for USE_TIME_TIMEZONE) only covers this
// translation unit, and sniffer_stats.cpp is a separate one -- so #include the real .cpp
// directly, compiling the actual implementation under the same macro in one unit.
#define USE_RS485_FRAME_SNIFFER_STATS

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/rs485_frame/sniffer_stats.cpp"  // NOLINT

namespace esphome::rs485_frame::testing {

namespace {

// Exposes the protected entries_ table and dump_payloads_() so tests can inspect both
// what record() captured AND what the periodic console dump actually renders, instead of
// scraping ESP_LOGI output.
class SnifferStatsProbe : public SnifferStats {
 public:
  size_t entry_count() const { return this->entries_.size(); }
  const SnifferEntry &entry(size_t i) const { return this->entries_[i]; }

  // Runs the real dump_payloads_() (the periodic sniffer_stats console table's hex+ASCII
  // "payloads:" section) for entry 0 only, then returns the exact strings it wrote into
  // hex_buf_/ascii_buf_ -- the same buffers dump_payloads_() hands to ESP_LOGI.
  std::pair<std::string, std::string> dump_entry_zero_for_test() {
    const uint8_t order[] = {0};
    this->dump_payloads_(/*top_n=*/1, order);
    return {std::string(this->hex_buf_.get()), std::string(this->ascii_buf_.get())};
  }
};

}  // namespace

// A forum report claimed the periodic sniffer dump stayed capped at 32 bytes
// even with payload_capture_bytes: 48 set, despite dump_frames showing the full frame.
// The console dump the reporter saw is dump_payloads_()'s hex+ASCII "payloads:" line, a
// SEPARATE formatting step from what record() captures -- proving record()'s capture isn't
// truncated does not prove the console line isn't, since dump_payloads_() re-renders from
// hex_buf_/ascii_buf_ with its own byte-by-byte loop. This test drives both steps with the
// reporter's real 35-byte Hayward payload and checks the actual rendered console strings,
// not just the internal capture buffer.
TEST(SnifferStatsTest, RecordCapturesFullPayloadPastLegacyThirtyTwoByteDefault) {
  SnifferStatsProbe stats;
  stats.init(/*max_entries=*/8, /*interval_ms=*/60000, /*payload_dump_top=*/4,
             /*max_unique_payloads=*/24, /*payload_capture_bytes=*/48,
             /*reference_frame_type=*/{}, /*strip_high_bit=*/true, /*reference_mode_send=*/false);

  // Reporter's real 35-byte Hayward display payload (decoded from their pasted dump_frames
  // hex): "\x01\x03 Filter Lo-all         to 05:30P\x00"
  const std::vector<uint8_t> payload = {0x01, 0x03, 0x20, 0x46, 0x69, 0x6c, 0x74, 0x65, 0x72, 0x20, 0x4c, 0x6f,
                                        0x2d, 0x61, 0x6c, 0x6c, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                        0x20, 0x74, 0x6f, 0x20, 0x30, 0x35, 0x3a, 0x33, 0x30, 0x50, 0x00};
  ASSERT_EQ(payload.size(), 35u);

  stats.record(payload, /*now=*/0);

  // --- Capture step: what record() stored ---
  ASSERT_EQ(stats.entry_count(), 1u);
  const SnifferEntry &e = stats.entry(0);
  ASSERT_EQ(e.unique_count, 1u);
  const PayloadCapture &captured = e.payloads[0];
  EXPECT_EQ(captured.len, 35u) << "payload_capture_bytes=48 should not truncate a 35-byte payload";
  EXPECT_EQ(std::vector<uint8_t>(captured.bytes.get(), captured.bytes.get() + captured.len), payload);

  // --- Console-dump step: what dump_payloads_() actually renders ---
  const auto [hex, ascii] = stats.dump_entry_zero_for_test();
  EXPECT_EQ(ascii.size(), 35u) << "rendered ASCII preview should cover all 35 captured bytes";
  // Non-printable bytes (0x01, 0x03, and the trailing 0x00) render as '.'. Bug report's exact
  // symptom: the trailing "0P" of "05:30P" got cut from the console line -- confirm it's not.
  EXPECT_EQ(ascii, ".. Filter Lo-all         to 05:30P.");
  // hex_buf_ is "XX " per byte (35 * 3 chars); confirm the last byte (0x00) made it in,
  // not just everything up to a stale 32-byte cutoff.
  EXPECT_EQ(hex.size(), 35u * 3);
  EXPECT_EQ(hex.substr(hex.size() - 9), "30 50 00 ");
}

}  // namespace esphome::rs485_frame::testing
