#pragma once

#include <algorithm>
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
/// for the whole frame, header included. A component can pass its own type with these two members; its get() can
/// return 0 to reject a header it can already tell is broken (a header checksum, an unknown type).
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

namespace frame_reader_detail {
// The length of a FrameFixedLength rule, 0 for every other rule
template<class LENGTH> constexpr size_t fixed_length = 0;
template<size_t LEN> constexpr size_t fixed_length<FrameFixedLength<LEN>> = LEN;
}  // namespace frame_reader_detail

/// One frame type for LengthFrameReader: frames that start with the header bytes HEADER... and whose length follows
/// LENGTH, one of the FrameLength types above.
template<class LENGTH, uint8_t... HEADER> struct FrameFormat {
  using Length = LENGTH;
  static constexpr size_t HEADER_LEN = sizeof...(HEADER);
  static constexpr uint8_t FIRST = frame_reader_detail::header_byte<HEADER...>(0);
  // Bytes collected when the length is checked; also the shortest frame
  static constexpr size_t MIN_LEN = std::max<size_t>({LENGTH::KNOWN_AT, HEADER_LEN, 2});
  static_assert(HEADER_LEN >= 1 && HEADER_LEN <= 8, "FrameFormat takes 1 to 8 header bytes");
  static_assert(LENGTH::KNOWN_AT == 1 || LENGTH::KNOWN_AT > HEADER_LEN, "The length field must follow the header");
  static constexpr uint8_t header_byte(size_t i) { return frame_reader_detail::header_byte<HEADER...>(i); }
};

