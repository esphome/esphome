#include "packet_interface_transport.h"
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface.transport");

void PacketInterfaceTransport::setup() {
  PacketTransport::setup();
  this->interface_ = this->interface_->top();
  this->interface_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &) { this->receive_(data); });
}

void PacketInterfaceTransport::receive_(const PacketBuffer &data) {
  // Packet interface layers deliver packets in one piece
  if (!data.is_contiguous()) {
    ESP_LOGW(TAG, "Packet split across buffers is not supported");
    return;
  }
  this->process_(data.as_span());
}

void PacketInterfaceTransport::send_packet(const std::vector<uint8_t> &buf) const {
  if (!this->interface_->send_to_interface(PacketBuffer(buf), {}))
    ESP_LOGD(TAG, "Failed to send packet of %zu bytes", buf.size());
}

}  // namespace esphome::packet_interface
