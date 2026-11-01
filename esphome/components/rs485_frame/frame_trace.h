#pragma once

// defines.h must come before the #ifdef gate so a translation unit that opens this header
// without first including a core component header (e.g., frame_trace.cpp itself) still sees
// USE_RS485_FRAME_FRAME_TRACE before the conditional is evaluated.
#include "esphome/core/defines.h"

#ifdef USE_RS485_FRAME_FRAME_TRACE

#include "esphome/core/helpers.h"

#include <cstdint>
#include <memory>

namespace esphome::rs485_frame {

// Upper bound for the user-configurable frame_trace.size (ring buffer capacity, in frames).
// Kept purely as a "don't typo a giant number" guard, same role as
// SNIFFER_MAX_FRAME_TYPES_UPPER; the Python schema enforces it too.
static constexpr size_t FRAME_TRACE_SIZE_UPPER = 128;

// Upper bound for frame_trace.capture_bytes. FrameTraceEntry::captured_len is uint8_t-backed,
// so 256 would wrap to 0 -- cap at 255, mirroring SNIFFER_PAYLOAD_CAPTURE_BYTES_UPPER.
static constexpr size_t FRAME_TRACE_CAPTURE_BYTES_UPPER = 255;

// One captured frame. Bytes are the raw on-wire frame exactly as sent/received -- DLE+STX
// through DLE+ETX, still escape-stuffed -- truncated to capture_bytes. This is deliberately
// the *stuffed* wire representation rather than a de-stuffed reconstruction: for a frame that
// fails validation because of a broken escape sequence, de-stuffing is not well-defined (the
// unescape loop bails out partway through), while the raw wire bytes are always complete and
// correct regardless of why validation failed. It also matches this feature's stated purpose
// most literally -- "what was actually on the wire" -- for both valid and invalid entries.
//
// bytes is allocated once per ring slot at FrameTrace::init() and reused for the life of the
// hub; record() only memcpy's into it, so the ring never allocates on the hot path after setup.
struct FrameTraceEntry {
  std::unique_ptr<uint8_t[]> bytes;
  uint32_t timestamp_ms{0};
  // Actual on-wire frame length before truncation. Saturates at UINT16_MAX (far beyond any
  // realistic max_frame_length). captured_len is how many of those bytes are actually stored
  // in `bytes` (<= capture_bytes).
  uint16_t total_len{0};
  uint8_t captured_len{0};
  bool is_tx{false};
  bool valid{true};

  // Allocates the bytes buffer. Called once per slot when the ring first grows into it;
  // later record() calls just memcpy into bytes.get().
  void init(size_t capacity) { this->bytes = std::make_unique<uint8_t[]>(capacity); }
};

// Optional diagnostic: a fixed-size ring buffer of recent RX and TX frames, dumped to the log
// on demand via the rs485_frame.dump_frame_trace action. Compiled out unless
// USE_RS485_FRAME_FRAME_TRACE is defined; the hub holds a unique_ptr that is nullptr in
// production, so the hot path is a single null-check.
//
// Unlike sniffer_stats/response_monitor, this module also needs to see frames that fail
// CRC/framing validation (include_invalid:) -- those never reach process_raw_frame_'s
// validated-RX tap, so the hub calls record() from the reject path too, tagged valid=false.
class FrameTrace {
 public:
  // Called once during to_code wiring. size is bounded by FRAME_TRACE_SIZE_UPPER,
  // capture_bytes by FRAME_TRACE_CAPTURE_BYTES_UPPER.
  void init(size_t size, size_t capture_bytes, bool include_invalid);

  // Records one frame. data/len are the raw on-wire bytes (DLE+STX...DLE+ETX, still
  // escape-stuffed) -- raw_frame_ for RX, the frame handed to write_frame_ for TX. A
  // valid=false frame is dropped unless include_invalid was set at init(). Returns
  // immediately if init() was never called.
  void record(const uint8_t *data, size_t len, bool is_tx, bool valid, uint32_t now);

  // Logs the ring, oldest to newest, tagging TX vs RX and valid vs invalid per entry.
  void dump() const;

 protected:
  FixedVector<FrameTraceEntry> entries_;
  // Index of the next slot to write. During initial fill this equals entries_.size(); once
  // the ring is full it wraps mod entries_.capacity() and overwrites the oldest entry.
  size_t write_idx_{0};
  size_t capture_bytes_{0};
  bool include_invalid_{false};
  bool initialized_{false};
  // Pre-allocated at init() so dump() never allocates. Sized via format_hex_size(capture_bytes_)
  // (2 hex chars per byte plus a null terminator), matching rs485_frame.cpp's dump_frames
  // hex_log_buf_ convention -- no separators, since dump() logs total_len/captured_len
  // separately rather than needing byte boundaries visible in the hex string.
  std::unique_ptr<char[]> hex_buf_;
};

}  // namespace esphome::rs485_frame

#endif  // USE_RS485_FRAME_FRAME_TRACE
