#include "sniffer_stats.h"

#ifdef USE_RS485_FRAME_SNIFFER_STATS

#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace esphome::rs485_frame {

static const char *const TAG = "rs485_frame.stats";

void DelayStats::reset() {
  this->min = UINT32_MAX;
  this->max = 0;
  this->recent_idx = 0;
  this->recent_count = 0;
}

void DelayStats::add(uint32_t delay_ms) {
  if (delay_ms < this->min)
    this->min = delay_ms;
  if (delay_ms > this->max)
    this->max = delay_ms;
  // Saturate the ring-buffer sample at uint16_t max so very rare frames don't break the
  // median; the exact max is still tracked above.
  this->recent[this->recent_idx] = delay_ms > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(delay_ms);
  this->recent_idx = (this->recent_idx + 1) % SNIFFER_RECENT_DELAYS;
  if (this->recent_count < SNIFFER_RECENT_DELAYS)
    this->recent_count++;
}

uint32_t DelayStats::median() const {
  if (this->recent_count == 0)
    return 0;
  uint16_t copy[SNIFFER_RECENT_DELAYS];
  std::memcpy(copy, this->recent, sizeof(uint16_t) * this->recent_count);
  // Insertion sort: N ≤ 16, cheap and avoids dragging in std::sort template instantiation.
  for (size_t i = 1; i < this->recent_count; i++) {
    uint16_t v = copy[i];
    size_t j = i;
    while (j > 0 && copy[j - 1] > v) {
      copy[j] = copy[j - 1];
      j--;
    }
    copy[j] = v;
  }
  return copy[this->recent_count / 2];
}

void PayloadCapture::init(size_t capacity) { this->bytes = std::make_unique<uint8_t[]>(capacity); }

void SnifferEntry::init(size_t max_unique_payloads, size_t payload_capture_bytes) {
  this->payloads = std::make_unique<PayloadCapture[]>(max_unique_payloads);
  for (size_t i = 0; i < max_unique_payloads; i++) {
    this->payloads[i].init(payload_capture_bytes);
  }
}

void SnifferEntry::reset_period_stats() {
  this->count = 0;
  this->last_seen_ms = 0;
  this->d_ref.reset();
  this->d_same.reset();
  // Wipe the unique-payload bookkeeping so the next period starts fresh. The payload
  // byte buffers stay allocated; per-slot len/count past unique_count are stale but
  // unread until a fresh payload overwrites them in update_unique_payload_.
  this->unique_count = 0;
  this->unique_overflow = 0;
}

void SnifferStats::init(size_t max_entries, uint32_t interval_ms, uint8_t payload_dump_top, size_t max_unique_payloads,
                        size_t payload_capture_bytes, const std::vector<uint8_t> &reference_frame_type,
                        bool strip_high_bit, bool reference_mode_send) {
  size_t capped = max_entries > SNIFFER_MAX_FRAME_TYPES_UPPER ? SNIFFER_MAX_FRAME_TYPES_UPPER : max_entries;
  this->entries_.init(capped);
  this->reference_frame_type_.assign(reference_frame_type.begin(), reference_frame_type.end());
  this->interval_ms_ = interval_ms;
  this->payload_dump_top_ = payload_dump_top;
  this->strip_high_bit_ = strip_high_bit;
  this->reference_mode_send_ = reference_mode_send;
  this->max_unique_payloads_ = max_unique_payloads;
  this->payload_capture_bytes_ = payload_capture_bytes;
  // Pre-allocate the hex/ASCII scratch buffers used by dump_payloads_ so dumps don't
  // allocate per-call. Sized to the worst-case captured payload.
  this->hex_buf_ = std::make_unique<char[]>(payload_capture_bytes * 3 + 1);
  this->ascii_buf_ = std::make_unique<char[]>(payload_capture_bytes + 1);
  this->initialized_ = true;
}

bool SnifferStats::matches_reference_(const std::vector<uint8_t> &payload) const {
  if (this->reference_frame_type_.empty() || payload.size() < this->reference_frame_type_.size())
    return false;
  return std::equal(this->reference_frame_type_.begin(), this->reference_frame_type_.end(), payload.begin());
}

SnifferEntry *SnifferStats::find_or_create_(const uint8_t *frame_type, bool is_tx) {
  // Match on (frame_type, is_tx) together: a frame_type number reused by the protocol for
  // both a request the hub sends and an independently-arriving reply must not merge into one
  // row's count/d_same/payload table (see SnifferEntry::is_tx).
  for (auto &entry : this->entries_) {
    if (entry.frame_type[0] == frame_type[0] && entry.frame_type[1] == frame_type[1] && entry.is_tx == is_tx)
      return &entry;
  }
  if (this->entries_.size() < this->entries_.capacity()) {
    SnifferEntry &e = this->entries_.emplace_back();
    e.frame_type[0] = frame_type[0];
    e.frame_type[1] = frame_type[1];
    e.is_tx = is_tx;
    // Lazy allocation: per-entry payload buffers are sized using the current SnifferStats
    // configuration. This only runs once per distinct (frame_type, is_tx) pair, then never
    // again — the hot record() path after this allocation is pure memcpy/compare.
    e.init(this->max_unique_payloads_, this->payload_capture_bytes_);
    return &e;
  }
  return nullptr;
}

