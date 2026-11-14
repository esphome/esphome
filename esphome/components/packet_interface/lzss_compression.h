#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PACKET_INTERFACE_LZSS

#include "packet_interface.h"

namespace esphome::packet_interface {

/**
 * A compression layer: each packet is compressed on its own with LZSS, so a lost packet costs nothing more.
 *
 * Packets are short, so most of what can be saved comes from a preset dictionary: text that both ends hold, which a
 * packet may copy from as if it came just before the packet. For JSON, a typical packet makes a good dictionary.
 *
 * A packet starts with a header byte: bit 7 is set when the rest is compressed and clear when it is sent as it is,
 * because compressing did not make it smaller; bits 0-6 are a check of the dictionary, so a peer with another
 * dictionary drops the packet rather than passing up garbage.
 *
 * The compressed data is a stream of bits, the first in bit 7 of each byte:
 * - a 1 bit and 8 bits of a byte to copy;
 * - a 0 bit, an offset back from the current position, and a length of at least 2. The offset less 1 takes as many
 *   bits as the number of bytes behind the current position less 1 needs, counting the dictionary, so near the start
 *   of a packet it takes few bits. The length less 1 is an Elias gamma code: one 0 bit for each bit after the first,
 *   then the value.
 * The last byte is padded with 1 bits, which read as the start of a byte to copy with too few bits left.
 */
class LzssCompression : public PacketInterface {
 public:
  /// The bit flagging compressed data, and the dictionary check in the bits below it.
  static constexpr uint8_t HEADER_COMPRESSED = 0x80;
  static constexpr uint8_t HEADER_CHECK_MASK = 0x7F;
  static constexpr size_t HEADER_SIZE = 1;
  /// The largest packet the layer takes by default, over a layer below whose largest packet is \p lower_size.
  static constexpr size_t packet_size(size_t lower_size) { return remaining_packet_size(lower_size, HEADER_SIZE); }
  /// The compressor finds earlier positions starting with the same two bytes through a hash table of this many bits.
  static constexpr uint8_t HASH_BITS = 8;
  /// How many earlier positions the compressor tries for each match; more finds longer matches, but takes longer.
  static constexpr size_t MAX_CHAIN = 32;
  /// How far back the compressor looks for a match, a power of 2; the format allows any distance.
  static constexpr size_t WINDOW = 512;
  /// The latest position for each hash, then the previous one for each position in the window.
  static constexpr size_t CHAIN_WORDS = (size_t{1} << HASH_BITS) + WINDOW;
  /// The packet to compress, the compressed packet, and an expanded packet.
  static constexpr size_t buffer_size(size_t lower_size, size_t max_packet_size) {
    return 2 * (max_packet_size != 0 ? max_packet_size : packet_size(lower_size)) + lower_size;
  }

  /**
   * @param max_packet_size The largest packet before compression, or 0 for what the layer below takes less the
   * header; a packet larger than that is refused when it does not compress enough
   * @param dictionary The preset dictionary, in flash; may be null when \p dictionary_size is 0
   * @param check The dictionary check sent in each header
   * @param buffers buffer_size() bytes, and \p chains CHAIN_WORDS words; setup() fails if either is nullptr
   */
  LzssCompression(PacketInterface *lower, size_t max_packet_size, const uint8_t *dictionary, size_t dictionary_size,
                  uint8_t check, uint8_t *buffers, uint16_t *chains);

  void setup() override;
  void dump_config() override;
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) override;

  /// Compress \p size bytes into \p out, which holds \p capacity bytes; returns the length, or 0 if it does not fit.
  size_t compress(const uint8_t *in, size_t size, uint8_t *out, size_t capacity);
  /// Expand \p size bytes from \p data, starting at \p offset, into \p out; returns the length, or SIZE_MAX if the
  /// data is malformed or longer than \p capacity.
  size_t expand(const PacketBuffer &data, size_t offset, size_t size, uint8_t *out, size_t capacity) const;

 protected:
  void receive_(const PacketBuffer &data, const PacketMetaData &meta_data);
  bool send_(const PacketBuffer &data, const PacketMetaData &meta_data);
  /// The byte at \p pos counting from the start of the dictionary, with \p data following it.
  uint8_t byte_at_(const uint8_t *data, size_t pos) const;
  /// Adds \p pos, which has \p total bytes from the start of the dictionary up to the end of the packet, to the chains.
  void insert_(const uint8_t *in, size_t pos, size_t total);
  size_t hash_(const uint8_t *in, size_t pos) const {
    return ((this->byte_at_(in, pos) << 8 | this->byte_at_(in, pos + 1)) * 0x9E3779B1u) >> (32 - HASH_BITS);
  }

  // First, so it fills the spare byte at the end of PacketInterface
  const uint8_t check_;
  PacketInterface *lower_;
  const uint8_t *dictionary_;
  const size_t dictionary_size_;
  uint8_t *tx_input_;
  uint8_t *tx_output_{nullptr};  // set once setup() succeeds
  uint8_t *rx_output_{nullptr};
  // For each hash, the latest position with it; for each of the last WINDOW positions, the one before it with the same
  // hash
  uint16_t *head_;
  uint16_t *previous_{nullptr};
};

}  // namespace esphome::packet_interface
#endif
