#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PACKET_INTERFACE_XXTEA
#include <array>
#include <initializer_list>

#include "esphome/core/preferences.h"
#include "packet_interface.h"

namespace esphome::packet_interface {

/// Whether and how the trailer carries a rolling code that rejects replayed packets.
enum class RollingCode : uint8_t {
  ROLLING_CODE_NONE,
  /// The trailer echoes the peer's boot number, so packets need to flow both ways.
  ROLLING_CODE_ECHO,
  /// No echo, for one sender heard by many receivers.
  ROLLING_CODE_BROADCAST,
};

/**
 * An encryption layer that encrypts and authenticates each packet with XXTEA and a 256-bit shared key.
 *
 * The packet is padded with zeros and followed by a trailer, then the whole is encrypted as one block. With the
 * rolling code enabled, the trailer starts with:
 * - a 32-bit boot number, saved in flash and increased at every boot;
 * - a 32-bit packet counter, starting at 0 at every boot;
 * - in echo mode, the highest boot number heard from the peer, echoed back to it.
 * The trailer always ends with 3 fixed check bytes and the number of padding bytes. XXTEA mixes the whole block, so
 * a packet that was changed or encrypted with another key fails the check.
 * With the rolling code, a packet is dropped as a replay unless its boot number and counter are higher than those of
 * the last packet accepted from the peer. In echo mode it must also echo this device's current boot number. A boot
 * number is never reused, so the echo acts as a fresh challenge after every restart.
 *
 * In broadcast mode there is no echo, so nothing stops a recorded packet being replayed after this device restarts
 * and forgets the peer's counter. With save_peer_boot, the peer's boot number is saved in flash, so only packets
 * from the peer's current boot can be replayed, and only until a newer packet is heard. Without it, any packet is
 * accepted first after a restart. reset_peer() forgets the saved boot number, for a peer whose flash was erased.
 */
class XxteaEncryption : public PacketInterface {
 public:
  XxteaEncryption(PacketInterface *lower, RollingCode rolling_code);

  void setup() override;
  void dump_config() override;
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) override;

  void set_key(std::initializer_list<uint32_t> key);
  /// In broadcast mode, keep the peer's boot number in flash so a restart does not accept older packets.
  void set_save_peer_boot(bool save) { this->save_peer_boot_ = save; }
  /// Accept the next packet from the peer whatever its boot number and counter, as after a restart of the peer
  /// that lost its own saved boot number.
  void reset_peer();

 protected:
  void receive_(const PacketBuffer &data, const PacketMetaData &meta_data);
  bool accept_counter_(uint32_t boot, uint32_t counter);
  void next_boot_();
  bool has_rolling_code_() const { return this->rolling_code_ != RollingCode::ROLLING_CODE_NONE; }

  PacketInterface *lower_;
  const size_t trailer_size_;
  std::array<uint32_t, 8> key_{};
  // Separate buffers, as a layer above may send a reply while a received packet is being passed up; both are in one
  // block allocated in setup(), and tx_buffer_ stays null if that fails
  uint32_t *tx_buffer_{nullptr};
  uint32_t *rx_buffer_{nullptr};
  ESPPreferenceObject pref_;
  ESPPreferenceObject peer_pref_;
  uint32_t boot_{0};
  uint32_t counter_{0};
  uint32_t peer_boot_{0};
  uint32_t peer_counter_{0};
  uint32_t saved_peer_boot_{0};
  uint32_t echo_boot_{0};  // highest boot number heard from the peer
  bool peer_seen_{false};
  bool old_boot_warned_{false};
  bool save_peer_boot_{false};
  const RollingCode rolling_code_;
};

}  // namespace esphome::packet_interface
#endif
