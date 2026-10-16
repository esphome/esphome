#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::uart {

// pos_ holds the line length, or one of these states. LINE_READER_DONE is a flag on the length.
static constexpr uint16_t LINE_READER_DONE = 0x8000;
static constexpr uint16_t LINE_READER_OVERFLOWED = 0x7FFE;
static constexpr uint16_t LINE_READER_DISCARDING = 0x7FFF;

/// Byte handling shared by every LineReader, so it is in flash once. Bytes are passed widened to 32 bits.
char *line_reader_feed(char *buf, size_t size, uint16_t &pos, uint32_t c, uint32_t terminator, uint32_t ignore);

/// Collects bytes into NUL terminated lines in a fixed buffer of N bytes, so lines hold up to N - 1 characters.
/// The terminator ends a line and is not stored; the ignore character is dropped wherever it appears
/// (IGNORE = '\0' only drops NUL bytes). A line longer than N - 1 characters is dropped up to its terminator.
template<size_t N, char TERMINATOR = '\n', char IGNORE = '\r'> class LineReader {
  static_assert(N >= 2 && N < LINE_READER_OVERFLOWED, "LineReader size out of range");

 public:
  /// Feed one byte. Returns the completed line, valid until the next feed(), or nullptr.
  /// Empty lines are returned too, as an empty string.
  char *feed(uint8_t c) {
    return line_reader_feed(this->buf_, N, this->pos_, c, static_cast<uint8_t>(TERMINATOR),
                            static_cast<uint8_t>(IGNORE));
  }

  /// Length of the line feed() just returned, or of the partial line.
  size_t length() const {
    if (this->pos_ & LINE_READER_DONE)
      return this->pos_ & ~LINE_READER_DONE;
    return this->pos_ >= LINE_READER_OVERFLOWED ? 0 : this->pos_;
  }

  /// The line collected so far, NUL terminated, for prompts that have no line end. It is empty right after
  /// feed() returned a line and while the rest of an overlong line is dropped; on the byte that overflowed
  /// it is the first N - 1 characters.
  const char *partial() {
    // Right after a line, point at the NUL that ended it so the line itself stays intact
    if (this->pos_ & LINE_READER_DONE)
      return this->buf_ + (this->pos_ & ~LINE_READER_DONE);
    size_t end = this->pos_;
    if (end == LINE_READER_DISCARDING) {
      end = 0;
    } else if (end == LINE_READER_OVERFLOWED) {
      end = N - 1;
    }
    this->buf_[end] = '\0';
    return this->buf_;
  }

  /// True only right after the feed() that found the line too long; the rest of that line is then dropped.
  bool overflowed() const { return this->pos_ == LINE_READER_OVERFLOWED; }

  /// Forget the partial line and start a new one with the next byte.
  void reset() { this->pos_ = 0; }

  /// Drop everything up to and including the next terminator.
  void discard_line() { this->pos_ = LINE_READER_DISCARDING; }

 protected:
  uint16_t pos_{0};
  char buf_[N];
};

}  // namespace esphome::uart
