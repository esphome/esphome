#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace esphome::uart {

/// Logs the shared "line dropped" warning; out of line to keep each caller small.
void log_line_dropped(const char *tag, size_t max_len);

/// Collects bytes into NUL terminated lines of up to N - 1 characters; IGNORE bytes are skipped and an
/// overlong line is dropped up to its terminator.
template<size_t N, char TERMINATOR = '\n', char IGNORE = '\r'> class LineReader {
  // pos_ is the length so far; N means "overflowed on this byte", N + 1 "dropping up to the terminator"
  using pos_t = std::conditional_t<(N < 0xFE), uint8_t, uint16_t>;
  static_assert(N >= 2 && N < 0xFFFE, "LineReader size out of range");
  static_assert(TERMINATOR != IGNORE, "LineReader terminator and ignore byte must differ");

 public:
  /// Feed one byte; returns the completed line (empty lines included), valid until the next call, or nullptr.
  char *feed(uint8_t c) {
    size_t len = 0;
    return this->feed(c, len);
  }

  /// As feed(c), and sets len to the line length when a line is returned.
  char *feed(uint8_t c, size_t &len) {
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
      len = pos;
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

  /// Length of the partial line; 0 after a line is returned and while an overlong line is dropped.
  size_t length() const { return this->pos_ < N ? this->pos_ : 0; }

  /// The partial line, NUL terminated, for prompts with no line end; the first N - 1 characters on the byte
  /// that overflowed, empty while the rest of that line is dropped.
  const char *partial() {
    this->buf_[this->end_()] = '\0';
    return this->buf_;
  }

  /// True only right after the feed() that found the line too long; the rest of that line is then dropped.
  bool overflowed() const { return this->pos_ == N; }

  /// Log the shared "line dropped" warning when the last feed() overflowed.
  void log_if_overflowed(const char *tag) const {
    if (this->overflowed()) {
      log_line_dropped(tag, N - 1);
    }
  }

  /// Forget the partial line and start a new one with the next byte.
  void reset() { this->pos_ = 0; }

  /// Start a new line with c, e.g. the byte that overflowed; c must not be TERMINATOR or IGNORE.
  void restart_with(uint8_t c) {
    this->buf_[0] = static_cast<char>(c);
    this->pos_ = 1;
  }

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
