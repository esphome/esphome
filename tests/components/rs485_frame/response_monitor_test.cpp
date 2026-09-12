// USE_RS485_FRAME_RESPONSE_MONITOR is normally emitted by to_code only when a hub's YAML
// config has a response_fields:/response_monitor: block. rs485_frame is MULTI_CONF, so the
// unit-test harness's synthesized empty-list config never runs codegen for it at all (see
// sniffer_stats_test.cpp's comment on the same issue) -- so #include the real .cpp directly,
// compiling the actual implementation under the same macro in one translation unit.
#define USE_RS485_FRAME_RESPONSE_MONITOR

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/rs485_frame/response_monitor.cpp"  // NOLINT

namespace esphome::rs485_frame::testing {

namespace {

// Exposes get_stat() (already public) plus a couple of scenario helpers so each test reads
// as "declare a field + entry, drive TX/RX events, assert the resulting counters" without
// repeating the same four add_field/add_entry/add_changed_alt calls in every test body.
class ResponseMonitorProbe : public ResponseMonitor {
 public:
  // field 0: [0x01, 0x02] carrier, 4-byte little-endian value at payload[2..5] -- an AquaLogic
  // LED-mask-shaped carrier, representative of a real response_fields: entry.
  void setup_changed_field() {
    this->add_field(/*frame_type=*/{0x01, 0x02}, /*frame_type_mask=*/{0xFF, 0xFF}, /*offset=*/2, /*length=*/4,
                    /*big_endian=*/false);
  }

  // entry 0: trigger byte [0xAA], 200ms window, single `changed` alt on field 0, mask 0x40 --
  // the toggle-style LED signature shape: `changed`+mask, not `masked_int`, since a toggle
  // button's response is a bit flipping, not landing on one known absolute value.
  uint8_t setup_changed_entry(uint32_t window_ms = 200) {
    uint8_t entry = this->add_entry(/*trigger=*/{0xAA}, window_ms);
    this->add_changed_alt(entry, /*field_index=*/0, /*mask=*/0x40);
    return entry;
  }

  // Feeds one RX frame for field 0 with the given 4-byte little-endian value, establishing or
  // updating its ambient snapshot without any entry pending.
  void feed_led_mask(uint32_t value, uint32_t now) {
    std::vector<uint8_t> payload = {
        0x01, 0x02, uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)};
    this->on_frame_received(payload, now);
  }

  // field 0: [0x01, 0x03] carrier, 6-byte field at payload[2..7] -- deliberately longer than
  // decode_int_'s 4-byte window, representative of an AquaLogic display-text response_fields:
  // entry (kept short here for a readable test payload; the real field is up to 36 bytes).
  void setup_text_field() {
    this->add_field(/*frame_type=*/{0x01, 0x03}, /*frame_type_mask=*/{0xFF, 0xFF}, /*offset=*/2, /*length=*/6,
                    /*big_endian=*/true);
  }

  // entry 0: trigger byte [0xAA], 200ms window, single `changed` alt on field 0. mask is the
  // config-layer default (all bits) -- meaningless for a >4-byte field, ignored by eval_alt_'s
  // full-length byte-compare path.
  uint8_t setup_text_changed_entry(uint32_t window_ms = 200) {
    uint8_t entry = this->add_entry(/*trigger=*/{0xAA}, window_ms);
    this->add_changed_alt(entry, /*field_index=*/0, /*mask=*/0xFFFFFFFF);
    return entry;
  }

  // Feeds one RX frame for field 0 with the given 6-byte value, establishing or updating its
  // ambient snapshot without any entry pending.
  void feed_text(const std::array<uint8_t, 6> &text, uint32_t now) {
    std::vector<uint8_t> payload = {0x01, 0x03};
    payload.insert(payload.end(), text.begin(), text.end());
    this->on_frame_received(payload, now);
  }
};

}  // namespace

