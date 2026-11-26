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

// Length rules: get() gives the whole frame length once KNOWN_AT bytes are in, or 0 to reject the header.

/// offset: where the length field starts, counted from the first header byte; trailer: the bytes after the payload
/// (checksum, footer), for a payload length only.
struct LengthField {
  size_t offset;
  size_t trailer = 0;
};

/// Whole-frame length: the field counts every byte of the frame, header included.
template<LengthField F> struct FrameLengthU8 {
  static_assert(F.trailer == 0, "A whole-frame length has no trailer");
  static constexpr size_t KNOWN_AT = F.offset + 1;
  static constexpr size_t get(const uint8_t *buf) { return buf[F.offset]; }
};
template<LengthField F> struct FrameLengthU16BE {
  static_assert(F.trailer == 0, "A whole-frame length has no trailer");
  static constexpr size_t KNOWN_AT = F.offset + 2;
  static constexpr size_t get(const uint8_t *buf) { return (size_t(buf[F.offset]) << 8) | buf[F.offset + 1]; }
};
template<LengthField F> struct FrameLengthU16LE {
  static_assert(F.trailer == 0, "A whole-frame length has no trailer");
  static constexpr size_t KNOWN_AT = F.offset + 2;
  static constexpr size_t get(const uint8_t *buf) { return buf[F.offset] | (size_t(buf[F.offset + 1]) << 8); }
};
/// Payload length: the field counts the payload right after it.
template<LengthField F> struct PayloadLengthU8 {
  static constexpr size_t KNOWN_AT = F.offset + 1;
  static constexpr size_t get(const uint8_t *buf) { return KNOWN_AT + buf[F.offset] + F.trailer; }
};
template<LengthField F> struct PayloadLengthU16BE {
  static constexpr size_t KNOWN_AT = F.offset + 2;
  static constexpr size_t get(const uint8_t *buf) {
    return KNOWN_AT + FrameLengthU16BE<LengthField{.offset = F.offset}>::get(buf) + F.trailer;
  }
};
template<LengthField F> struct PayloadLengthU16LE {
  static constexpr size_t KNOWN_AT = F.offset + 2;
  static constexpr size_t get(const uint8_t *buf) {
    return KNOWN_AT + FrameLengthU16LE<LengthField{.offset = F.offset}>::get(buf) + F.trailer;
  }
};
/// A frame type that always has LEN bytes.
template<size_t LEN> struct FrameFixedLength {
  static_assert(LEN >= 2, "A fixed length frame has at least 2 bytes");
  static constexpr size_t KNOWN_AT = 1;
  static constexpr size_t get(const uint8_t * /*buf*/) { return LEN; }
};

/// Wraps a length rule: CHECK(buf) runs once AT bytes are in, and a header it fails gives length 0 (rejected).
template<class LENGTH, size_t AT, bool (*CHECK)(const uint8_t *)> struct CheckedLength {
  static_assert(AT >= LENGTH::KNOWN_AT, "AT must cover the length field");
  static constexpr size_t KNOWN_AT = AT;
  static size_t get(const uint8_t *buf) { return CHECK(buf) ? LENGTH::get(buf) : 0; }
};

/// Wraps a length rule: a frame longer than MAX calls WARN(length) and is rejected; a replay can warn again.
template<class LENGTH, size_t MAX, void (*WARN)(size_t)> struct WarnIfLonger {
  static constexpr size_t KNOWN_AT = LENGTH::KNOWN_AT;
  static size_t get(const uint8_t *buf) {
    const size_t len = LENGTH::get(buf);
    if (len <= MAX)
      return len;
    WARN(len);
    return 0;
  }
};

/// The shortest frame of a FrameFormat, when that is more than its header and length field.
template<size_t LEN> struct FrameMinLength {
  static constexpr size_t VALUE = LEN;
};

namespace frame_reader_detail {
// The length of a FrameFixedLength rule, 0 for every other rule
template<class LENGTH> constexpr size_t FIXED_LENGTH = 0;
template<size_t LEN> constexpr size_t FIXED_LENGTH<FrameFixedLength<LEN>> = LEN;
template<class LENGTH, size_t AT, bool (*CHECK)(const uint8_t *)>
constexpr size_t FIXED_LENGTH<CheckedLength<LENGTH, AT, CHECK>> = FIXED_LENGTH<LENGTH>;
template<class LENGTH, size_t MAX, void (*WARN)(size_t)>
constexpr size_t FIXED_LENGTH<WarnIfLonger<LENGTH, MAX, WARN>> = FIXED_LENGTH<LENGTH>;
// The MAX of a WarnIfLonger rule, 0 for every other rule
template<class LENGTH> constexpr size_t WARN_MAX = 0;
template<class LENGTH, size_t MAX, void (*WARN)(size_t)>
constexpr size_t WARN_MAX<WarnIfLonger<LENGTH, MAX, WARN>> = MAX;
template<class LENGTH, size_t AT, bool (*CHECK)(const uint8_t *)>
constexpr size_t WARN_MAX<CheckedLength<LENGTH, AT, CHECK>> = WARN_MAX<LENGTH>;
}  // namespace frame_reader_detail

/// The bytes every frame of a FrameFormat starts with.
template<uint8_t... BYTES> struct FrameHeader {
  static constexpr size_t LEN = sizeof...(BYTES);
  static_assert(LEN >= 1 && LEN <= 8, "A FrameHeader has 1 to 8 bytes");
  static constexpr uint8_t byte(size_t i) { return frame_reader_detail::header_byte<BYTES...>(i); }
};

