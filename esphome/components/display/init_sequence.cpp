#include "init_sequence.h"
#include "esphome/core/hal.h"

namespace esphome::display {

InitSequenceStep InitSequenceReader::next(InitSequenceEntry &entry, uint8_t max_count) {
  const size_t remaining = this->length_ - this->index_;
  if (remaining == 0)
    return INIT_SEQUENCE_STEP_END;
  const uint8_t *pos = this->sequence_ + this->index_;
  if (remaining >= 2) {
    entry.value = progmem_read_byte(pos);
    entry.count = progmem_read_byte(pos + 1);
    entry.args = pos + 2;
    if (entry.count == INIT_SEQUENCE_DELAY_FLAG) {
      this->index_ += 2;
      return INIT_SEQUENCE_STEP_DELAY;
    }
    if (entry.count <= max_count && remaining - 2 >= entry.count) {
      this->index_ += 2 + entry.count;
      return INIT_SEQUENCE_STEP_COMMAND;
    }
  }
  this->index_ = this->length_;
  return INIT_SEQUENCE_STEP_MALFORMED;
}

}  // namespace esphome::display
