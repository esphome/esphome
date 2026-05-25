#include "discovery.h"

#ifdef USE_RS485_FRAME_DISCOVERY

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::rs485_frame {

static const char *const TAG = "rs485_frame.discovery";

// A candidate frame must have at least this many bursts behind it before the report stops
// saying "collecting" and prints candidates.
static const uint32_t MIN_BURSTS_TO_REPORT = 5;
// Interior-DLE observations required before classifying the escape scheme.
static const uint32_t MIN_DLE_SUCC = 4;
// Cross-frame agreement required to trust a CRC scheme, by CRC width. A 1-byte checksum
// matches a wrong scheme 1/256 of the time per frame, so it needs many more samples than a
// 2-byte CRC before a consistent match is meaningful.
static const uint32_t MIN_CRC_SAMPLES_W1 = 20;
static const uint32_t MIN_CRC_SAMPLES_W2 = 8;
// Cadence of the "collecting traffic" heartbeat printed before the first full report.
static const uint32_t STATUS_HEARTBEAT_MS = 5000;

enum CrcAlgo : uint8_t { ALGO_SUM8, ALGO_XOR8, ALGO_SUM16, ALGO_MODBUS };

struct CrcHypDef {
  CrcAlgo algo;
  uint8_t width;
  bool header;      // true = CRC covers DLE+STX header, false = payload only
  bool big_endian;  // only meaningful for width == 2
};

// Order must match RS485FrameDiscovery::NUM_CRC_HYPS.
static const CrcHypDef CRC_HYPS[] = {
    {ALGO_SUM8, 1, true, false},   {ALGO_SUM8, 1, false, false},  {ALGO_XOR8, 1, true, false},
    {ALGO_XOR8, 1, false, false},  {ALGO_SUM16, 2, true, true},   {ALGO_SUM16, 2, true, false},
    {ALGO_SUM16, 2, false, true},  {ALGO_SUM16, 2, false, false}, {ALGO_MODBUS, 2, true, true},
    {ALGO_MODBUS, 2, true, false}, {ALGO_MODBUS, 2, false, true}, {ALGO_MODBUS, 2, false, false},
};

// big_endian is only used for ALGO_SUM16 — the endianness is embedded in the name so the
// caller does not need to add a separate "big-endian"/"little-endian" descriptor for it.
static const char *algo_name(CrcAlgo a, bool big_endian = true) {
  switch (a) {
    case ALGO_SUM8:
      return "sum8";
    case ALGO_XOR8:
      return "xor8";
    case ALGO_SUM16:
      return big_endian ? "sum16_big_endian" : "sum16_little_endian";
    case ALGO_MODBUS:
      return "crc16_modbus";
    default:
      return "?";
  }
}

static uint32_t disc_sum(const uint8_t *d, size_t n, bool hdr, uint8_t dle, uint8_t stx) {
  uint32_t s = hdr ? static_cast<uint32_t>(dle) + stx : 0;
  for (size_t i = 0; i < n; i++)
    s += d[i];
  return s;
}

static uint8_t disc_xor8(const uint8_t *d, size_t n, bool hdr, uint8_t dle, uint8_t stx) {
  uint8_t x = hdr ? static_cast<uint8_t>(dle ^ stx) : 0;
  for (size_t i = 0; i < n; i++)
    x ^= d[i];
  return x;
}

static uint16_t disc_modbus(const uint8_t *d, size_t n, bool hdr, uint8_t dle, uint8_t stx) {
  uint16_t c = 0xFFFF;
  auto process = [&c](uint8_t b) {
    c ^= b;
    for (int k = 0; k < 8; k++)
      c = (c & 0x0001) ? static_cast<uint16_t>((c >> 1) ^ 0xA001) : static_cast<uint16_t>(c >> 1);
  };
  if (hdr) {
    process(dle);
    process(stx);
  }
  for (size_t i = 0; i < n; i++)
    process(d[i]);
  return c;
}

void RS485FrameDiscovery::setup() {
  this->burst_.reserve(this->max_burst_);
  this->unescaped_.reserve(this->max_burst_);
  this->raw_inner_.reserve(this->max_burst_);
}

void RS485FrameDiscovery::feed_byte(uint8_t b, uint32_t now) {
  this->last_byte_time_ = now;
  this->burst_open_ = true;
  if (this->burst_.size() >= this->max_burst_) {
    // Overflow: likely two frames merged across an undetected gap, or not a framed bus. Stop
    // accumulating and let the idle-gap close the (truncated) burst rather than splitting at
    // an arbitrary offset.
    this->burst_truncated_ = true;
    return;
  }
  this->burst_.push_back(b);
}

void RS485FrameDiscovery::tick(uint32_t now) {
  if (!this->report_primed_) {
    this->last_report_time_ = now;
    this->last_status_time_ = now;
    this->report_primed_ = true;
  }
  if (this->burst_open_ && now - this->last_byte_time_ >= this->idle_gap_ms_)
    this->close_burst_(now);
  // While the sweep is running the analyzer state belongs to the current candidate, so the
  // normal periodic report is suppressed; the sweep prints its own per-candidate lines.
  if (this->sweeping_) {
    this->sweep_tick_(now);
    return;
  }
  if (now - this->last_report_time_ >= this->report_interval_ms_) {
    this->report_();
    this->first_report_done_ = true;
    this->last_report_time_ = now;
    this->last_status_time_ = now;
    return;
  }
  // Until the first full report fires, emit a lightweight heartbeat every STATUS_HEARTBEAT_MS so
  // the log window is not silent for a whole interval after boot. Skipped when the interval is
  // already short enough that the first report arrives promptly.
  if (!this->first_report_done_ && this->report_interval_ms_ > STATUS_HEARTBEAT_MS &&
      now - this->last_status_time_ >= STATUS_HEARTBEAT_MS) {
    this->last_status_time_ = now;
    ESP_LOGI(TAG, "RS485 discovery: collecting traffic (%" PRIu32 " bursts, %" PRIu32 " frames so far)",
             this->total_bursts_, this->total_frames_);
  }
}

void RS485FrameDiscovery::close_burst_(uint32_t /*now*/) {
  if (this->burst_.size() >= 2 && !this->burst_truncated_)
    this->analyze_burst_();
  this->burst_.clear();
  this->burst_open_ = false;
  this->burst_truncated_ = false;
}

void RS485FrameDiscovery::bump_pair(BytePair *table, size_t &len, uint8_t a, uint8_t b) {
  for (size_t i = 0; i < len; i++) {
    if (table[i].a == a && table[i].b == b) {
      table[i].count++;
      return;
    }
  }
  if (len < PAIR_TABLE_SIZE) {
    table[len++] = {a, b, 1};
    return;
  }
  // Table full: replace the current minimum (heavy-hitters approximation). A genuinely common
  // pair quickly re-accumulates; transient noise pairs churn through the low-count slots.
  size_t min_i = 0;
  for (size_t i = 1; i < len; i++) {
    if (table[i].count < table[min_i].count)
      min_i = i;
  }
  table[min_i] = {a, b, 1};
}

const RS485FrameDiscovery::BytePair *RS485FrameDiscovery::top_pair(const BytePair *table, size_t len) {
  if (len == 0)
    return nullptr;
  const BytePair *top = &table[0];
  for (size_t i = 1; i < len; i++) {
    if (table[i].count > top->count)
      top = &table[i];
  }
  return top;
}

void RS485FrameDiscovery::reset_scoring_() {
  for (size_t v = 0; v < NUM_ESCAPE_VIEWS; v++) {
    for (size_t h = 0; h < NUM_CRC_HYPS; h++) {
      this->crc_samples_[v][h] = 0;
      this->crc_matches_[v][h] = 0;
    }
  }
}

void RS485FrameDiscovery::score_crc_(const std::vector<uint8_t> &content, bool unescaped_view) {
  const size_t view = unescaped_view ? 0 : 1;
  const size_t n = content.size();
  const uint8_t *d = content.data();
  for (size_t h = 0; h < NUM_CRC_HYPS; h++) {
    const CrcHypDef &hyp = CRC_HYPS[h];
    // Need at least a 2-byte frame_type as payload plus the CRC width.
    if (n < static_cast<size_t>(hyp.width) + 2)
      continue;
    const size_t payload_len = n - hyp.width;
    uint32_t received = 0;
    if (hyp.width == 1) {
      received = d[payload_len];
    } else if (hyp.big_endian) {
      received = (static_cast<uint32_t>(d[payload_len]) << 8) | d[payload_len + 1];
    } else {
      received = d[payload_len] | (static_cast<uint32_t>(d[payload_len + 1]) << 8);
    }
    uint32_t computed;
    switch (hyp.algo) {
      case ALGO_SUM8:
        computed = disc_sum(d, payload_len, hyp.header, this->scored_dle_, this->scored_stx_) & 0xFF;
        break;
      case ALGO_XOR8:
        computed = disc_xor8(d, payload_len, hyp.header, this->scored_dle_, this->scored_stx_);
        break;
      case ALGO_SUM16:
        computed = disc_sum(d, payload_len, hyp.header, this->scored_dle_, this->scored_stx_) & 0xFFFF;
        break;
      case ALGO_MODBUS:
      default:
        computed = disc_modbus(d, payload_len, hyp.header, this->scored_dle_, this->scored_stx_);
        break;
    }
    this->crc_samples_[view][h]++;
    if (computed == received)
      this->crc_matches_[view][h]++;
  }
}

void RS485FrameDiscovery::analyze_burst_() {
  this->total_bursts_++;
  const std::vector<uint8_t> &b = this->burst_;
  const size_t len = b.size();
  if (len < 4)
    return;

  // Framing-byte candidates from the burst's first and last byte pairs. Even a burst that holds
  // several back-to-back frames opens with the first frame's DLE+STX and closes with the last
  // frame's DLE+ETX, so these pairs still vote for the right delimiters.
  this->framing_bursts_++;
  bump_pair(this->start_pairs_, this->start_pairs_len_, b[0], b[1]);
  bump_pair(this->end_pairs_, this->end_pairs_len_, b[len - 2], b[len - 1]);

  const BytePair *top_start = top_pair(this->start_pairs_, this->start_pairs_len_);
  const BytePair *top_end = top_pair(this->end_pairs_, this->end_pairs_len_);
  if (top_start == nullptr || top_end == nullptr)
    return;

  const uint8_t dle = top_start->a;
  const uint8_t stx = top_start->b;
  const uint8_t etx = top_end->b;

  // When the framing candidate shifts (typically only in the first few bursts), discard the
  // escape histogram and CRC counters so they reflect a single consistent hypothesis.
  if (!this->scored_valid_ || dle != this->scored_dle_ || stx != this->scored_stx_ || etx != this->scored_etx_) {
    this->reset_scoring_();
    for (uint32_t &slot : this->dle_succ_hist_)
      slot = 0;
    this->dle_succ_total_ = 0;
    this->scored_dle_ = dle;
    this->scored_stx_ = stx;
    this->scored_etx_ = etx;
    this->scored_valid_ = true;
    this->scored_has_marker_ = false;
    this->scored_marker_ = 0;
  }

  // Split the burst into individual frames. A burst can hold several frames when they arrive
  // back-to-back faster than idle_gap, so escape/CRC analysis must run per frame, not per burst:
  // treating a multi-frame burst as one frame both pollutes the escape histogram with the
  // DLE+STX / DLE+ETX bytes at internal frame boundaries and makes every CRC check span two
  // frames (so none match). Inside a frame a DLE not followed by ETX is an escape and consumes
  // the next byte, so an embedded DLE+marker never falsely terminates the frame (the only
  // requirement is marker != ETX, which always holds in practice).
  size_t i = 0;
  while (i + 1 < len) {
    if (b[i] != dle || b[i + 1] != stx) {
      i++;
      continue;
    }
    const size_t inner_start = i + 2;
    size_t j = inner_start;
    size_t frame_end = SIZE_MAX;  // index of the closing DLE, once found
    while (j + 1 < len) {
      if (b[j] == dle) {
        if (b[j + 1] == etx) {
          frame_end = j;
          break;
        }
        j += 2;  // escape: skip the marker/literal byte
      } else {
        j++;
      }
    }
    if (frame_end == SIZE_MAX)
      break;  // trailing partial frame with no terminator; stop scanning this burst
    this->analyze_frame_(b, inner_start, frame_end, dle);
    i = frame_end + 2;  // resume past the closing DLE+ETX
  }
}

void RS485FrameDiscovery::analyze_frame_(const std::vector<uint8_t> &b, size_t inner_start, size_t frame_end,
                                         uint8_t dle) {
  this->total_frames_++;

  // Escape histogram: within a single frame's interior every DLE is an escape, so the following
  // byte is the escape marker. Advance by two on a hit so a doubled DLE (double mode) is counted
  // once, not twice.
  for (size_t i = inner_start; i + 1 < frame_end;) {
    if (b[i] == dle) {
      this->dle_succ_hist_[b[i + 1]]++;
      this->dle_succ_total_++;
      i += 2;
    } else {
      i++;
    }
  }

  // (Re)classify the escape marker once enough interior DLEs have been seen.
  if (this->dle_succ_total_ >= MIN_DLE_SUCC) {
    uint16_t arg = 0;
    for (uint16_t v = 1; v < 256; v++) {
      if (this->dle_succ_hist_[v] > this->dle_succ_hist_[arg])
        arg = v;
    }
    const uint8_t marker = static_cast<uint8_t>(arg);
    if (!this->scored_has_marker_ || marker != this->scored_marker_) {
      // The escape marker drives unescaping, so a change invalidates the CRC counters.
      this->reset_scoring_();
      this->scored_has_marker_ = true;
      this->scored_marker_ = marker;
    }
  }

  if (frame_end <= inner_start)
    return;

  // raw_inner_ = the on-wire bytes strictly between the opening STX and the closing DLE.
  this->raw_inner_.assign(b.begin() + inner_start, b.begin() + frame_end);

  // unescaped_ = raw_inner_ with DLE byte-stuffing removed, using the detected marker. With no
  // confirmed marker yet, unescaping is the identity (and equals the raw view).
  this->unescaped_.clear();
  if (this->scored_has_marker_) {
    for (size_t i = 0; i < this->raw_inner_.size(); i++) {
      if (this->raw_inner_[i] == dle && i + 1 < this->raw_inner_.size() &&
          this->raw_inner_[i + 1] == this->scored_marker_) {
        this->unescaped_.push_back(dle);
        i++;
      } else {
        this->unescaped_.push_back(this->raw_inner_[i]);
      }
    }
  } else {
    this->unescaped_ = this->raw_inner_;
  }

  this->score_crc_(this->unescaped_, true);
  this->score_crc_(this->raw_inner_, false);
}

void RS485FrameDiscovery::report_() {
  ESP_LOGI(TAG, "RS485 discovery (cumulative since boot): %" PRIu32 " bursts, %" PRIu32 " frames extracted",
           this->total_bursts_, this->total_frames_);

  if (this->total_bursts_ < MIN_BURSTS_TO_REPORT) {
    ESP_LOGI(TAG, "  Collecting traffic - need at least %" PRIu32 " bursts before suggesting candidates",
             MIN_BURSTS_TO_REPORT);
    return;
  }

  const BytePair *top_start = top_pair(this->start_pairs_, this->start_pairs_len_);
  const BytePair *top_end = top_pair(this->end_pairs_, this->end_pairs_len_);
  if (top_start == nullptr || top_end == nullptr)
    return;

  const bool dle_agrees = top_start->a == top_end->a;
  // Framing confidence = the share of voting bursts whose opening/closing pair is the top
  // candidate, taken as the weaker of the two. A wrong or noisy bus splits its votes across
  // many pairs, so the top pair holds only a small share; a real DLE bus is near 100%.
  const uint32_t denom = this->framing_bursts_ > 0 ? this->framing_bursts_ : 1;
  const uint32_t confidence = this->compute_confidence_(top_start, top_end);
  const bool confident = dle_agrees && confidence >= this->min_framing_confidence_;
  if (dle_agrees) {
    ESP_LOGI(TAG,
             "  Framing: DLE=0x%02x STX=0x%02x ETX=0x%02x  (confidence %" PRIu32 "%%, start pair x%" PRIu32
             ", end pair x%" PRIu32 " of %" PRIu32 " bursts)",
             top_start->a, top_start->b, top_end->b, confidence, top_start->count, top_end->count, denom);
    if (!confident) {
      ESP_LOGI(TAG,
               "  Framing confidence %" PRIu32 "%% is below the %u%% threshold - delimiters not yet trusted. "
               "Capture more traffic, or lower discovery.min_framing_confidence if this bus is genuinely noisy.",
               confidence, this->min_framing_confidence_);
    }
  } else {
    ESP_LOGI(TAG, "  Framing AMBIGUOUS: start pair 0x%02x 0x%02x (x%" PRIu32 "), end pair 0x%02x 0x%02x (x%" PRIu32 ")",
             top_start->a, top_start->b, top_start->count, top_end->a, top_end->b, top_end->count);
    ESP_LOGI(TAG, "  The opening and closing delimiter byte disagree - this bus may not be DLE-framed.");
  }

  // Escape scheme.
  bool escape_double = false;
  uint8_t escape_marker = 0;
  bool escape_known = false;
  if (this->dle_succ_total_ >= MIN_DLE_SUCC) {
    uint16_t arg = 0;
    for (uint16_t v = 1; v < 256; v++) {
      if (this->dle_succ_hist_[v] > this->dle_succ_hist_[arg])
        arg = v;
    }
    escape_marker = static_cast<uint8_t>(arg);
    escape_known = true;
    const uint32_t pct = this->dle_succ_hist_[arg] * 100 / this->dle_succ_total_;
    if (escape_marker == top_start->a) {
      escape_double = true;
      ESP_LOGI(TAG, "  Escape: double (DLE DLE) - %" PRIu32 "%% of %" PRIu32 " in-frame DLEs", pct,
               this->dle_succ_total_);
    } else {
      ESP_LOGI(TAG, "  Escape: escape_byte 0x%02x - %" PRIu32 "%% of %" PRIu32 " in-frame DLEs", escape_marker, pct,
               this->dle_succ_total_);
    }
  } else {
    ESP_LOGI(TAG, "  Escape: unconfirmed - no in-frame DLE observed in %" PRIu32 " bursts. Capture longer / busier",
             this->total_bursts_);
  }

  // CRC survivors.
  bool any_crc = false;
  const char *view_name[NUM_ESCAPE_VIEWS] = {"unescaped", "raw wire bytes"};
  for (size_t v = 0; v < NUM_ESCAPE_VIEWS; v++) {
    for (size_t h = 0; h < NUM_CRC_HYPS; h++) {
      const CrcHypDef &hyp = CRC_HYPS[h];
      const uint32_t need = hyp.width == 1 ? MIN_CRC_SAMPLES_W1 : MIN_CRC_SAMPLES_W2;
      const uint32_t samples = this->crc_samples_[v][h];
      if (samples >= need && this->crc_matches_[v][h] == samples) {
        any_crc = true;
        const char *cover = hyp.header ? "header_inclusive" : "payload_only";
        if (hyp.width == 2 && hyp.algo != ALGO_SUM16) {
          // For non-sum16 2-byte CRCs (currently only ALGO_MODBUS), show endianness separately.
          ESP_LOGI(TAG, "  CRC match: %s %s %s (%s) - %" PRIu32 "/%" PRIu32 " frames", algo_name(hyp.algo), cover,
                   hyp.big_endian ? "big-endian" : "little-endian", view_name[v], this->crc_matches_[v][h], samples);
        } else if (hyp.width == 2) {
          // ALGO_SUM16: endianness is encoded in the name (sum16_big_endian / sum16_little_endian).
          ESP_LOGI(TAG, "  CRC match: %s %s (%s) - %" PRIu32 "/%" PRIu32 " frames", algo_name(hyp.algo, hyp.big_endian),
                   cover, view_name[v], this->crc_matches_[v][h], samples);
        } else {
          ESP_LOGI(TAG, "  CRC match: %s %s (%s) - %" PRIu32 "/%" PRIu32 " frames", algo_name(hyp.algo), cover,
                   view_name[v], this->crc_matches_[v][h], samples);
        }
      }
    }
  }
  if (!any_crc) {
    ESP_LOGI(TAG, "  CRC: no scheme matched consistently yet - try crc: {type: none}, or the bus uses an "
                  "unsupported check");
  }

  // If a baud/data-bits sweep ran, surface the locked UART settings so the user can copy them.
  // Parity and stop bits are not detectable passively (see the component docs).
  if (this->sweep_done_) {
    ESP_LOGI(TAG, "  Suggested uart config (from sweep; parity/stop_bits not passively detectable):");
    ESP_LOGI(TAG, "    uart:");
    ESP_LOGI(TAG, "      baud_rate: %" PRIu32, this->uart_->get_baud_rate());
    ESP_LOGI(TAG, "      data_bits: %u", this->uart_->get_data_bits());
  }

  // Ready-to-paste config suggestion (only when the framing delimiters are coherent and the
  // top candidate clears the confidence threshold).
  if (confident) {
    ESP_LOGI(TAG, "  Suggested framing/escape config:");
    ESP_LOGI(TAG, "    framing:");
    ESP_LOGI(TAG, "      dle: 0x%02x", top_start->a);
    ESP_LOGI(TAG, "      stx: 0x%02x", top_start->b);
    ESP_LOGI(TAG, "      etx: 0x%02x", top_end->b);
    if (!escape_known) {
      ESP_LOGI(TAG, "      # escape: unconfirmed - no DLE seen inside a payload yet; capture more traffic");
    } else if (escape_double) {
      ESP_LOGI(TAG, "      escape: {mode: double}");
    } else {
      ESP_LOGI(TAG, "      escape: {mode: escape_byte, byte: 0x%02x}", escape_marker);
    }
  }
}

uint32_t RS485FrameDiscovery::compute_confidence_(const BytePair *top_start, const BytePair *top_end) const {
  const uint32_t denom = this->framing_bursts_ > 0 ? this->framing_bursts_ : 1;
  const uint32_t start_pct = top_start->count * 100 / denom;
  const uint32_t end_pct = top_end->count * 100 / denom;
  return start_pct < end_pct ? start_pct : end_pct;
}

bool RS485FrameDiscovery::any_crc_match_() const {
  for (size_t v = 0; v < NUM_ESCAPE_VIEWS; v++) {
    for (size_t h = 0; h < NUM_CRC_HYPS; h++) {
      const uint32_t need = CRC_HYPS[h].width == 1 ? MIN_CRC_SAMPLES_W1 : MIN_CRC_SAMPLES_W2;
      const uint32_t samples = this->crc_samples_[v][h];
      if (samples >= need && this->crc_matches_[v][h] == samples)
        return true;
    }
  }
  return false;
}

void RS485FrameDiscovery::reset_analyzer_() {
  this->burst_.clear();
  this->burst_open_ = false;
  this->burst_truncated_ = false;
  this->start_pairs_len_ = 0;
  this->end_pairs_len_ = 0;
  this->total_bursts_ = 0;
  this->framing_bursts_ = 0;
  this->total_frames_ = 0;
  for (uint32_t &slot : this->dle_succ_hist_)
    slot = 0;
  this->dle_succ_total_ = 0;
  this->scored_valid_ = false;
  this->scored_has_marker_ = false;
  this->scored_marker_ = 0;
  this->reset_scoring_();
}

void RS485FrameDiscovery::set_baud_sweep(uart::UARTComponent *uart, const std::vector<uint32_t> &bauds,
                                         const std::vector<uint8_t> &data_bits, uint32_t dwell_ms) {
  this->uart_ = uart;
  this->sweep_bauds_ = bauds;
  this->sweep_data_bits_ = data_bits;
  this->sweep_dwell_ms_ = dwell_ms;
  // A sweep needs a UART to reconfigure and at least one baud and one data-bit width to try.
  // Runtime UART reconfiguration (load_settings) only exists on ESP-IDF and ESP8266; on other
  // platforms the sweep cannot change the line settings, so it is disabled rather than silently
  // scoring every candidate against the unchanged hardware baud.
#if defined(USE_ESP8266) || defined(USE_ESP32)
  this->sweeping_ = uart != nullptr && !bauds.empty() && !data_bits.empty();
  if (this->sweeping_)
    this->sweep_results_.resize(this->sweep_total_());
#else
  this->sweeping_ = false;
  if (uart != nullptr && !bauds.empty())
    ESP_LOGW(TAG, "discovery baud_sweep is not supported on this platform (no runtime UART reconfiguration); ignoring");
#endif
}

void RS485FrameDiscovery::apply_sweep_candidate_(uint32_t now) {
  const uint32_t baud = this->sweep_baud_at_(this->sweep_idx_);
  const uint8_t data_bits = this->sweep_data_bits_at_(this->sweep_idx_);
  if (this->uart_ != nullptr) {
    this->uart_->set_baud_rate(baud);
    this->uart_->set_data_bits(data_bits);
    // Tears down and reinstalls the UART driver with the new line settings, flushing any bytes
    // received under the previous (wrong) candidate. The sweep is only enabled on platforms that
    // provide load_settings (see set_baud_sweep), so this is always reached on a real sweep.
#if defined(USE_ESP8266) || defined(USE_ESP32)
    this->uart_->load_settings(false);
#endif
  }
  this->reset_analyzer_();
  this->sweep_phase_start_ = now;
  ESP_LOGI(TAG, "Baud sweep: trying %" PRIu32 " baud, %u data bits (candidate %zu/%zu) for %" PRIu32 " ms", baud,
           data_bits, this->sweep_idx_ + 1, this->sweep_total_(), this->sweep_dwell_ms_);
}

void RS485FrameDiscovery::record_sweep_result_() {
  const BytePair *top_start = top_pair(this->start_pairs_, this->start_pairs_len_);
  const BytePair *top_end = top_pair(this->end_pairs_, this->end_pairs_len_);
  SweepResult r{};
  r.baud = this->sweep_baud_at_(this->sweep_idx_);
  r.data_bits = this->sweep_data_bits_at_(this->sweep_idx_);
  r.frames = this->total_frames_;
  if (top_start != nullptr && top_end != nullptr) {
    r.dle_agrees = top_start->a == top_end->a;
    r.confidence = this->compute_confidence_(top_start, top_end);
  }
  r.crc_matched = this->any_crc_match_();
  this->sweep_results_[this->sweep_idx_] = r;
  ESP_LOGI(TAG,
           "Baud sweep result: %" PRIu32 " baud %u data bits -> framing confidence %" PRIu32 "%%, %s, %" PRIu32
           " frames",
           r.baud, r.data_bits, r.confidence, r.crc_matched ? "CRC matched" : "no CRC match", r.frames);
}

void RS485FrameDiscovery::finish_sweep_(uint32_t now) {
  // Rank: a candidate with a consistent CRC beats one without; among equals, higher framing
  // confidence wins. CRC agreement is the strongest signal the line settings are right, because
  // a wrong baud or data-bit width corrupts the bytes so no checksum can match across frames.
  size_t best = 0;
  for (size_t i = 1; i < this->sweep_results_.size(); i++) {
    const SweepResult &a = this->sweep_results_[i];
    const SweepResult &b = this->sweep_results_[best];
    const bool better = a.crc_matched != b.crc_matched ? a.crc_matched : a.confidence > b.confidence;
    if (better)
      best = i;
  }
  const SweepResult &win = this->sweep_results_[best];
  if (!win.crc_matched && win.confidence < this->min_framing_confidence_) {
    ESP_LOGW(TAG,
             "Baud sweep: no candidate produced coherent DLE framing (best %" PRIu32 " baud %u data bits at %" PRIu32
             "%% confidence). The bus may not be DLE-framed, or its real baud/data bits are outside the swept list.",
             win.baud, win.data_bits, win.confidence);
  }
  if (this->uart_ != nullptr) {
    this->uart_->set_baud_rate(win.baud);
    this->uart_->set_data_bits(win.data_bits);
#if defined(USE_ESP8266) || defined(USE_ESP32)
    this->uart_->load_settings(false);
#endif
  }
  ESP_LOGI(TAG,
           "Baud sweep complete: locked to %" PRIu32 " baud, %u data bits (framing confidence %" PRIu32
           "%%, %s). Continuing discovery at these settings.",
           win.baud, win.data_bits, win.confidence, win.crc_matched ? "CRC matched" : "no CRC match");
  this->reset_analyzer_();
  this->sweeping_ = false;
  this->sweep_done_ = true;
  this->last_report_time_ = now;
  this->last_status_time_ = now;
}

void RS485FrameDiscovery::sweep_tick_(uint32_t now) {
  if (!this->sweep_started_) {
    ESP_LOGI(TAG, "Starting baud/data-bits sweep: %zu candidate(s), %" PRIu32 " ms each", this->sweep_total_(),
             this->sweep_dwell_ms_);
    this->sweep_idx_ = 0;
    this->sweep_started_ = true;
    this->apply_sweep_candidate_(now);
    return;
  }
  if (now - this->sweep_phase_start_ < this->sweep_dwell_ms_)
    return;
  this->record_sweep_result_();
  this->sweep_idx_++;
  if (this->sweep_idx_ >= this->sweep_total_()) {
    this->finish_sweep_(now);
    return;
  }
  this->apply_sweep_candidate_(now);
}

}  // namespace esphome::rs485_frame

#endif  // USE_RS485_FRAME_DISCOVERY