// Acceptance case 1: success -- a trigger fires, the addressed field's masked bits flip within
// the window, and the entry resolves as a confirmed press.
TEST(ResponseMonitorTest, SuccessWhenMaskedBitsChangeWithinWindow) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();

  rm.feed_led_mask(0x00000000, /*now=*/0);   // establish ambient baseline, bit 0x40 clear
  rm.on_trigger_sent({0xAA}, /*now=*/10);    // arms the entry, 200ms window from t=10
  rm.feed_led_mask(0x00000040, /*now=*/60);  // bit 0x40 now set -- a real transition

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 1u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_FAIL), 0u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_TIMEOUT), 0u);

  // process_timeouts after the window has fully elapsed must not re-resolve an already-
  // resolved (no longer pending) entry.
  rm.process_timeouts(/*now=*/500);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 1u);
}

// Acceptance case 2: wrong-signature -- the addressed field arrives during the window (proving
// the bus and the field addressing both work), but its masked bits never change. This must be
// distinguished from case 3 (nothing arrived at all).
TEST(ResponseMonitorTest, FailWhenAddressedFieldArrivesButNeverChanges) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();

  rm.feed_led_mask(0x00000000, /*now=*/0);
  rm.on_trigger_sent({0xAA}, /*now=*/10);
  rm.feed_led_mask(0x00000000, /*now=*/60);  // same field, same value -- not a transition
  rm.process_timeouts(/*now=*/9999);         // window (200ms from t=10) has long since elapsed

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_FAIL), 1u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 0u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_TIMEOUT), 0u);
}

// Acceptance case 3: timeout-with-no-response -- the trigger fires but the addressed field
// never arrives at all before the window elapses.
TEST(ResponseMonitorTest, TimeoutWhenAddressedFieldNeverArrives) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();

  rm.feed_led_mask(0x00000000, /*now=*/0);
  rm.on_trigger_sent({0xAA}, /*now=*/10);
  // No RX at all before the window (200ms from t=10) elapses.
  rm.process_timeouts(/*now=*/9999);

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_TIMEOUT), 1u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_FAIL), 0u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 0u);
}

// Acceptance case 4 (confirm.md's orphan requirement): a response signature match observed
// with no matching pending trigger -- e.g. someone pressed the physical panel button directly.
TEST(ResponseMonitorTest, OrphanWhenSignatureMatchesWithNoPendingTrigger) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();

  rm.feed_led_mask(0x00000000, /*now=*/0);   // baseline, bit 0x40 clear
  rm.feed_led_mask(0x00000040, /*now=*/50);  // bit flips with no trigger ever sent

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_ORPHAN), 1u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 0u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_FAIL), 0u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_TIMEOUT), 0u);
}

// masked_int mode: exact match against a known absolute value, independent of prior state --
// unlike `changed`, a masked_int alt must resolve SUCCESS even on the very first observation
// of the field (no ambient baseline required).
TEST(ResponseMonitorTest, MaskedIntMatchesAbsoluteValueWithNoBaseline) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.add_entry({0xBB}, 200);
  rm.add_masked_int_alt(entry, /*field_index=*/0, /*mask=*/0xFFFFFFFF, /*values=*/{100});

  rm.on_trigger_sent({0xBB}, /*now=*/0);
  rm.feed_led_mask(100, /*now=*/5);

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 1u);
}

// text_enum mode: ASCII-decodes the field (stripping the display's blink/inverse bit 7 and
// trailing space padding) and compares against a small set of known target strings.
TEST(ResponseMonitorTest, TextEnumMatchesTrimmedDecodedString) {
  ResponseMonitorProbe rm;
  // A wider field: [0x01, 0x03] display carrier, 10-byte text at payload[2..11].
  rm.add_field({0x01, 0x03}, {0xFF, 0xFF}, /*offset=*/2, /*length=*/10, /*big_endian=*/true);
  uint8_t entry = rm.add_entry({0xCC}, 200);
  rm.add_text_enum_alt(entry, /*field_index=*/0, {"Auto Control", "Manual Off"});

  rm.on_trigger_sent({0xCC}, /*now=*/0);
  // "Manual Off" padded with spaces to fill the 10-byte field, high bit set on one byte (the
  // display's blink-flag convention) to prove it gets stripped before comparison.
  std::vector<uint8_t> payload = {0x01, 0x03, 'M', 'a', 'n', 'u', 'a', 'l', ' ', 0xCF /* 'O'|0x80 */, 'f', 'f'};
  rm.on_frame_received(payload, /*now=*/5);

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 1u);
}

