#include "udp_packet_interface.h"
#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"

namespace esphome::udp {

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;

ESPHOME_LOG_TAG(TAG, "udp.packet_interface");

void UdpPacketInterface::setup() {
  this->parent_->add_listener([this](std::span<const uint8_t> data) { this->receive_(data); });
}

void UdpPacketInterface::receive_(std::span<const uint8_t> data) {
  if (data.size() > this->max_packet_size_) {
    ESP_LOGV(TAG, "%s: Packet of %zu bytes is larger than %zu, dropped", this->name_, data.size(),
             this->max_packet_size_);
    return;
  }
  this->on_receive_from_interface_(PacketBuffer(data.data(), data.size()), {});
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
