#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PACKET_INTERFACE_LZSS

#include "esphome/components/lzss/lzss.h"
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
 * dictionary drops the packet rather than passing up garbage. The compressed data is in the format of lzss::Lzss.
 */
class LzssCompression final : public PacketInterface {
 public:
  /// The bit flagging compressed data, and the dictionary check in the bits below it.
  static constexpr uint8_t HEADER_COMPRESSED = 0x80;
  static constexpr uint8_t HEADER_CHECK_MASK = 0x7F;
  static constexpr size_t HEADER_SIZE = 1;
  /// The largest packet the layer takes by default, over a layer below whose largest packet is \p lower_size.
  static constexpr size_t packet_size(size_t lower_size) { return remaining_packet_size(lower_size, HEADER_SIZE); }
  static constexpr size_t CHAIN_WORDS = lzss::Lzss::CHAIN_WORDS;
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
  // One compressed packet per packet, so a fragmentation layer above sees the room below
  size_t get_send_capacity() const override { return this->lower_->get_send_capacity(); }

 protected:
  void receive_(const PacketBuffer &data, const PacketMetaData &meta_data);
  bool send_(const PacketBuffer &data, const PacketMetaData &meta_data);

  // First, so it fills the spare byte at the end of PacketInterface
  const uint8_t check_;
  PacketInterface *lower_;
  const lzss::Lzss codec_;
  uint8_t *tx_input_;
  uint8_t *tx_output_{nullptr};  // set once setup() succeeds
  uint8_t *rx_output_{nullptr};
  uint16_t *chains_;
};

}  // namespace esphome::packet_interface
#endif
