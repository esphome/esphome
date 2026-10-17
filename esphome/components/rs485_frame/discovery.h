#pragma once

// defines.h must come before the #ifdef gate so a translation unit that opens this header
// without first including a core component header (e.g., discovery.cpp itself) still sees
// USE_RS485_FRAME_DISCOVERY before the conditional is evaluated.
#include "esphome/core/defines.h"

#ifdef USE_RS485_FRAME_DISCOVERY

#include <cstddef>
#include <cstdint>
#include <vector>

#include "esphome/components/uart/uart.h"

namespace esphome::rs485_frame {

// Passive framing/CRC discovery for an unknown DLE-framed bus. This is a bring-up tool: with
// discovery: enabled the hub does no framing, validation, or transmission — it captures raw
// bytes, segments them into bursts by idle gap, and periodically logs candidate framing bytes,
// the escape scheme, and any CRC scheme that matches consistently across captured frames.
//
// It tests the DLE-framing hypothesis (opening DLE+STX, closing DLE+ETX, byte-stuffed DLEs).
// A bus that is length-prefixed, fixed-size, or Modbus-style produces no consistent candidate,
// which is itself useful information. Everything here is compiled out unless the YAML enables
// discovery (see USE_RS485_FRAME_DISCOVERY).
class RS485FrameDiscovery {
 public:
  RS485FrameDiscovery(uint32_t report_interval_ms, uint32_t idle_gap_ms, size_t max_burst,
                      uint8_t min_framing_confidence)
      : report_interval_ms_(report_interval_ms),
        idle_gap_ms_(idle_gap_ms),
        max_burst_(max_burst),
        min_framing_confidence_(min_framing_confidence) {}

  // Enable the baud / data-bits sweep. Before normal analysis, the discovery cycles the UART
  // through each (baud rate, data-bit width) candidate for dwell_ms, scores the framing at each
  // using the same burst/CRC machinery, then locks onto the highest-scoring setting and reports
  // a ready-to-paste uart: snippet. Reconfiguration uses the UART's runtime load_settings(),
  // which is implemented on ESP-IDF and ESP8266; on platforms without it the sweep is a no-op.
  // Parity and stop bits cannot be told apart by passive listening, so they are not swept (see
  // the component docs). bauds empty (or uart null) leaves the sweep disabled.
  void set_baud_sweep(uart::UARTComponent *uart, const std::vector<uint32_t> &bauds,
                      const std::vector<uint8_t> &data_bits, uint32_t dwell_ms);

  // Reserve the burst buffer once; no allocation after setup().
  void setup();
  // Append one received byte to the current burst (stamped with the loop time for gap timing).
  void feed_byte(uint8_t b, uint32_t now);
  // Close the current burst once the bus has been idle for idle_gap_ms_, and emit the periodic
  // report every report_interval_ms_.
  void tick(uint32_t now);

 protected:
  static constexpr size_t PAIR_TABLE_SIZE = 16;
  // CRC hypotheses scored against each candidate-valid frame: {sum8, xor8, sum16, crc16_modbus}
  // x {header_inclusive, payload_only} x {endianness for 2-byte}. 1-byte algos ignore endian.
  static constexpr size_t NUM_CRC_HYPS = 12;
  // Each hypothesis is scored twice: once over the unescaped frame content and once over the
  // raw on-wire content (the "CRC computed over escaped bytes" diagnostic — a scheme this
  // component cannot represent, but worth surfacing during discovery).
  static constexpr size_t NUM_ESCAPE_VIEWS = 2;

  struct BytePair {
    uint8_t a;
    uint8_t b;
    uint32_t count;
  };

  void close_burst_(uint32_t now);
  void analyze_burst_();
  // Process one extracted frame: interior bytes are b[inner_start, frame_end) (between the
  // opening STX and the closing DLE). Updates the escape histogram and CRC counters.
  void analyze_frame_(const std::vector<uint8_t> &b, size_t inner_start, size_t frame_end, uint8_t dle);
  void report_();
  static void bump_pair(BytePair *table, size_t &len, uint8_t a, uint8_t b);
  static const BytePair *top_pair(const BytePair *table, size_t len);
  void reset_scoring_();
  void score_crc_(const std::vector<uint8_t> &content, bool unescaped_view);
  // Framing confidence (percent) = the weaker of the top start-pair and end-pair shares of the
  // voting bursts. Shared by report_() and the sweep result recorder.
  uint32_t compute_confidence_(const BytePair *top_start, const BytePair *top_end) const;
  // True once any CRC hypothesis has cleared its sample threshold with an unbroken match streak.
  bool any_crc_match_() const;