/// Collects binary frames that give their own length, of up to N bytes, in one or more FrameFormats; the first byte
/// picks the format, so each format needs a different first header byte. Bytes before a header are skipped. A length
/// that ends the frame before its length field, or is longer than N, is not a frame, and the reader looks for a
/// header after its first byte. Checksums, footers and a minimum length that covers them are left to the caller,
/// which passes a handler that returns false for a frame it rejects:
///
///   this->reader_.feed(c, [this](const uint8_t *frame, size_t len) { return this->handle_frame_(frame, len); });
///
/// For frames of one fixed size, FrameReader is smaller.
template<size_t N, class... FORMATS> class LengthFrameReader {
  using pos_t = std::conditional_t<(N <= 0xFF), uint8_t, uint16_t>;
  static_assert(sizeof...(FORMATS) >= 1, "LengthFrameReader takes at least one FrameFormat");
  static_assert(N >= 2 && N <= 0xFFFF, "LengthFrameReader size out of range");
  static constexpr bool distinct_first() {
    const uint8_t first[] = {FORMATS::FIRST...};
    for (size_t i = 0; i < sizeof...(FORMATS); i++) {
      for (size_t j = i + 1; j < sizeof...(FORMATS); j++) {
        if (first[i] == first[j])
          return false;
      }
    }
    return true;
  }
  static_assert(distinct_first(), "Each FrameFormat needs a different first header byte");
  static_assert(((FORMATS::MIN_LEN <= N) && ...), "N must hold the header and length field of every FrameFormat");
  static_assert(((frame_reader_detail::fixed_length<typename FORMATS::Length> == 0 ||
                  (frame_reader_detail::fixed_length<typename FORMATS::Length> >= FORMATS::MIN_LEN &&
                   frame_reader_detail::fixed_length<typename FORMATS::Length> <= N)) &&
                 ...),
                "A fixed length must hold the header and fit in N");

 public:
  /// Feed one byte; calls handle(frame, length) for each frame it completes, header included. A rejected frame
  /// (handle returns false) is searched for frames inside it and after it, which go to handle the same way.
  /// handle returns false when the bytes are not a real frame (checksum, footer, structure), true once it is used.
  /// handle must not feed this reader again: the frame and the bytes held behind it live in its buffer.
  template<class F> void __attribute__((always_inline)) feed(uint8_t c, F &&handle) {
    // Most bytes are in the middle of a frame whose length is known, and not its last byte
    const pos_t pos = this->pos_;
    if (pos + 1u < this->end_) {
      this->buf_[pos] = c;
      this->pos_ = pos + 1;
    } else if (const size_t len = this->feed_edge_(c)) {
      this->drain_(len, handle);
    }
  }

  /// For a partial frame left too long without a byte, which a corrupt length can cause: give up on it, and pass the
  /// frames it held back to handle. A UART component gets this from UARTDevice::read_frames().
  template<class F> void flush(F &&handle) {
    while (this->pos_ != 0)
      this->drain_(this->replay_(1, this->pos_), handle);
  }

  /// Number of bytes of the partial frame; 0 after a frame is returned and while looking for a header.
  size_t length() const { return this->pos_; }

 protected:
  // Hands each frame, len bytes at buf_, to handle; nothing is collected while a frame is held, so a handled frame
  // goes on after its end and a rejected one from its second byte, over the bytes held behind it
  template<class F> void ESPHOME_FRAME_READER_COLD drain_(size_t len, F &handle) {
    while (len != 0) {
      len = this->replay_(handle(this->buf_, len) ? len : 1, this->held_end_);
    }
  }

  // A header byte, the bytes up to the length, the last byte of a frame, or a byte between frames; returns the length
  // of a completed frame, at buf_, or 0
  size_t ESPHOME_FRAME_READER_COLD feed_edge_(uint8_t c) {
    const pos_t pos = this->pos_;
    if (pos == 0) {
      const bool header = ((c == FORMATS::FIRST) || ...);
      if (header) {
        this->buf_[0] = c;
        this->pos_ = 1;
      }
      return 0;
    }
    if constexpr (sizeof...(FORMATS) == 1) {
      return this->feed_as_<FORMATS...>(pos, c);
    } else {
      const uint8_t first = this->buf_[0];
      size_t len = 0;
      (void) ((first == FORMATS::FIRST && ((len = this->feed_as_<FORMATS>(pos, c)), true)) || ...);
      return len;
    }
  }

  template<class F> size_t feed_as_(pos_t pos, uint8_t c) {
    if (pos < F::HEADER_LEN && c != F::header_byte(pos)) {
      // A later byte of the broken header, or c, may start the real one
      this->buf_[pos] = c;
      return this->replay_(1, pos + 1);
    }
    this->buf_[pos++] = c;
    if (pos == F::MIN_LEN) {
      const size_t len = F::Length::get(this->buf_);
      if (len < F::MIN_LEN || len > N)
        return this->replay_(1, pos);
      this->end_ = len;
    }
    if (pos == this->end_) {
      this->pos_ = 0;
      this->end_ = 0;
      this->held_end_ = pos;
      return pos;
    }
    this->pos_ = pos;
    return 0;
  }

  // Start over and feed buf_[from..to) again; writes stay behind the read position. On a completed frame, moves the
  // bytes not fed yet right behind it, for drain_(), and returns its length; 0 otherwise.
  size_t ESPHOME_FRAME_READER_COLD replay_(size_t from, size_t to) {
    this->pos_ = 0;
    this->end_ = 0;
    for (size_t i = from; i < to; i++) {
      const size_t len = this->feed_edge_(this->buf_[i]);
      if (len == 0)
        continue;
      // A nested replay may already have put bytes of its own behind the frame
      const size_t held = this->held_end_;
      const size_t rest = to - i - 1;
      std::memmove(this->buf_ + held, this->buf_ + i + 1, rest);
      this->held_end_ = held + rest;
      return len;
    }
    return 0;
  }

  // Before buf_, so they stay within a short load offset of the reader when N is large
  pos_t pos_{0};
  // Length of the frame being collected once its length is known; 0 otherwise
  pos_t end_{0};
  // While a frame is held: end of the frame plus the bytes after it that are not fed yet; set when a frame completes
  pos_t held_end_{0};
  uint8_t buf_[N];
};

}  // namespace esphome

#undef ESPHOME_FRAME_READER_COLD
