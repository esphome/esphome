#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PACKET_INTERFACE_XXTEA
#include <array>
#include <initializer_list>

#include "esphome/core/preferences.h"
#include "packet_interface.h"

namespace esphome::packet_interface {

/**
 * An encryption layer that encrypts and authenticates each packet with XXTEA and a 256-bit shared key.
 *
 * The packet is padded with zeros and followed by a trailer, then the whole is encrypted as one block. With the
 * rolling code enabled, the trailer starts with:
 * - a 32-bit boot number, saved in flash and increased at every boot;
 * - a 32-bit packet counter, starting at 0 at every boot;
 * - the highest boot number heard from the peer, echoed back to it.
 * The trailer always ends with 3 fixed check bytes and the number of padding bytes. XXTEA mixes the whole block, so
 * a packet that was changed or encrypted with another key fails the check.
 * With the rolling code, a packet is dropped as a replay unless it echoes this device's current boot number, and its
 * boot number and counter are higher than those of the last packet accepted from the peer. A boot number is never
 * reused, so the echo acts as a fresh challenge after every restart.
 */
class XxteaEncryption : public PacketInterface {
 public:
  /// The check bytes and the padding count.
  static constexpr size_t CHECK_SIZE = 4;
  /// Boot number, counter, and the echo of the receiver's boot number.
  static constexpr size_t ROLLING_SIZE = 12;

  static constexpr size_t trailer_size(bool rolling_code) {
    return rolling_code ? ROLLING_SIZE + CHECK_SIZE : CHECK_SIZE;
  }
  // XXTEA works on whole 32-bit words
  static constexpr size_t packet_size(size_t lower_size, bool rolling_code) {
    return remaining_packet_size(lower_size & ~size_t{3}, trailer_size(rolling_code));
  }
  static constexpr size_t buffer_words(size_t lower_size) { return lower_size / 4 * 2; }

  /// @param buffers buffer_words() words for the send and receive buffers; setup() fails if it is nullptr
  XxteaEncryption(PacketInterface *lower, bool rolling_code, uint32_t *buffers);

  void setup() override;
  void dump_config() override;
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) override;

  void set_key(std::initializer_list<uint32_t> key);

 protected:
  bool send_(const PacketBuffer &data, const PacketMetaData &meta_data);
  void receive_(const PacketBuffer &data, const PacketMetaData &meta_data);
  bool accept_counter_(uint32_t boot, uint32_t counter);
  void next_boot_();

  PacketInterface *lower_;
  std::array<uint32_t, 8> key_{};
  // Separate buffers, as a layer above may send a reply while a received packet is being passed up; tx_buffer_ is set
  // only once setup() succeeds
  uint32_t *tx_buffer_{nullptr};
  uint32_t *rx_buffer_;
  ESPPreferenceObject pref_;
  uint32_t boot_{0};
  uint32_t counter_{0};
  uint32_t peer_boot_{0};
  uint32_t peer_counter_{0};
  uint32_t echo_boot_{0};  // highest boot number heard from the peer
  bool peer_seen_{false};
  bool old_boot_warned_{false};
  const bool rolling_code_;
};

}  // namespace esphome::packet_interface
#endif
