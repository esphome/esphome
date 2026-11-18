#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

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
      if (pos != 0) {
        this->buf_[pos] = c;
        this->replay_(pos);
      }
      return nullptr;
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
  // Start over and feed buf_[1..last] again; never completes a frame as last < N. Kept out of line so feed() can be
  // inlined into the caller's byte loop.
  void __attribute__((noinline)) replay_(size_t last) {
    this->pos_ = 0;
    for (size_t i = 1; i <= last; i++) {
      this->feed(this->buf_[i]);
    }
  }

  uint8_t buf_[N];
  pos_t pos_{0};
};

/// Collects binary frames that start with the header bytes HEADER... and give their own length: the byte at LEN_POS,
/// plus LEN_ADD, is the length of the whole frame, header included. A length that ends the frame before the length
/// byte, or is longer than N, is not a frame: the reader looks for a header after its first byte. Checksums, and a
/// minimum length that covers them, are left to the caller.
template<size_t N, size_t LEN_POS, size_t LEN_ADD, uint8_t... HEADER> class LengthFrameReader {
  using pos_t = std::conditional_t<(N <= 0xFF), uint8_t, uint16_t>;
  static constexpr size_t HEADER_LEN = sizeof...(HEADER);
  static_assert(HEADER_LEN >= 1 && HEADER_LEN <= 8, "LengthFrameReader takes 1 to 8 header bytes");
  static_assert(HEADER_LEN <= LEN_POS && LEN_POS < N && N <= 0xFFFF, "LengthFrameReader size out of range");

 public:
  /// Feed one byte; returns the completed frame, header included, valid until the next call, or nullptr.
  /// frame_length() gives its length.
  const uint8_t *feed(uint8_t c) {
    pos_t pos = this->pos_;
    if (pos < HEADER_LEN && c != frame_reader_detail::header_byte<HEADER...>(pos)) {
      if (pos != 0) {
        this->buf_[pos] = c;
        this->replay_(pos);
      }
      return nullptr;
    }
    this->buf_[pos++] = c;
    if (pos > LEN_POS) {
      const size_t len = this->frame_length();
      if (pos == LEN_POS + 1 && (len <= LEN_POS || len > N)) {
        // Never completes a frame: a frame is longer than the LEN_POS bytes replayed
        this->replay_(LEN_POS);
        return nullptr;
      }
      if (pos == len) {
        this->pos_ = 0;
        return this->buf_;
      }
    }
    this->pos_ = pos;
    return nullptr;
  }

  /// Length given by the length byte; for the frame feed() returned, valid until the next call.
  size_t frame_length() const { return this->buf_[LEN_POS] + LEN_ADD; }

  /// Number of bytes of the partial frame; 0 after a frame is returned and while looking for a header.
  size_t length() const { return this->pos_; }

  /// Forget the partial frame and look for a header from the next byte.
  void reset() { this->pos_ = 0; }

  /// For a frame the caller rejected (too short for its checksum, or a bad one), or a partial frame left too long
  /// without a byte: drop its first byte and realign on a header in the rest. Call right after feed() returned that
  /// frame, or while length() is not 0. Returns a complete frame found in the rest, like feed(), dropping the bytes
  /// after it; call again if that frame is rejected too.
  const uint8_t *resync() {
    const size_t len = this->pos_ != 0 ? this->pos_ : this->frame_length();
    // A length feed() rejected, or garbage when nothing was collected, has nothing to replay
    if (len <= 1 || len > N)
      return nullptr;
    return this->replay_(len - 1);
  }

 protected:
  // Start over and feed buf_[1..last] again, stopping at a completed frame; writes stay behind the read position.
  // Kept out of line so feed() can be inlined into the caller's byte loop.
  const uint8_t *__attribute__((noinline)) replay_(size_t last) {
    this->pos_ = 0;
    for (size_t i = 1; i <= last; i++) {
      if (this->feed(this->buf_[i]) != nullptr)
        return this->buf_;
    }
    return nullptr;
  }

  uint8_t buf_[N];
  pos_t pos_{0};
};

}  // namespace esphome
