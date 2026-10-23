#include "init_sequence.h"
#include "esphome/core/hal.h"

namespace esphome::display {

InitSequenceStep InitSequenceReader::next(InitSequenceEntry &entry) {
  const size_t remaining = this->length_ - this->index_;
  if (remaining == 0)
    return InitSequenceStep::INIT_SEQUENCE_STEP_END;
  if (remaining < 2) {
    this->index_ = this->length_;
    return InitSequenceStep::INIT_SEQUENCE_STEP_MALFORMED;
  }
  const uint8_t *pos = this->sequence_ + this->index_;
  entry.value = progmem_read_byte(pos);
  entry.count = progmem_read_byte(pos + 1);
  entry.args = pos + 2;
  if (entry.count == INIT_SEQUENCE_DELAY_FLAG) {
    this->index_ += 2;
    return InitSequenceStep::INIT_SEQUENCE_STEP_DELAY;
  }
  if (remaining - 2 < entry.count) {
    this->index_ = this->length_;
    return InitSequenceStep::INIT_SEQUENCE_STEP_MALFORMED;
  }
  this->index_ += 2 + entry.count;
  return InitSequenceStep::INIT_SEQUENCE_STEP_COMMAND;
}

}  // namespace esphome::display
