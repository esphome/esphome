#pragma once

// defines.h must come before the #ifdef gate so a translation unit that opens this header
// without first including a core component header (e.g., sniffer_stats.cpp itself) still
// sees USE_RS485_FRAME_SNIFFER_STATS before the conditional is evaluated.
#include "esphome/core/defines.h"

#ifdef USE_RS485_FRAME_SNIFFER_STATS

#include "esphome/core/helpers.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace esphome::rs485_frame {

// Ring-buffer length of recent inter-arrival samples per frame_type. Median is computed
// from this window at dump time; min/max are tracked exactly across the whole period.
// Fixed at compile time because 16 is a good window for any sane bus and there's no
// reason a user would want to tune it.
static constexpr size_t SNIFFER_RECENT_DELAYS = 16;

// Upper bound for the user-configurable max_frame_types schema option. Each entry
// allocates max_unique_payloads * payload_capture_bytes of payload buffer plus overhead,
// so the real memory cap is the product of the three configurable knobs — keep this
// purely as a "don't typo a giant number" guard. The Python schema enforces it.
static constexpr size_t SNIFFER_MAX_FRAME_TYPES_UPPER = 64;

// Upper bound for the reference frame_type length. Matches MAX_FRAME_TYPE_LEN in
// rs485_frame.h but duplicated here so this header has no dependency on the hub header.
static constexpr size_t SNIFFER_REFERENCE_MAX_LEN = 8;

// Per-frame-type sliding-window stats for the inter-arrival delay between bus frames.
// Tracks exact min/max across the dump period plus a small ring buffer of recent samples
// for median estimation. The ring buffer trades exact median for a fixed memory footprint;
// a 16-sample window is enough to reflect the cadence of the most recent few seconds at
// typical bus frame rates (10–100 Hz).
struct DelayStats {
  uint32_t min{UINT32_MAX};
  uint32_t max{0};
  // Samples saturate at uint16_t max (~65 s) to halve memory; min/max remain exact via
  // uint32_t, so the saturation only affects the median estimate for very rare frames.
  uint16_t recent[SNIFFER_RECENT_DELAYS]{};
  uint8_t recent_idx{0};
  uint8_t recent_count{0};

  void reset();
  void add(uint32_t delay_ms);
  // Returns 0 when no samples are present; caller is expected to check recent_count first
  // if it needs to distinguish "no samples" from "median is exactly 0 ms".
  uint32_t median() const;
};

// One captured payload sample for the unique-payload table. The byte buffer is allocated
// once at entry-creation time (sized to SnifferStats::payload_capture_bytes_); record()
// then writes into it without further allocation. len = actual bytes captured for this
// payload; count = number of times this distinct payload was observed in the current
// dump period (reset to 0 at each dump, set to 1 on first sighting).
struct PayloadCapture {
  std::unique_ptr<uint8_t[]> bytes;
  uint8_t len{0};
  uint16_t count{0};

  // Allocate the bytes buffer. Called once per slot when the owning SnifferEntry is
  // initialized; later record() calls just memcpy into bytes.get().
  void init(size_t capacity);
};

// Per-frame-type accumulator. Frame types are keyed by the first 2 bytes of the payload
// (matching the existing last_frame_type_ diagnostic convention). A frame_type prefix
// longer than 2 bytes still matches on_frame: triggers correctly; the sniffer just buckets
// it together with any other prefix that happens to share its first 2 bytes.
struct SnifferEntry {
  uint8_t frame_type[2]{};
  uint32_t count{0};
  uint32_t last_seen_ms{0};
  // Set at creation and never changed: find_or_create_() keys entries on (frame_type, is_tx)
  // together, not frame_type alone, so this is a fixed property of the entry, not a sticky
  // guess. Needed because UART_MODE_RS485_HALF_DUPLEX suppresses local echo, so the RX path
  // never sees our own transmissions — that rules out echo-caused collisions, but says
  // nothing about a protocol that genuinely reuses a frame_type numerically for both a
  // request the hub sends and an unrelated reply it receives. Keying on direction too means
  // that case gets two separate rows instead of one row silently mixing both event streams.
  bool is_tx{false};
  DelayStats d_ref;
  DelayStats d_same;
  // Heap-allocated array of PayloadCapture slots, sized to SnifferStats::max_unique_payloads_
  // at SnifferEntry::init(). Slot byte buffers are allocated up front too, so update_unique_payload_
  // never allocates from the hot path after the first sighting of each frame type.
  std::unique_ptr<PayloadCapture[]> payloads;
  uint8_t unique_count{0};
  uint16_t unique_overflow{0};

  // Allocate the payloads array and each slot's byte buffer. Called once per entry from
  // SnifferStats::find_or_create_ when a new frame_type is first observed.
  void init(size_t max_unique_payloads, size_t payload_capture_bytes);

  // Clears per-period counters: total frame count, delay stats, last_seen, AND the unique
  // payload list. Per request from the workflow side: a fresh dump period starts with an
  // empty payload list so the user can press a set of buttons, capture the table, then
  // press a different set in the next period without restarting the ESP. The allocated
  // payload byte buffers are kept; only the bookkeeping (unique_count, per-slot len/count,
  // unique_overflow) resets.
  void reset_period_stats();
};

