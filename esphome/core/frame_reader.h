#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

// Out of line, one copy: the frame readers' rare paths call each other, and GCC would add specialised clones
#if defined(__clang__)
#define ESPHOME_FRAME_READER_COLD __attribute__((noinline))
#else
#define ESPHOME_FRAME_READER_COLD __attribute__((noinline, noclone))
#endif

namespace esphome {

namespace frame_reader_detail {
// A fold, not an array, so no header table lands in RAM on ESP8266
template<uint8_t... HEADER> constexpr uint8_t header_byte(size_t i) {
  size_t j = 0;
  uint8_t r = 0;
  ((r = (j++ == i) ? HEADER : r), ...);
  return r;
}
template<class FIRST, class... REST> struct First {
  using type = FIRST;
};
}  // namespace frame_reader_detail

/// Collects fixed length binary frames of N bytes that start with the header bytes HEADER...; the binary twin of
/// LineReader. Bytes before a header are skipped; checksums are left to the caller.
template<size_t N, uint8_t... HEADER> class FrameReader {
  using pos_t = std::conditional_t<(N <= 0xFF), uint8_t, uint16_t>;
  static constexpr size_t HEADER_LEN = sizeof...(HEADER);
  static_assert(HEADER_LEN >= 1 && HEADER_LEN <= 8, "FrameReader takes 1 to 8 header bytes");  // bounds replay_() depth
  static_assert(HEADER_LEN < N && N <= 0xFFFF, "FrameReader size out of range");

 public:
  /// Feed one byte; returns the completed frame, header included, valid until the next call, or nullptr.
  const uint8_t *feed(uint8_t c) {
    pos_t pos = this->pos_;
    if (pos < HEADER_LEN && c != frame_reader_detail::header_byte<HEADER...>(pos)) {
      // A later byte of the broken header, or c, may start the real one
      if (pos == 0)
        return nullptr;
      this->buf_[pos] = c;
      return this->replay_(pos);
    }
    this->buf_[pos++] = c;
    if (pos < N) {
      this->pos_ = pos;
      return nullptr;
    }
    this->pos_ = 0;
    return this->buf_;
  }

  /// Number of bytes of the partial frame; 0 after a frame is returned and while looking for a header.
  size_t length() const { return this->pos_; }

  /// Forget the partial frame and look for a header from the next byte.
  void reset() { this->pos_ = 0; }

  /// For a frame the caller rejected (bad checksum): drop its first byte and realign on a header in the rest.
  /// Call only right after feed() returned that frame; use reset() otherwise.
  void resync() { this->replay_(N - 1); }

 protected:
  // Start over and feed buf_[1..last] again; never completes a frame as last < N, so it returns nullptr, which lets
  // feed() end in a tail call. Out of line, as feed() and replay_() call each other.
  const uint8_t *ESPHOME_FRAME_READER_COLD replay_(size_t last) {
    this->pos_ = 0;
    for (size_t i = 1; i <= last; i++) {
      this->feed(this->buf_[i]);
    }
    return nullptr;
  }

  uint8_t buf_[N];
  pos_t pos_{0};
};

/// Length rules for FrameFormat: the frame length is known once KNOWN_AT bytes are collected, and get() returns it
/// for the whole frame, header included. A component can pass its own type with these two members.
/// One length byte at POS; ADD is added to give the whole frame length.
template<size_t POS, size_t ADD = 0> struct FrameLengthU8 {
  static constexpr size_t KNOWN_AT = POS + 1;
  static constexpr size_t get(const uint8_t *buf) { return buf[POS] + ADD; }
};
/// Two length bytes at POS, high byte first.
template<size_t POS, size_t ADD = 0> struct FrameLengthU16BE {
  static constexpr size_t KNOWN_AT = POS + 2;
  static constexpr size_t get(const uint8_t *buf) { return ((size_t(buf[POS]) << 8) | buf[POS + 1]) + ADD; }
};
/// Two length bytes at POS, low byte first.
template<size_t POS, size_t ADD = 0> struct FrameLengthU16LE {
  static constexpr size_t KNOWN_AT = POS + 2;
  static constexpr size_t get(const uint8_t *buf) { return (buf[POS] | (size_t(buf[POS + 1]) << 8)) + ADD; }
};
/// A frame type that always has LEN bytes.
template<size_t LEN> struct FrameFixedLength {
  static_assert(LEN >= 2, "A fixed length frame has at least 2 bytes");
  static constexpr size_t KNOWN_AT = 1;
  static constexpr size_t get(const uint8_t * /*buf*/) { return LEN; }
};

/// One frame type for LengthFrameReader: frames that start with the header bytes HEADER... and whose length follows
/// LENGTH, one of the FrameLength types above.
template<class LENGTH, uint8_t... HEADER> struct FrameFormat {
  using Length = LENGTH;
  static constexpr size_t HEADER_LEN = sizeof...(HEADER);
  static constexpr uint8_t FIRST = frame_reader_detail::header_byte<HEADER...>(0);
  static_assert(HEADER_LEN >= 1 && HEADER_LEN <= 8, "FrameFormat takes 1 to 8 header bytes");
  static_assert(LENGTH::KNOWN_AT == 1 || LENGTH::KNOWN_AT > HEADER_LEN, "The length field must follow the header");
  static constexpr uint8_t header_byte(size_t i) { return frame_reader_detail::header_byte<HEADER...>(i); }
};

/// Collects binary frames that give their own length, of up to N bytes, in one or more FrameFormats; the first byte
/// picks the format, so each format needs a different first header byte. Bytes before a header are skipped. A length
/// that ends the frame before its length field, or is longer than N, is not a frame, and the reader looks for a
/// header after its first byte. Checksums, footers and a minimum length that covers them are left to the caller:
///
///   const uint8_t *frame = reader.feed(c);
///   while (frame != nullptr)
///     frame = valid(frame, reader.frame_length()) ? (handle(frame), reader.next()) : reader.resync();
template<size_t N, class... FORMATS> class LengthFrameReader {
  using pos_t = std::conditional_t<(N <= 0xFF), uint8_t, uint16_t>;
  static_assert(sizeof...(FORMATS) >= 1, "LengthFrameReader takes at least one FrameFormat");
  static_assert(N >= 2 && N <= 0xFFFF, "LengthFrameReader size out of range");

 public:
  /// Feed one byte; returns the completed frame, header included, valid until the next call, or nullptr.
  /// frame_length() gives its length.
  inline const uint8_t *__attribute__((always_inline)) feed(uint8_t c) {
    // Most bytes are in the middle of a frame: no header or length to check and not the last byte
    const pos_t pos = this->pos_;
    bool middle;
    if constexpr (sizeof...(FORMATS) == 1) {
      middle = this->in_middle_<FORMATS...>(pos);
    } else {
      const uint8_t first = this->buf_[0];
      middle = ((first == FORMATS::FIRST && this->in_middle_<FORMATS>(pos)) || ...);
    }
    if (middle) {
      this->buf_[pos] = c;
      this->pos_ = pos + 1;
      return nullptr;
    }
    return this->feed_edge_(c);
  }

  /// Length of the frame feed(), resync() or next() returned; valid until the next call on the reader.
  size_t frame_length() const {
    if constexpr (sizeof...(FORMATS) == 1) {
      return frame_reader_detail::First<FORMATS...>::type::Length::get(this->buf_);
    } else {
      const uint8_t first = this->buf_[0];
      size_t len = 0;
      (void) ((first == FORMATS::FIRST && ((len = FORMATS::Length::get(this->buf_)), true)) || ...);
      return len;
    }
  }

  /// Number of bytes of the partial frame; 0 after a frame is returned and while looking for a header.
  size_t length() const { return this->pos_; }

  /// Forget the partial frame and look for a header from the next byte.
  void reset() {
    this->pos_ = 0;
    this->held_end_ = 0;
  }

  /// For a frame the caller rejected (too short, a bad checksum or footer), or a partial frame left too long without
  /// a byte: drop its first byte and look for a frame in the rest. Returns the next frame found, like feed(), or
  /// nullptr. Does nothing when no frame is held and nothing is collected.
  const uint8_t *resync() {
    if (this->pos_ != 0)
      return this->replay_(1, this->pos_);
    return this->held_end_ != 0 ? this->replay_(1, this->held_end_) : nullptr;
  }

  /// After handling a frame: look for the next one in the bytes that followed it in a resync(). Returns that frame,
  /// or nullptr once they are used up; feed() then goes on as usual.
  const uint8_t *next() {
    const size_t len = this->frame_length();
    const size_t end = this->held_end_;
    if (end <= len) {
      this->held_end_ = 0;
      return nullptr;
    }
    return this->replay_(len, end);
  }

 protected:
  template<class F> bool in_middle_(pos_t pos) const {
    constexpr size_t from = F::Length::KNOWN_AT > F::HEADER_LEN ? F::Length::KNOWN_AT : F::HEADER_LEN;
    return pos >= from && pos + 1u < F::Length::get(this->buf_);
  }

  // A header byte, the byte that completes the length, the last byte of a frame, or a byte between frames
  const uint8_t *ESPHOME_FRAME_READER_COLD feed_edge_(uint8_t c) {
    const pos_t pos = this->pos_;
    if (pos == 0) {
      this->held_end_ = 0;
      const bool header = ((c == FORMATS::FIRST) || ...);
      if (header) {
        this->buf_[0] = c;
        this->pos_ = 1;
      }
      return nullptr;
    }
    if constexpr (sizeof...(FORMATS) == 1) {
      return this->feed_as_<FORMATS...>(pos, c);
    } else {
      const uint8_t first = this->buf_[0];
      const uint8_t *frame = nullptr;
      (void) ((first == FORMATS::FIRST && ((frame = this->feed_as_<FORMATS>(pos, c)), true)) || ...);
      return frame;
    }
  }

  template<class F> const uint8_t *feed_as_(pos_t pos, uint8_t c) {
    if (pos < F::HEADER_LEN && c != F::header_byte(pos)) {
      // A later byte of the broken header, or c, may start the real one
      this->buf_[pos] = c;
      return this->replay_(1, pos + 1);
    }
    this->buf_[pos++] = c;
    if (pos >= F::Length::KNOWN_AT) {
      const size_t len = F::Length::get(this->buf_);
      if (pos == F::Length::KNOWN_AT && (len < F::Length::KNOWN_AT || len < F::HEADER_LEN || len > N))
        return this->replay_(1, pos);
      if (pos == len) {
        this->pos_ = 0;
        this->held_end_ = len;
        return this->buf_;
      }
    }
    this->pos_ = pos;
    return nullptr;
  }

  // Start over and feed buf_[from..to) again; writes stay behind the read position. On a completed frame, moves the
  // bytes not fed yet right behind it, for next() and resync(), and returns it. Out of line, as feed() and replay_()
  // call each other.
  const uint8_t *ESPHOME_FRAME_READER_COLD replay_(size_t from, size_t to) {
    this->pos_ = 0;
    this->held_end_ = 0;
    for (size_t i = from; i < to; i++) {
      if (this->feed(this->buf_[i]) == nullptr)
        continue;
      // A nested replay may already have put bytes of its own behind the frame
      const size_t held = this->held_end_;
      const size_t rest = to - i - 1;
      std::memmove(this->buf_ + held, this->buf_ + i + 1, rest);
      this->held_end_ = held + rest;
      return this->buf_;
    }
    return nullptr;
  }

  uint8_t buf_[N];
  pos_t pos_{0};
  // While a frame is held: end of the frame plus the bytes after it that are not fed yet; 0 otherwise
  pos_t held_end_{0};
};

}  // namespace esphome
