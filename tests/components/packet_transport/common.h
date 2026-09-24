#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <gtest/gtest.h>
#include "esphome/components/packet_transport/packet_transport.h"

namespace esphome::packet_transport::testing {

// Protocol constants mirrored from packet_transport.cpp for test packet construction.
static constexpr uint16_t MAGIC_NUMBER = 0x4553;
static constexpr uint16_t MAGIC_PING = 0x5048;

// Concrete testable implementation of PacketTransport.
// Captures sent packets and exposes protected members for verification.
//
// Sensor round-trip tests require USE_SENSOR / USE_BINARY_SENSOR to be defined,
// which happens when 'sensor' and 'binary_sensor' components are in the build.
// Run with --all or include those components to enable the full test suite.
class TestablePacketTransport : public PacketTransport {
 public:
  using PacketTransport::add_key_;
  using PacketTransport::data_;
  using PacketTransport::encryption_key_;
  using PacketTransport::flush_;
  using PacketTransport::header_;
  using PacketTransport::increment_code_;
  using PacketTransport::init_data_;
  using PacketTransport::is_encrypted_;
  using PacketTransport::is_provider_;
  using PacketTransport::name_;
  using PacketTransport::ping_key_;
  using PacketTransport::ping_keys_;
  using PacketTransport::ping_pong_enable_;
  using PacketTransport::ping_pong_recyle_time_;
  using PacketTransport::process_;
  using PacketTransport::providers_;
  using PacketTransport::rolling_code_;
  using PacketTransport::rolling_code_enable_;
  using PacketTransport::send_data_;
  using PacketTransport::updated_;
#ifdef USE_SENSOR
  using PacketTransport::add_data_;
  using PacketTransport::remote_sensors_;
  using PacketTransport::sensors_;
#endif
#ifdef USE_BINARY_SENSOR
  using PacketTransport::add_binary_data_;
  using PacketTransport::binary_sensors_;
  using PacketTransport::remote_binary_sensors_;
#endif

  // NOTE: std::vector is used here for test convenience. For production code,
  // consider using StaticVector or FixedVector from esphome/core/helpers.h instead.
  mutable std::vector<std::vector<uint8_t>> sent_packets;
  size_t max_packet_size{512};
  bool send_enabled{true};

  void send_packet(const std::vector<uint8_t> &buf) const override { this->sent_packets.push_back(buf); }
  size_t get_max_packet_size() override { return this->max_packet_size; }
  bool should_send() override { return this->send_enabled; }

  /// Build the packet header for testing without requiring App or global_preferences.
  void init_for_test(const char *name) {
    this->name_ = name;
    this->header_.clear();
    // MAGIC_NUMBER as uint16_t little-endian
    this->header_.push_back(MAGIC_NUMBER & 0xFF);
    this->header_.push_back((MAGIC_NUMBER >> 8) & 0xFF);
    // Length-prefixed hostname
    auto len = strlen(name);
    this->header_.push_back(static_cast<uint8_t>(len));
    for (size_t i = 0; i < len; i++)
      this->header_.push_back(name[i]);
    // Pad to 4-byte boundary
    while (this->header_.size() & 0x3)
      this->header_.push_back(0);
  }
};

/// Build a MAGIC_PING packet for testing add_key_ / ping-pong flows.
inline std::vector<uint8_t> build_ping_packet(const char *hostname, uint32_t key) {
  std::vector<uint8_t> packet;
  packet.push_back(MAGIC_PING & 0xFF);
  packet.push_back((MAGIC_PING >> 8) & 0xFF);
  auto len = strlen(hostname);
  packet.push_back(static_cast<uint8_t>(len));
  for (size_t i = 0; i < len; i++)
    packet.push_back(hostname[i]);
  packet.push_back(key & 0xFF);
  packet.push_back((key >> 8) & 0xFF);
  packet.push_back((key >> 16) & 0xFF);
  packet.push_back((key >> 24) & 0xFF);
  return packet;
}

}  // namespace esphome::packet_transport::testing
