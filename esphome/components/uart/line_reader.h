#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace esphome::uart {

/// Collects bytes into NUL terminated lines in a fixed buffer of N bytes, so lines hold up to N - 1 characters.
/// The terminator ends a line and is not stored; the ignore character is dropped wherever it appears
/// (IGNORE = '\0' only drops NUL bytes). A line longer than N - 1 characters is dropped up to its terminator.
template<size_t N, char TERMINATOR = '\n', char IGNORE = '\r'> class LineReader {
  // pos_ is the length so far; N means "overflowed on this byte", N + 1 "dropping up to the terminator"
  using pos_t = std::conditional_t<(N < 0xFE), uint8_t, uint16_t>;
  static_assert(N >= 2 && N < 0xFFFE, "LineReader size out of range");
  static_assert(TERMINATOR != IGNORE, "LineReader terminator and ignore byte must differ");

 public:
  /// Feed one byte. Returns the completed line, valid until the next call on this reader, or nullptr.
  /// Empty lines are returned too, as an empty string.
  char *feed(uint8_t c) {
    pos_t pos = this->pos_;
    if (pos == N) {
      this->pos_ = pos = N + 1;
    }
    if (c == static_cast<uint8_t>(IGNORE)) {
      return nullptr;
    }
    if (c == static_cast<uint8_t>(TERMINATOR)) {
      this->pos_ = 0;
      if (pos > N) {
        return nullptr;
      }
      this->buf_[pos] = '\0';
      return this->buf_;
    }
    if (pos < N - 1) {
      this->buf_[pos] = static_cast<char>(c);
      this->pos_ = pos + 1;
    } else if (pos == N - 1) {
      this->pos_ = N;
    }
    return nullptr;
  }

  /// Characters of the partial line collected so far; 0 right after a line was returned, and from the byte
  /// that overflowed until the terminator.
  size_t length() const { return this->pos_ < N ? this->pos_ : 0; }

  /// The partial line, NUL terminated, for prompts that have no line end. On the byte that overflowed it is
  /// the first N - 1 characters; while the rest of that line is dropped it is empty.
  const char *partial() {
    this->buf_[this->end_()] = '\0';
    return this->buf_;
  }

  /// True only right after the feed() that found the line too long; the rest of that line is then dropped.
  bool overflowed() const { return this->pos_ == N; }

  /// Forget the partial line and start a new one with the next byte.
  void reset() { this->pos_ = 0; }

  /// Drop everything up to and including the next terminator.
  void discard_line() { this->pos_ = N + 1; }

 protected:
  pos_t end_() const {
    if (this->pos_ < N) {
      return this->pos_;
    }
    return this->pos_ == N ? N - 1 : 0;
  }

  char buf_[N];
  pos_t pos_{0};
};

}  // namespace esphome::uart