void SnifferStats::update_unique_payload_(SnifferEntry &e, const std::vector<uint8_t> &payload) {
  size_t len = payload.size() < this->payload_capture_bytes_ ? payload.size() : this->payload_capture_bytes_;
  for (uint8_t i = 0; i < e.unique_count; i++) {
    PayloadCapture &slot = e.payloads[i];
    if (slot.len == len && std::memcmp(slot.bytes.get(), payload.data(), len) == 0) {
      // Saturate the per-payload count at uint16_t max — a very chatty payload over a
      // long dump interval can otherwise wrap. The exact count past 65535 doesn't matter
      // for the discovery use case; "≥65535" is information enough.
      if (slot.count < UINT16_MAX)
        slot.count++;
      return;
    }
  }
  if (e.unique_count < this->max_unique_payloads_) {
    PayloadCapture &slot = e.payloads[e.unique_count];
    std::memcpy(slot.bytes.get(), payload.data(), len);
    slot.len = static_cast<uint8_t>(len);
    slot.count = 1;  // first sighting in this period
    e.unique_count++;
  } else if (e.unique_overflow < UINT16_MAX) {
    e.unique_overflow++;
  }
}

void SnifferStats::record(const std::vector<uint8_t> &payload, uint32_t now) {
  if (!this->initialized_ || payload.size() < 2)
    return;
  // reference_mode: receive (default) — is_ref is "this RX frame matches
  // reference_frame_type_"; in reference_mode: send, RX frames never advance the reference
  // clock (only TX events do, in record_tx() below), so is_ref is always false here.
  bool is_ref = !this->reference_mode_send_ && this->matches_reference_(payload);
  this->record_common_(payload, now, is_ref, /*is_tx=*/false);
}

void SnifferStats::record_tx(const std::vector<uint8_t> &payload, uint32_t now) {
  if (!this->initialized_ || payload.size() < 2)
    return;
  // reference_mode: send — every TX event advances the reference clock, so it is always its
  // own is_ref. reference_mode: receive leaves the reference clock to RX frames only.
  bool is_ref = this->reference_mode_send_;
  this->record_common_(payload, now, is_ref, /*is_tx=*/true);
}

void SnifferStats::record_common_(const std::vector<uint8_t> &payload, uint32_t now, bool is_ref, bool is_tx) {
  // Update the reference-frame timestamp *before* computing since-ref for this frame so
  // that the reference frame itself shows up with no since-ref sample (its own d-ref row
  // is always "-"), and the next non-reference frame measures from this one.
  if (is_ref) {
    this->last_ref_time_ = now;
    this->ref_seen_in_period_ = true;
  }

  SnifferEntry *e = this->find_or_create_(payload.data(), is_tx);
  if (e == nullptr) {
    if (this->dropped_frame_types_ < UINT32_MAX)
      this->dropped_frame_types_++;
    return;
  }

  // since-same-type: only after we've seen this frame type at least once in this period.
  if (e->count > 0) {
    // Unsigned subtraction wraps correctly for the 49-day millis rollover.
    e->d_same.add(now - e->last_seen_ms);
  }

  // since-ref: skip when this event is the reference itself (the d-ref column would be
  // always zero and is uninformative for the reference row).
  if (this->ref_seen_in_period_ && !is_ref) {
    e->d_ref.add(now - this->last_ref_time_);
  }

  this->update_unique_payload_(*e, payload);

  e->count++;
  e->last_seen_ms = now;
}

void SnifferStats::tick(uint32_t now) {
  if (!this->initialized_ || this->interval_ms_ == 0)
    return;
  if (this->last_dump_time_ == 0) {
    // First tick after init — establish baseline so the first dump fires ~interval_ms
    // after sniffer start rather than immediately.
    this->last_dump_time_ = now;
    return;
  }
  if (now - this->last_dump_time_ < this->interval_ms_)
    return;
  this->dump_(now);
  this->last_dump_time_ = now;
}