  // --- Baud / data-bits sweep ---
  struct SweepResult {
    uint32_t baud;
    uint8_t data_bits;
    bool dle_agrees;
    uint32_t confidence;  // best framing confidence reached during the dwell
    bool crc_matched;
    uint32_t frames;
  };
  // Wipe all accumulated analysis state for a fresh measurement (used between sweep candidates
  // and once more when the sweep locks, so post-sweep capture starts clean).
  void reset_analyzer_();
  size_t sweep_total_() const { return this->sweep_bauds_.size() * this->sweep_data_bits_.size(); }
  uint32_t sweep_baud_at_(size_t idx) const { return this->sweep_bauds_[idx / this->sweep_data_bits_.size()]; }
  uint8_t sweep_data_bits_at_(size_t idx) const { return this->sweep_data_bits_[idx % this->sweep_data_bits_.size()]; }
  void sweep_tick_(uint32_t now);
  void apply_sweep_candidate_(uint32_t now);
  void record_sweep_result_();
  void finish_sweep_(uint32_t now);

  uint32_t report_interval_ms_;
  uint32_t idle_gap_ms_;
  size_t max_burst_;
  // The top start/end delimiter pair must account for at least this percentage of the bursts
  // that voted (length >= 4) before the framing is reported as confident and a ready-to-paste
  // config is suggested. 0 disables the gate. CRC scoring is unaffected: it always runs against
  // the current top candidate and self-corrects via reset_scoring_ when that candidate shifts.
  uint8_t min_framing_confidence_;

  std::vector<uint8_t> burst_;
  std::vector<uint8_t> unescaped_;  // scratch for unescaping a candidate frame's interior
  std::vector<uint8_t> raw_inner_;  // scratch for the raw (escaped) interior
  uint32_t last_byte_time_{0};
  bool burst_open_{false};
  bool burst_truncated_{false};

  uint32_t last_report_time_{0};
  bool report_primed_{false};
  // Heartbeat so the log is not silent for a whole report interval after boot. A one-shot boot
  // line is unreliable: it is logged before the network log client connects and so is missed.
  uint32_t last_status_time_{0};
  bool first_report_done_{false};

  uint32_t total_bursts_{0};
  uint32_t framing_bursts_{0};  // bursts long enough (>= 4 bytes) to vote for a delimiter pair
  uint32_t total_frames_{0};    // individual frames extracted from all bursts

  BytePair start_pairs_[PAIR_TABLE_SIZE];
  BytePair end_pairs_[PAIR_TABLE_SIZE];
  size_t start_pairs_len_{0};
  size_t end_pairs_len_{0};

  // Histogram of the byte following an interior DLE (the current DLE candidate), used to
  // classify the escape scheme: a peak at DLE means doubling, a peak at some other byte means
  // that byte is the escape marker.
  uint32_t dle_succ_hist_[256] = {};
  uint32_t dle_succ_total_{0};

  // Framing/escape candidate the CRC counters were scored against. When the candidate shifts
  // (early on, before it stabilizes) the counters and successor histogram are reset so they
  // reflect a single consistent hypothesis.
  bool scored_valid_{false};
  uint8_t scored_dle_{0};
  uint8_t scored_stx_{0};
  uint8_t scored_etx_{0};
  uint8_t scored_marker_{0};
  bool scored_has_marker_{false};

  uint32_t crc_samples_[NUM_ESCAPE_VIEWS][NUM_CRC_HYPS] = {};
  uint32_t crc_matches_[NUM_ESCAPE_VIEWS][NUM_CRC_HYPS] = {};

  // Baud / data-bits sweep state. Empty sweep_bauds_ (the default) leaves it disabled and the
  // analyzer runs continuously at the YAML-configured UART settings.
  uart::UARTComponent *uart_{nullptr};
  std::vector<uint32_t> sweep_bauds_;
  std::vector<uint8_t> sweep_data_bits_;
  std::vector<SweepResult> sweep_results_;
  uint32_t sweep_dwell_ms_{0};
  bool sweeping_{false};
  bool sweep_started_{false};
  bool sweep_done_{false};
  size_t sweep_idx_{0};
  uint32_t sweep_phase_start_{0};
};

}  // namespace esphome::rs485_frame

#endif  // USE_RS485_FRAME_DISCOVERY