// changed_gated mode: when the gate does not hold at trigger time, the occurrence is
// NOT_APPLICABLE (not armed, not a failure) -- e.g. a Plus/Minus button whose confirmation
// only applies while a submenu is open; without gating, every press outside that submenu
// would misreport as a failure instead of "precondition not met".
TEST(ResponseMonitorTest, ChangedGatedIsNotApplicableWhenGateDoesNotHoldAtTriggerTime) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();                               // field 0: the watched value
  rm.add_field({0x01, 0x09}, {0xFF, 0xFF}, 2, 4, false);  // field 1: the gate flag
  uint8_t entry = rm.add_entry({0xDD}, 200);
  rm.add_changed_gated_alt(entry, /*field_index=*/0, /*mask=*/0x40, /*gate_field_index=*/1,
                           /*gate_mask=*/0x01, /*gate_value=*/0x01);

  // Gate field observed as 0 (does not match gate_value 0x01) before the trigger fires.
  std::vector<uint8_t> gate_payload = {0x01, 0x09, 0x00, 0x00, 0x00, 0x00};
  rm.on_frame_received(gate_payload, /*now=*/0);
  rm.feed_led_mask(0x00000000, /*now=*/0);

  rm.on_trigger_sent({0xDD}, /*now=*/10);

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_NOT_APPLICABLE), 1u);
  // Not armed at all -- a later matching RX must not resolve it as SUCCESS.
  rm.feed_led_mask(0x00000040, /*now=*/50);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 0u);
}

// Review finding 1: two signature alts sharing one field_index must not double-count a single
// physical RX event as two orphans. Regression test for a missing `break` after resolving an
// orphan (unlike the pending/SUCCESS branch a few lines above, which does break).
TEST(ResponseMonitorTest, OrphanCountedOnceEvenWhenTwoAltsShareOneField) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();  // field 0
  uint8_t entry = rm.add_entry({0xEE}, 200);
  rm.add_changed_alt(entry, /*field_index=*/0, /*mask=*/0x40);
  rm.add_changed_alt(entry, /*field_index=*/0, /*mask=*/0x40);  // second alt, same field

  rm.feed_led_mask(0x00000000, /*now=*/0);   // baseline
  rm.feed_led_mask(0x00000040, /*now=*/50);  // one RX event; both alts evaluate true; no trigger sent

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_ORPHAN), 1u);
}

// Review finding 4: a trigger that fires while the entry is already pending from an earlier,
// unresolved occurrence must not silently discard that earlier occurrence's outcome.
TEST(ResponseMonitorTest, RearmWhilePendingResolvesThePriorOccurrenceFirst) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();  // trigger {0xAA}, 200ms window, changed+0x40 on field 0

  rm.feed_led_mask(0x00000000, /*now=*/0);
  rm.on_trigger_sent({0xAA}, /*now=*/10);  // arm #1: deadline 210, nothing has arrived yet
  rm.on_trigger_sent({0xAA}, /*now=*/50);  // arm #2 (double press) while #1 is still pending

  // Occurrence #1 saw nothing before being superseded -- it must be counted as TIMEOUT right
  // away, not dropped when #2 overwrites pending/deadline/saw_any_match.
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_TIMEOUT), 1u);

  rm.feed_led_mask(0x00000040, /*now=*/100);  // response to the SECOND press

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 1u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_TIMEOUT), 1u);
}

