#include "init_sequence.h"
#include "esphome/core/hal.h"

namespace esphome::display {

bool InitSequenceReader::next(InitSequenceEntry &entry, uint8_t max_count) {
  if (this->index_ >= this->length_)
    return false;
  const size_t remaining = this->length_ - this->index_;
  const uint8_t *pos = this->sequence_ + this->index_;
  if (remaining >= 2) {
    entry.value = progmem_read_byte(pos);
    entry.count = progmem_read_byte(pos + 1);
    entry.args = pos + 2;
    if (entry.is_delay()) {
      this->index_ += 2;
      return true;
    }
    if (entry.count <= max_count && remaining - 2 >= entry.count) {
      this->index_ += 2 + entry.count;
      return true;
    }
  }
  this->index_ = this->length_ + 1;
  return false;
}

}  // namespace esphome::display
