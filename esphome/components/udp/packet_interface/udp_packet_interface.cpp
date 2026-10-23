#include "udp_packet_interface.h"
#ifdef USE_NETWORK
#include <cstring>
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::udp {

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;

ESPHOME_LOG_TAG(TAG, "udp.packet_interface");

UdpPacketInterface::UdpPacketInterface(UDPComponent *udp_component, std::span<uint8_t> tx_buffer,
                                       std::array<uint8_t, 4> peer)
    : packet_interface::PacketInterface(tx_buffer.size()), parent_(udp_component), tx_buffer_(tx_buffer), peer_(peer) {
  if (this->peer_ != std::array<uint8_t, 4>{})
    this->capabilities_ = packet_interface::CAPABILITY_ADDRESSING;
}

void UdpPacketInterface::setup() {
  this->parent_->add_listener(
      [this](std::span<const uint8_t> data) { this->receive_(data, this->parent_->get_rx_source()); });
}

void UdpPacketInterface::receive_(std::span<const uint8_t> data, const sockaddr *source) {
  if (!this->accepts_from_(source)) {
    this->warn_dropped_(source);
    return;
  }
  if (data.size() > this->max_packet_size_) {
    ESP_LOGV(TAG, "%s: Packet of %zu bytes is larger than %zu, dropped", this->name_, data.size(),
             this->max_packet_size_);
    return;
  }
  this->on_receive_from_interface_(PacketBuffer(data.data(), data.size()), {});
}

bool UdpPacketInterface::accepts_from_(const sockaddr *source) const {
  if (!this->has_capability(packet_interface::CAPABILITY_ADDRESSING))
    return true;
  uint32_t address;
  return socket::sockaddr_to_ipv4(source, &address) && memcmp(&address, this->peer_.data(), sizeof(address)) == 0;
}

void UdpPacketInterface::warn_dropped_(const sockaddr *source) {
  static constexpr uint32_t WARNING_INTERVAL_MS = 60000;
  uint32_t now = App.get_loop_component_start_time();
  if (this->drop_warned_ && now - this->last_drop_warning_ms_ < WARNING_INTERVAL_MS)
    return;
  this->drop_warned_ = true;
  this->last_drop_warning_ms_ = now;
  char from[socket::SOCKADDR_STR_LEN];
  socket::format_sockaddr_to(source, sizeof(sockaddr_storage), from);
  ESP_LOGW(TAG,
           "%s: Packet from %s dropped, as only %u.%u.%u.%u is accepted; set point_to_point: false if that is a "
           "broadcast address",
           this->name_, from, this->peer_[0], this->peer_[1], this->peer_[2], this->peer_[3]);
}

bool UdpPacketInterface::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  size_t size = data.size();
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
    return false;
  }
  if (!network::is_connected())
    return false;
  if (data.is_contiguous()) {
    auto span = data.as_span();
    return this->parent_->send_packet(span.data(), span.size());
  }
  data.copy_to(this->tx_buffer_.data(), size);
  return this->parent_->send_packet(this->tx_buffer_.data(), size);
}

}  // namespace esphome::udp
#endif