// Review finding 5: an RX matching an alt whose gate is inactive this arm cycle must not mark
// the entry "seen" -- otherwise process_timeouts reports FAIL (wrong signature) instead of
// TIMEOUT (nothing addressed arrived) for a genuinely absent response, hiding real comm gaps.
TEST(ResponseMonitorTest, InactiveAltFieldArrivalDoesNotCountAsSeenForTimeoutClassification) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();                               // field 0: the entry's real target
  rm.add_field({0x01, 0x09}, {0xFF, 0xFF}, 2, 4, false);  // field 1: gate flag
  rm.add_field({0x01, 0x0A}, {0xFF, 0xFF}, 2, 4, false);  // field 2: a second, gated-off signal

  uint8_t entry = rm.add_entry({0xFF}, 200);
  // alt 0: changed_gated on field 2, gated by field 1 -- the gate never holds below, so this
  // alt is never active.
  rm.add_changed_gated_alt(entry, /*field_index=*/2, /*mask=*/0xFF, /*gate_field_index=*/1,
                           /*gate_mask=*/0x01, /*gate_value=*/0x01);
  // alt 1: plain `changed` on field 0 -- always active, the entry's real signature.
  rm.add_changed_alt(entry, /*field_index=*/0, /*mask=*/0x40);

  // Baselines: gate field reads 0 (gate never holds).
  std::vector<uint8_t> gate_payload = {0x01, 0x09, 0x00, 0x00, 0x00, 0x00};
  rm.on_frame_received(gate_payload, /*now=*/0);
  rm.feed_led_mask(0x00000000, /*now=*/0);
  std::vector<uint8_t> field2_payload = {0x01, 0x0A, 0x00, 0x00, 0x00, 0x00};
  rm.on_frame_received(field2_payload, /*now=*/0);

  rm.on_trigger_sent({0xFF}, /*now=*/10);  // arms: alt 1 active, alt 0 inactive (gate false)

  // field 2 (the inactive alt's field) arrives and DOES change -- but that alt is gated off,
  // so this must not count as "the addressed field was seen".
  std::vector<uint8_t> field2_changed = {0x01, 0x0A, 0x01, 0x00, 0x00, 0x00};
  rm.on_frame_received(field2_changed, /*now=*/50);

  // field 0 (alt 1's real target) never arrives before the window elapses.
  rm.process_timeouts(/*now=*/9999);

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_TIMEOUT), 1u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_FAIL), 0u);
}

// Review follow-up: proves the ambient/baseline freshness guarantee that
// OrphanWhenSignatureMatchesWithNoPendingTrigger does not -- an untriggered RX must refresh the
// ambient snapshot it uses for `changed` comparisons, not leave a stale baseline from setup.
TEST(ResponseMonitorTest, AmbientBaselineIsRefreshedByAnUntriggeredRxNotStaleFromSetup) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();

  rm.feed_led_mask(0x00000000, /*now=*/0);   // baseline at t=0: bit clear
  rm.feed_led_mask(0x00000040, /*now=*/50);  // untriggered RX sets the bit -- orphan; must refresh ambient

  rm.on_trigger_sent({0xAA}, /*now=*/100);
  rm.feed_led_mask(0x00000040, /*now=*/150);  // unchanged relative to the t=50 value

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_ORPHAN), 1u);
  rm.process_timeouts(/*now=*/9999);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_FAIL), 1u)
      << "ambient baseline must be refreshed by the t=50 orphan RX, not stale from t=0";
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 0u);
}

// `changed` on a field longer than 4 bytes must compare the field's full
// byte range, not just the first 4 bytes decode_int_ can address -- a menu/display-text button
// (e.g. AquaLogic's Menu/Left/Right) whose only observable effect lands past byte 4 would
// otherwise never be detected as "changed".
TEST(ResponseMonitorTest, ChangedOnFieldOverFourBytesComparesFullByteRangeNotJustFirstFour) {
  ResponseMonitorProbe rm;
  rm.setup_text_field();
  uint8_t entry = rm.setup_text_changed_entry();

  rm.feed_text({'A', 'B', 'C', 'D', 'E', 'F'}, /*now=*/0);  // baseline
  rm.on_trigger_sent({0xAA}, /*now=*/10);
  // Only byte index 5 -- past decode_int_'s 4-byte window -- differs from the baseline.
  rm.feed_text({'A', 'B', 'C', 'D', 'E', 'G'}, /*now=*/60);

  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 1u)
      << "a change past the first 4 bytes of a >4-byte field must still register as 'changed'";
}

