#include "line_reader.h"

namespace esphome::uart {

char *line_reader_feed(char *buf, size_t size, uint16_t &pos, uint32_t c, uint32_t terminator, uint32_t ignore) {
  // A returned line is done with, and an overflow becomes plain discarding
  if (pos >= LINE_READER_OVERFLOWED)
    pos = (pos & LINE_READER_DONE) ? 0 : LINE_READER_DISCARDING;
  if (c == ignore)
    return nullptr;
  if (c == terminator) {
    if (pos == LINE_READER_DISCARDING) {
      pos = 0;
      return nullptr;
    }
    buf[pos] = '\0';
    pos |= LINE_READER_DONE;
    return buf;
  }
  if (pos >= size - 1) {
    if (pos != LINE_READER_DISCARDING)
      pos = LINE_READER_OVERFLOWED;
    return nullptr;
  }
  buf[pos++] = static_cast<char>(c);
  return nullptr;
}

}  // namespace esphome::uart
