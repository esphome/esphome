#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::fendt_caravan {

/// The panel takes a command in chunks of this many bytes; a chunk that continues ends in CONTINUATION
static constexpr size_t CHUNK_SIZE = 17;
static constexpr char CONTINUATION = '@';

/// Copies the chunk of `command` that starts at `pos` into `out` (CHUNK_SIZE + 1 bytes), returns the bytes to write
inline size_t next_chunk(const char *command, size_t len, size_t pos, char *out) {
  const size_t n = std::min(CHUNK_SIZE, len - pos);
  memcpy(out, command + pos, n);
  if (pos + n == len)
    return n;
  out[n] = CONTINUATION;
  return n + 1;
}

enum class FrameResult : uint8_t { FRAME_RESULT_INCOMPLETE, FRAME_RESULT_COMPLETE, FRAME_RESULT_OVERFLOW };

/// Joins notification fragments into one "KEY:value" report; a fragment that ends in CONTINUATION goes on in
/// the next one
template<size_t N> class FrameAssembler {
  static_assert(N < 256, "the length is kept in a byte");

 public:
  FrameResult push(const uint8_t *fragment, size_t len) {
    const bool more = len != 0 && fragment[len - 1] == CONTINUATION;
    if (more)
      len--;
    if (this->dropping_) {
      // The rest of a report that overflowed is not a report of its own
      this->dropping_ = more;
      return FrameResult::FRAME_RESULT_INCOMPLETE;
    }
    if (this->len_ + len >= N) {
      this->reset();
      this->dropping_ = more;
      return FrameResult::FRAME_RESULT_OVERFLOW;
    }
    memcpy(this->buf_ + this->len_, fragment, len);
    this->len_ += len;
    if (more)
      return FrameResult::FRAME_RESULT_INCOMPLETE;
    this->buf_[this->len_] = '\0';
    return FrameResult::FRAME_RESULT_COMPLETE;
  }
  /// Fragments received so far, a complete report not yet taken with reset(), or the rest of an overflowed report
  /// still arriving
  bool pending() const { return this->len_ != 0 || this->dropping_; }
  /// The report, NUL terminated and writable so a decoder can edit it in place
  char *data() { return this->buf_; }
  void reset() {
    this->len_ = 0;
    this->dropping_ = false;
  }

 protected:
  char buf_[N];
  uint8_t len_{0};
  bool dropping_{false};
};

}  // namespace esphome::fendt_caravan