TEST(ResponseMonitorTest, ChangedOnFieldOverFourBytesFailsWhenBytesAreIdentical) {
  ResponseMonitorProbe rm;
  rm.setup_text_field();
  uint8_t entry = rm.setup_text_changed_entry();

  rm.feed_text({'A', 'B', 'C', 'D', 'E', 'F'}, /*now=*/0);  // baseline
  rm.on_trigger_sent({0xAA}, /*now=*/10);
  rm.feed_text({'A', 'B', 'C', 'D', 'E', 'F'}, /*now=*/60);  // identical -- no real change

  rm.process_timeouts(/*now=*/9999);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_FAIL), 1u);
  EXPECT_EQ(rm.get_stat(entry, RESPONSE_MONITOR_STAT_SUCCESS), 0u);
}

// on_confirmed:/on_failed: automation triggers. A SUCCESS resolution must fire the entry's
// on_confirmed callback exactly once, and must not touch on_failed.
TEST(ResponseMonitorTest, SuccessFiresOnConfirmedCallbackOnce) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();
  int confirmed = 0;
  int failed = 0;
  rm.add_on_confirmed_callback(entry, [&]() { confirmed++; });
  rm.add_on_failed_callback(entry, [&]() { failed++; });

  rm.feed_led_mask(0x00000000, /*now=*/0);
  rm.on_trigger_sent({0xAA}, /*now=*/10);
  rm.feed_led_mask(0x00000040, /*now=*/60);  // real transition -- SUCCESS

  EXPECT_EQ(confirmed, 1);
  EXPECT_EQ(failed, 0);
}

// A FAIL resolution (addressed field arrived but signature never matched) must fire
// on_failed exactly once, and must not touch on_confirmed.
TEST(ResponseMonitorTest, FailFiresOnFailedCallbackOnce) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();
  int confirmed = 0;
  int failed = 0;
  rm.add_on_confirmed_callback(entry, [&]() { confirmed++; });
  rm.add_on_failed_callback(entry, [&]() { failed++; });

  rm.feed_led_mask(0x00000000, /*now=*/0);
  rm.on_trigger_sent({0xAA}, /*now=*/10);
  rm.feed_led_mask(0x00000000, /*now=*/60);  // same value -- not a transition
  rm.process_timeouts(/*now=*/9999);

  EXPECT_EQ(failed, 1);
  EXPECT_EQ(confirmed, 0);
}

// A TIMEOUT resolution (addressed field never arrived at all) must also fire on_failed --
// on_failed: covers both wrong-signature and no-response outcomes alike.
TEST(ResponseMonitorTest, TimeoutFiresOnFailedCallbackOnce) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();
  int confirmed = 0;
  int failed = 0;
  rm.add_on_confirmed_callback(entry, [&]() { confirmed++; });
  rm.add_on_failed_callback(entry, [&]() { failed++; });

  rm.feed_led_mask(0x00000000, /*now=*/0);
  rm.on_trigger_sent({0xAA}, /*now=*/10);
  // No RX at all before the window elapses.
  rm.process_timeouts(/*now=*/9999);

  EXPECT_EQ(failed, 1);
  EXPECT_EQ(confirmed, 0);
}

