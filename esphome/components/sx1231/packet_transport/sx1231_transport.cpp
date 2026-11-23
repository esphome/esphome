#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "sx1231_transport.h"
#include <algorithm>
#include <cstring>

namespace esphome::sx1231 {

ESPHOME_LOG_TAG(TAG, "sx1231_transport");

void SX1231Transport::setup() {
  PacketTransport::setup();
  this->parent_->register_listener(this);
}

void SX1231Transport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->transmit_packet(buf); }

void SX1231Transport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  // process_() decrypts in place, and the radio's other listeners and on_packet get the same packet
  alignas(uint32_t) uint8_t copy[SX1231_MAX_PACKET_SIZE];
  const size_t size = std::min(packet.size(), sizeof(copy));
  memcpy(copy, packet.data(), size);
  this->process_(std::span<const uint8_t>(copy, size));
}

}  // namespace esphome::sx1231
