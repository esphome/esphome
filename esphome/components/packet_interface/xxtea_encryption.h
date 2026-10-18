#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PACKET_INTERFACE_XXTEA
#include <array>
#include <initializer_list>
#include <memory>

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
  XxteaEncryption(PacketInterface *lower, bool rolling_code);

  void setup() override;
  void dump_config() override;
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) override;

  void set_key(std::initializer_list<uint32_t> key);

 protected:
  void receive_(const PacketBuffer &data, const PacketMetaData &meta_data);
  bool accept_counter_(uint32_t boot, uint32_t counter);
  void next_boot_();

  PacketInterface *lower_;
  const size_t trailer_size_;
  std::array<uint32_t, 8> key_{};
  // Separate buffers, as a layer above may send a reply while a received packet is being passed up
  std::unique_ptr<uint32_t[]> tx_buffer_;
  std::unique_ptr<uint32_t[]> rx_buffer_;
  ESPPreferenceObject pref_;
  uint32_t boot_{0};
  uint32_t counter_{0};
  uint32_t peer_boot_{0};
  uint32_t peer_counter_{0};
  uint32_t echo_boot_{0};  // highest boot number heard from the peer
  bool peer_seen_{false};
  const bool rolling_code_;
};

}  // namespace esphome::packet_interface
#endif
