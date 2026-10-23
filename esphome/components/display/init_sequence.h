#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::display {

/// Count byte that marks a delay entry: the byte before it is the delay in ms.
static constexpr uint8_t INIT_SEQUENCE_DELAY_FLAG = 0xFF;

enum class InitSequenceStep : uint8_t {
  INIT_SEQUENCE_STEP_COMMAND,
  INIT_SEQUENCE_STEP_DELAY,
  INIT_SEQUENCE_STEP_END,
  INIT_SEQUENCE_STEP_MALFORMED,
};

/// One entry of an init sequence. `value` is the command, or the delay in ms.
/// `args` points into the sequence, so it is a PROGMEM pointer when the sequence is.
struct InitSequenceEntry {
  const uint8_t *args;
  uint8_t value;
  uint8_t count;
};

/// Walks a sequence built by mipi.flatten_sequence: `cmd, count, args...` or `ms, 0xFF`.
/// Reads through the progmem helpers, and keeps its position so a caller can stop at a delay and resume.
class InitSequenceReader {
 public:
  InitSequenceReader(const uint8_t *sequence, size_t length) : sequence_(sequence), length_(length) {}

  /// Fill `entry` with the next entry. On MALFORMED, `value` and `count` hold what was read, if anything.
  InitSequenceStep next(InitSequenceEntry &entry);

 protected:
  const uint8_t *sequence_;
  size_t length_;
  size_t index_{0};
};

}  // namespace esphome::display