void SnifferStats::dump_(uint32_t now) {
  // Indirection-sort by count descending. order[] holds entry indices; entry data is not
  // moved. Insertion sort keeps the cost bounded for our small N (≤ 64).
  uint8_t order[SNIFFER_MAX_FRAME_TYPES_UPPER];
  size_t n = this->entries_.size();
  for (size_t i = 0; i < n; i++)
    order[i] = static_cast<uint8_t>(i);
  for (size_t i = 1; i < n; i++) {
    uint8_t v = order[i];
    uint32_t key = this->entries_[v].count;
    size_t j = i;
    while (j > 0 && this->entries_[order[j - 1]].count < key) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = v;
  }

  ESP_LOGI(TAG, "RS485 sniffer stats over %" PRIu32 " ms (sorted by count):", now - this->last_dump_time_);
  // Header and data row widths are matched by hand: type(4) + sep(1) + cnt(5) + sep(3)
  // + d_ref triplet(17 = 3*5 + 2 separators) + sep(3) + d_same triplet(17) + sep(3) + payloads.
  // The label "(min/med/max)" is wider than the data triplet; the labels bleed past the
  // column right edge, which is harmless because nothing follows them on the header line.
  ESP_LOGI(TAG, "  type  cnt    d-ref(min/med/max)    d-same(min/med/max)   payloads");

  char d_ref_buf[24];
  char d_same_buf[24];
  char unique_buf[32];

  for (size_t i = 0; i < n; i++) {
    SnifferEntry &e = this->entries_[order[i]];
    // A frame type seen in an earlier dump period but not this one still has a stale entry
    // (reset_period_stats() zeroes count but the entry itself persists). Skip it rather than
    // print an all-dashes row that adds no information about the current period.
    if (e.count == 0)
      continue;

    if (e.d_ref.recent_count == 0) {
      std::snprintf(d_ref_buf, sizeof(d_ref_buf), "%5s %5s %5s", "-", "-", "-");
    } else {
      std::snprintf(d_ref_buf, sizeof(d_ref_buf), "%5" PRIu32 " %5" PRIu32 " %5" PRIu32, e.d_ref.min, e.d_ref.median(),
                    e.d_ref.max);
    }
    if (e.d_same.recent_count == 0) {
      std::snprintf(d_same_buf, sizeof(d_same_buf), "%5s %5s %5s", "-", "-", "-");
    } else {
      std::snprintf(d_same_buf, sizeof(d_same_buf), "%5" PRIu32 " %5" PRIu32 " %5" PRIu32, e.d_same.min,
                    e.d_same.median(), e.d_same.max);
    }
    if (e.unique_overflow > 0) {
      std::snprintf(unique_buf, sizeof(unique_buf), "%u unique +%u", e.unique_count, e.unique_overflow);
    } else {
      std::snprintf(unique_buf, sizeof(unique_buf), "%u unique", e.unique_count);
    }

    // '>' marks a TX row (we sent this frame_type) so the table reads directionally without
    // a separate column; unmarked rows are RX as before. Entries are keyed on
    // (frame_type, is_tx) together, so a frame_type reused in both directions gets two rows
    // rather than one mixed row -- see SnifferEntry::is_tx.
    ESP_LOGI(TAG, " %c%02X%02X %5" PRIu32 "    %s    %s   %s", e.is_tx ? '>' : ' ', e.frame_type[0], e.frame_type[1],
             e.count, d_ref_buf, d_same_buf, unique_buf);
  }

  if (this->payload_dump_top_ > 0 && n > 0) {
    size_t dump_n = this->payload_dump_top_ < n ? this->payload_dump_top_ : n;
    this->dump_payloads_(dump_n, order);
  }

  if (this->dropped_frame_types_ > 0) {
    ESP_LOGW(TAG, "  dropped %" PRIu32 " events for frame types past table capacity (%zu)", this->dropped_frame_types_,
             this->entries_.capacity());
  }

  // Per-period reset: total frame count, delay stats, AND unique payload list. The
  // payload list is intentionally cleared every period so the user can use successive
  // dumps as independent capture windows (press buttons A, dump, press buttons B, dump)
  // without having to reboot the ESP between sessions.
  for (size_t i = 0; i < n; i++)
    this->entries_[i].reset_period_stats();
  this->dropped_frame_types_ = 0;
  this->ref_seen_in_period_ = false;
}

void SnifferStats::dump_payloads_(size_t top_n, const uint8_t *order) const {
  // Hex/ASCII view of every captured unique payload for the top-N frame types by count.
  // When ascii_strip_high_bit is set, bit 7 is masked before the printable-range gate so
  // displays that pack an attribute flag (blink/inverse) into the high bit render as their
  // underlying character. Off by default to avoid collapsing distinct 8-bit values on binary
  // buses. Non-printable bytes are rendered as '.'. Buffers are preallocated on SnifferStats
  // so the dump path has no heap traffic.
  for (size_t i = 0; i < top_n; i++) {
    const SnifferEntry &e = this->entries_[order[i]];
    if (e.unique_count == 0)
      continue;
    ESP_LOGI(TAG, "  %02X%02X payloads:", e.frame_type[0], e.frame_type[1]);
    for (uint8_t k = 0; k < e.unique_count; k++) {
      const PayloadCapture &p = e.payloads[k];
      for (uint8_t b = 0; b < p.len; b++)
        std::snprintf(this->hex_buf_.get() + b * 3, 4, "%02X ", p.bytes[b]);
      this->hex_buf_[p.len * 3] = '\0';
      for (uint8_t b = 0; b < p.len; b++) {
        uint8_t c = this->strip_high_bit_ ? (p.bytes[b] & 0x7F) : p.bytes[b];
        this->ascii_buf_[b] = (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '.';
      }
      this->ascii_buf_[p.len] = '\0';
      ESP_LOGI(TAG, "    %5u @ %s |%s|", p.count, this->hex_buf_.get(), this->ascii_buf_.get());
    }
  }
}

}  // namespace esphome::rs485_frame

#endif  // USE_RS485_FRAME_SNIFFER_STATS