/// One frame type for LengthFrameReader: a FrameHeader, a length rule from above and an optional FrameMinLength.
template<class HEADER, class LENGTH, class MIN = FrameMinLength<0>> struct FrameFormat {
  using Length = LENGTH;
  static constexpr size_t HEADER_LEN = HEADER::LEN;
  static constexpr uint8_t FIRST = HEADER::byte(0);
  // Bytes collected when the length is checked
  static constexpr size_t MIN_LEN = std::max<size_t>({LENGTH::KNOWN_AT, HEADER_LEN, 2});
  // A shorter length is not a frame
  static constexpr size_t SHORTEST = std::max(MIN_LEN, MIN::VALUE);
  static_assert(LENGTH::KNOWN_AT == 1 || LENGTH::KNOWN_AT > HEADER_LEN, "The length field must follow the header");
  static constexpr uint8_t header_byte(size_t i) { return HEADER::byte(i); }
};

/// Collects binary frames that give their own length, of up to N bytes, in one or more FrameFormats; the first byte
/// picks the format, so each format needs a different first header byte. Bytes before a header are skipped. A length
/// that ends the frame before its length field, or is longer than N, is not a frame, and the reader looks for a
/// header after its first byte. Checksums and footers are left to the caller, which passes a handler that returns
/// false for a frame it rejects:
///
///   this->reader_.feed(c, [this](const uint8_t *frame, size_t len) { return this->handle_frame_(frame, len); });
///
/// For frames of one fixed size, FrameReader is smaller.
template<size_t N, class... FORMATS> class LengthFrameReader {
  using pos_t = std::conditional_t<(N <= 0xFF), uint8_t, uint16_t>;
  static_assert(sizeof...(FORMATS) >= 1, "LengthFrameReader takes at least one FrameFormat");
  // flush_() copies a partial frame on the stack
  static_assert(N >= 2 && N <= 1024, "LengthFrameReader size out of range");
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
  static_assert(((FORMATS::SHORTEST <= N) && ...), "N must hold the shortest frame of every FrameFormat");
  static_assert(((frame_reader_detail::FIXED_LENGTH<typename FORMATS::Length> == 0 ||
                  (frame_reader_detail::FIXED_LENGTH<typename FORMATS::Length> >= FORMATS::SHORTEST &&
                   frame_reader_detail::FIXED_LENGTH<typename FORMATS::Length> <= N)) &&
                 ...),
                "A fixed length must hold the header and fit in N");
  static_assert(((frame_reader_detail::WARN_MAX<typename FORMATS::Length> <= N) && ...),
                "A WarnIfLonger MAX above N lets longer frames be skipped without a warning");

 public:
  /// Feed one byte; calls handle(frame, length) for each frame it completes, header included. A rejected frame
  /// (handle returns false) is searched for frames inside it and after it, which go to handle the same way.
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

  /// For a partial frame idle too long: hands over the frames a corrupt length holds back inside it, else keeps it for
  /// a slow UART. UARTDevice::read_frames() calls this.
  template<class F> void flush(F &&handle) {
    // held_end_ marks a partial frame already searched; new bytes move pos_ past it
    if (this->pos_ != 0 && this->held_end_ != this->pos_)
      this->flush_(handle);
  }

  /// Number of bytes of the partial frame; 0 after a frame is returned and while looking for a header.
  size_t length() const { return this->pos_; }

  /// Whether a frame is coming in: a partial frame that an idle flush has not searched and kept.
  bool receiving() const { return this->pos_ != 0 && this->held_end_ != this->pos_; }

  /// Forget a partial frame an idle flush kept, before a request starts a new exchange; safe inside a frame handler.
  void drop_kept() {
    if (this->pos_ != 0 && this->held_end_ == this->pos_)
      this->reset();
  }

  /// Forget the partial frame, for a request/response protocol starting a new exchange.
  void reset() {
    this->pos_ = 0;
    this->end_ = 0;
    this->held_end_ = 0;
  }

 protected:
  // Hands each frame (len bytes at buf_) to handle, then goes on after a handled frame or from the second byte of a
  // rejected one
  template<class F> bool ESPHOME_FRAME_READER_COLD drain_(size_t len, F &handle) {
    bool handled = false;
    while (len != 0) {
      const bool ok = handle(this->buf_, len);
      handled |= ok;
      len = this->replay_(ok ? len : 1, this->held_end_);
    }
    this->held_end_ = 0;
    return handled;
  }

  // Searches the partial frame for frames; when handle takes none, puts the partial frame back and marks it searched
  template<class F> void ESPHOME_FRAME_READER_COLD flush_(F &handle) {
    const pos_t pos = this->pos_;
    const pos_t end = this->end_;
    uint8_t saved[N];
    std::memcpy(saved, this->buf_, pos);
    while (this->pos_ != 0) {
      // A partial frame left behind the handed over ones is searched on the next idle pass
      if (this->drain_(this->replay_(1, this->pos_), handle))
        return;
    }
    std::memcpy(this->buf_, saved, pos);
    this->pos_ = pos;
    this->end_ = end;
    this->held_end_ = pos;
  }

  // Every byte but the middle of a frame; returns the length of a completed frame, or 0
  size_t ESPHOME_FRAME_READER_COLD feed_edge_(uint8_t c) {
    // A byte that starts or ends a frame clears the searched mark, so it cannot match a later partial frame
    this->held_end_ = 0;
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
      if (len < F::SHORTEST || len > N)
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
  // End of a held frame plus the bytes after it not fed yet; or, after flush() kept a partial frame, the pos_ it
  // searched
  pos_t held_end_{0};
  uint8_t buf_[N];
};

}  // namespace esphome

#undef ESPHOME_FRAME_READER_COLD
