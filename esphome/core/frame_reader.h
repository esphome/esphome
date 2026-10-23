#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace esphome {

/// Collects fixed length binary frames of N bytes that start with the header bytes HEADER...; the binary twin of
/// LineReader. Bytes before a header are skipped; checksums are left to the caller.
template<size_t N, uint8_t... HEADER> class FrameReader {
  using pos_t = std::conditional_t<(N <= 0xFF), uint8_t, uint16_t>;
  static constexpr size_t HEADER_LEN = sizeof...(HEADER);
  static_assert(HEADER_LEN >= 1, "FrameReader needs at least one header byte");
  static_assert(HEADER_LEN < N && N <= 0xFFFF, "FrameReader size out of range");

 public:
  /// Feed one byte; returns the completed frame, header included, valid until the next call, or nullptr.
  const uint8_t *feed(uint8_t c) {
    pos_t pos = this->pos_;
    if (pos < HEADER_LEN && c != header_byte(pos)) {
      if constexpr (HEADER_LEN > 1) {
        // A later byte of the broken header, or c, may start the real one
        this->buf_[pos] = c;
        this->replay_(pos);
      } else {
        this->pos_ = 0;
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
  void resync() { this->replay_(N - 1); }

 protected:
  // A fold, not an array, so no header table lands in RAM on ESP8266
  static constexpr uint8_t header_byte(size_t i) {
    size_t j = 0;
    uint8_t r = 0;
    ((r = (j++ == i) ? HEADER : r), ...);
    return r;
  }

  // Start over and feed buf_[1..last] again; never completes a frame as last < N
  void replay_(size_t last) {
    this->pos_ = 0;
    for (size_t i = 1; i <= last; i++) {
      this->feed(this->buf_[i]);
    }
  }

  uint8_t buf_[N];
  pos_t pos_{0};
};

}  // namespace esphome