// Optional diagnostic that records per-frame-type cadence and unique-payload histograms
// while the hub is in sniffer mode. Compiled out unless USE_RS485_FRAME_SNIFFER_STATS is
// defined; the hub holds a unique_ptr that is nullptr in production, so the hot path is
// a single null-check.
//
// One instance is owned by the hub and fed by a single call from process_raw_frame_().
// Output is periodic: tick() is called from loop() and emits the table when interval_ms
// has elapsed, then resets per-period counters.
class SnifferStats {
 public:
  // Called once during to_code wiring. max_entries is bounded by SNIFFER_MAX_FRAME_TYPES_UPPER.
  // max_unique_payloads + payload_capture_bytes are also user-configurable and decide how
  // much payload material the sniffer can distinguish per frame type. reference_frame_type
  // may be empty, in which case the d-ref column is always "-" (useful for protocols with
  // no obvious reference frame). reference_mode_send selects what "the reference" means: false
  // (default, reference_mode: receive) keeps the original behavior — d-ref measures since the
  // last RX frame matching reference_frame_type; true (reference_mode: send) measures d-ref
  // since the last TX event instead, ignoring reference_frame_type.
  void init(size_t max_entries, uint32_t interval_ms, uint8_t payload_dump_top, size_t max_unique_payloads,
            size_t payload_capture_bytes, const std::vector<uint8_t> &reference_frame_type, bool strip_high_bit,
            bool reference_mode_send);

  // Hot path. Called once per validated RX frame with the payload-relative bytes (frame
  // type at payload[0..N-1], data after). Returns immediately if init() was never called.
  void record(const std::vector<uint8_t> &payload, uint32_t now);

  // Called once per transmitted frame from write_frame_(), with the same payload-relative
  // view record() uses (frame_type at payload[0..N-1]) extracted from the built frame before
  // framing/CRC. TX events share the same per-frame-type table as RX; dump_() tags their row
  // with a '>' prefix on the frame-type hex. Returns immediately if init() was never called.
  void record_tx(const std::vector<uint8_t> &payload, uint32_t now);

  // Called from the hub's loop(). Emits the table if interval_ms has elapsed since the
  // last dump, then resets per-period counters.
  void tick(uint32_t now);

 protected:
  // Linear scan; entries_ has at most SNIFFER_MAX_FRAME_TYPES_UPPER (64) entries by
  // construction. Keyed on (frame_type, is_tx) together — see SnifferEntry::is_tx — so a
  // frame_type used both ways gets two independent entries rather than one merged row.
  // Returns nullptr if the table is full and this (frame_type, is_tx) pair has not been seen
  // before — the caller bumps dropped_frame_types_ instead. Lazily allocates the per-entry
  // payload buffers on first sighting.
  SnifferEntry *find_or_create_(const uint8_t *frame_type, bool is_tx);
  bool matches_reference_(const std::vector<uint8_t> &payload) const;
  // Shared bookkeeping between record() and record_tx(): advances the reference clock when
  // is_ref, buckets the frame into its entry, updates d_ref/d_same/unique-payload state.
  // is_ref's meaning depends on reference_mode_send_ — see record()/record_tx() below.
  void record_common_(const std::vector<uint8_t> &payload, uint32_t now, bool is_ref, bool is_tx);
  // Compares the (possibly truncated) payload against the entry's existing unique
  // payloads. Non-static because it needs max_unique_payloads_ and payload_capture_bytes_
  // for bounds and truncation; both are runtime-configurable.
  void update_unique_payload_(SnifferEntry &e, const std::vector<uint8_t> &payload);
  void dump_(uint32_t now);
  void dump_payloads_(size_t top_n, const uint8_t *order) const;

  FixedVector<SnifferEntry> entries_;
  StaticVector<uint8_t, SNIFFER_REFERENCE_MAX_LEN> reference_frame_type_;
  uint32_t last_ref_time_{0};
  bool ref_seen_in_period_{false};
  // false (default) = reference_mode: receive — is_ref is "this RX frame matches
  // reference_frame_type_". true = reference_mode: send — is_ref is "this is a TX event",
  // so d-ref on every other row measures time since our last transmission.
  bool reference_mode_send_{false};
  uint32_t interval_ms_{0};
  uint32_t last_dump_time_{0};
  uint32_t dropped_frame_types_{0};
  uint8_t payload_dump_top_{0};
  bool strip_high_bit_{false};
  // Sized by the YAML schema; carried here so update_unique_payload_, find_or_create_,
  // and dump_payloads_ all reach the same numbers.
  size_t max_unique_payloads_{0};
  size_t payload_capture_bytes_{0};
  // Allocated once at init() to render hex+ASCII previews without per-dump heap traffic.
  // Sized to payload_capture_bytes_ * 3 + 1 and payload_capture_bytes_ + 1 respectively.
  std::unique_ptr<char[]> hex_buf_;
  std::unique_ptr<char[]> ascii_buf_;
  bool initialized_{false};
};

}  // namespace esphome::rs485_frame

#endif  // USE_RS485_FRAME_SNIFFER_STATS