// NOT_APPLICABLE (a changed_gated entry whose gate does not hold at trigger time) is neither
// a confirmation nor a failure -- it must fire neither callback.
TEST(ResponseMonitorTest, NotApplicableFiresNeitherCallback) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();                               // field 0: the watched value
  rm.add_field({0x01, 0x09}, {0xFF, 0xFF}, 2, 4, false);  // field 1: the gate flag
  uint8_t entry = rm.add_entry({0xDD}, 200);
  rm.add_changed_gated_alt(entry, /*field_index=*/0, /*mask=*/0x40, /*gate_field_index=*/1,
                           /*gate_mask=*/0x01, /*gate_value=*/0x01);
  int confirmed = 0;
  int failed = 0;
  rm.add_on_confirmed_callback(entry, [&]() { confirmed++; });
  rm.add_on_failed_callback(entry, [&]() { failed++; });

  std::vector<uint8_t> gate_payload = {0x01, 0x09, 0x00, 0x00, 0x00, 0x00};  // gate never holds
  rm.on_frame_received(gate_payload, /*now=*/0);
  rm.feed_led_mask(0x00000000, /*now=*/0);

  rm.on_trigger_sent({0xDD}, /*now=*/10);  // resolves NOT_APPLICABLE immediately

  EXPECT_EQ(confirmed, 0);
  EXPECT_EQ(failed, 0);
}

// ORPHAN (a signature match with no trigger pending) must not fire on_failed -- no press was
// ever made through this entry's own trigger, so there is nothing to report as failed.
TEST(ResponseMonitorTest, OrphanFiresNeitherCallback) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();
  int confirmed = 0;
  int failed = 0;
  rm.add_on_confirmed_callback(entry, [&]() { confirmed++; });
  rm.add_on_failed_callback(entry, [&]() { failed++; });

  rm.feed_led_mask(0x00000000, /*now=*/0);   // baseline, bit 0x40 clear
  rm.feed_led_mask(0x00000040, /*now=*/50);  // bit flips with no trigger ever sent -- orphan

  EXPECT_EQ(confirmed, 0);
  EXPECT_EQ(failed, 0);
}

// Multiple on_confirmed: actions on the same entry (YAML allows a list under one trigger key)
// must all fire, not just the first registered.
TEST(ResponseMonitorTest, MultipleOnConfirmedCallbacksOnSameEntryAllFire) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();
  uint8_t entry = rm.setup_changed_entry();
  int first = 0;
  int second = 0;
  rm.add_on_confirmed_callback(entry, [&]() { first++; });
  rm.add_on_confirmed_callback(entry, [&]() { second++; });

  rm.feed_led_mask(0x00000000, /*now=*/0);
  rm.on_trigger_sent({0xAA}, /*now=*/10);
  rm.feed_led_mask(0x00000040, /*now=*/60);

  EXPECT_EQ(first, 1);
  EXPECT_EQ(second, 1);
}

// A second entry's callbacks must never fire from the first entry's resolution -- proves
// on_confirmed_callbacks_/on_failed_callbacks_ are correctly index-aligned with entries_.
TEST(ResponseMonitorTest, CallbacksAreIsolatedPerEntry) {
  ResponseMonitorProbe rm;
  rm.setup_changed_field();  // field 0, shared by both entries below
  uint8_t entry_a = rm.add_entry({0xAA}, 200);
  rm.add_changed_alt(entry_a, /*field_index=*/0, /*mask=*/0x40);
  uint8_t entry_b = rm.add_entry({0xBB}, 200);
  rm.add_changed_alt(entry_b, /*field_index=*/0, /*mask=*/0x80);

  int confirmed_a = 0;
  int confirmed_b = 0;
  rm.add_on_confirmed_callback(entry_a, [&]() { confirmed_a++; });
  rm.add_on_confirmed_callback(entry_b, [&]() { confirmed_b++; });

  rm.feed_led_mask(0x00000000, /*now=*/0);
  rm.on_trigger_sent({0xAA}, /*now=*/10);    // arms entry_a only
  rm.feed_led_mask(0x000000C0, /*now=*/60);  // both masked bits (0x40 and 0x80) change

  EXPECT_EQ(confirmed_a, 1);
  EXPECT_EQ(confirmed_b, 0);
}

}  // namespace esphome::rs485_frame::testing
