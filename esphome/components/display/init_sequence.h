#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::display {

/// Count byte that marks a delay entry: the byte before it is the delay in ms.
static constexpr uint8_t INIT_SEQUENCE_DELAY_FLAG = 0xFF;

/// `value` is the command or the delay in ms; `args` points into the (possibly PROGMEM) sequence.
struct InitSequenceEntry {
  const uint8_t *args;
  uint8_t value;
  uint8_t count;
  bool is_delay() const { return this->count == INIT_SEQUENCE_DELAY_FLAG; }
};

/// Walks a mipi.flatten_sequence table (`cmd, count, args...` or `ms, 0xFF`) with progmem reads; resumable.
class InitSequenceReader {
 public:
  InitSequenceReader(const uint8_t *sequence, size_t length) : sequence_(sequence), length_(length) {}

  /// Fill `entry` with the next entry; false at the end or on a malformed entry, or one over `max_count` args.
  bool next(InitSequenceEntry &entry, uint8_t max_count = 0xFE);
  bool malformed() const { return this->index_ > this->length_; }

 protected:
  const uint8_t *sequence_;
  size_t length_;
  size_t index_{0};
};

}  // namespace esphome::display
