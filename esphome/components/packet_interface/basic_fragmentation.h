#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PACKET_INTERFACE_BASIC_FRAGMENTATION

#include "packet_interface.h"

namespace esphome::packet_interface {

/**
 * A fragmentation layer: packets up to this layer's max_packet_size are split into fragments that fit the layer
 * below, and joined again at the other end.
 *
 * Each fragment starts with a 2 byte header: an 8-bit packet number, then the fragment's index in bits 0-6, with bit
 * 7 set on the last fragment. A packet that fits in one fragment still has the header, and is sent and passed up
 * without being copied.
 *
 * A packet is started only if the layer below can take all of its fragments now (get_send_capacity()), so a full
 * reliability queue does not leave the peer with part of a packet. The receiver joins one packet at a time, and drops
 * it when a fragment is missing or arrives with another packet number, or when the same source, or another one,
 * starts a new packet of several fragments. Other fragments from another source address are dropped, leaving the
 * packet being joined intact. Above a reliability layer, fragments arrive in order and none are missing.
 */
class BasicFragmentation : public PacketInterface {
 public:
  /// Packet number, then index and last flag.
  static constexpr size_t HEADER_SIZE = 2;
  /// The index has 7 bits.
  static constexpr size_t MAX_FRAGMENTS = 128;

  /// One fragment being sent, then the packet being joined.
  static constexpr size_t buffer_size(size_t lower_size, size_t max_packet_size) {
    return remaining_packet_size(lower_size, HEADER_SIZE) + max_packet_size;
  }

  /// @param buffer buffer_size() bytes; setup() fails if it is nullptr
  BasicFragmentation(PacketInterface *lower, size_t max_packet_size, uint8_t *buffer);

  void setup() override;
  void dump_config() override;
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) override;

 protected:
  bool send_(const PacketBuffer &data, const PacketMetaData &meta_data);
  void receive_(const PacketBuffer &data, const PacketMetaData &meta_data);
  size_t fragment_size_() const {
    size_t lower_size = this->lower_->get_max_packet_size();
    return lower_size > HEADER_SIZE ? lower_size - HEADER_SIZE : 0;
  }
  size_t fragments_for_(size_t size) const;

  PacketInterface *lower_;
  uint8_t *tx_buffer_;
  uint8_t *rx_buffer_{nullptr};
  uint16_t rx_length_{0};  // at most max_packet_size_, which configuration limits to 16384
  uint16_t rx_source_{0};
  uint8_t tx_number_{0};
  uint8_t rx_number_{0};
  uint8_t rx_next_index_{0};
  bool rx_joining_ : 1 {false};
  bool queue_too_short_ : 1 {false};
};

}  // namespace esphome::packet_interface
#endif
