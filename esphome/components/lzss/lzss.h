#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::lzss {

/**
 * LZSS compression of a block of data on its own, with a preset dictionary: text that both ends hold, which the
 * block may copy from as if it came just before the block. Short blocks gain most from a dictionary that looks like
 * them.
 *
 * The compressed data is a stream of bits, the first in bit 7 of each byte:
 * - a 1 bit and 8 bits of a byte to copy;
 * - a 0 bit, an offset back from the current position, and a length of at least 2. The offset less 1 takes as many
 *   bits as the number of bytes behind the current position less 1 needs, counting the dictionary, so near the start
 *   of a block it takes few bits. The length less 1 is an Elias gamma code: one 0 bit for each bit after the first,
 *   then the value.
 * The last byte is padded with 1 bits, which read as the start of a byte to copy with too few bits left.
 */
class Lzss {
 public:
  /// The compressor finds earlier positions starting with the same two bytes through a hash table of this many bits.
  static constexpr uint8_t HASH_BITS = 8;
  /// How many earlier positions the compressor tries for each match; more finds longer matches, but takes longer.
  static constexpr size_t MAX_CHAIN = 32;
  /// How far back the compressor looks for a match, a power of 2; the format allows any distance.
  static constexpr size_t WINDOW = 512;
  /// The working space compress() needs: the latest position for each hash, then the previous one for each position
  /// in the window.
  static constexpr size_t CHAIN_WORDS = (size_t{1} << HASH_BITS) + WINDOW;

  /// @param dictionary The preset dictionary, in flash; may be null when \p dictionary_size is 0
  Lzss(const uint8_t *dictionary, size_t dictionary_size)
      : dictionary_(dictionary), dictionary_size_(dictionary_size) {}

  size_t dictionary_size() const { return this->dictionary_size_; }

  /// Compress \p size bytes into \p out, which holds \p capacity bytes, using \p chains (CHAIN_WORDS words) as working
  /// space; returns the length, or 0 if it does not fit.
  size_t compress(const uint8_t *in, size_t size, uint8_t *out, size_t capacity, uint16_t *chains) const;
  /// Expand \p size bytes into \p out, which holds \p capacity bytes; returns the length, or SIZE_MAX if the data is
  /// malformed or expands to more than \p capacity.
  size_t expand(const uint8_t *in, size_t size, uint8_t *out, size_t capacity) const;

 protected:
  /// The byte at \p pos counting from the start of the dictionary, with \p data following it.
  uint8_t byte_at_(const uint8_t *data, size_t pos) const;

  const uint8_t *dictionary_;
  size_t dictionary_size_;
};

}  // namespace esphome::lzss
