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
  if (data.is_contiguous()) {
    this->process_(data.as_span());
    return;
  }
  size_t max_size = this->interface_->get_max_packet_size();
  if (data.size() > max_size) {
    ESP_LOGW(TAG, "Received packet of %zu bytes is larger than %zu", data.size(), max_size);
    return;
  }
  if (!this->rx_copy_)
    this->rx_copy_ = std::make_unique<uint8_t[]>(max_size);
  size_t len = data.copy_to(this->rx_copy_.get(), max_size);
  this->process_(std::span<const uint8_t>(this->rx_copy_.get(), len));
}

void PacketInterfaceTransport::send_packet(const std::vector<uint8_t> &buf) const {
  if (!this->interface_->send_to_interface(PacketBuffer(buf), {}))
    ESP_LOGD(TAG, "Failed to send packet of %zu bytes", buf.size());
}

}  // namespace esphome::packet_interface
