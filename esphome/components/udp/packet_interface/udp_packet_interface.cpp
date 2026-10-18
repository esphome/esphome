#include "udp_packet_interface.h"
#ifdef USE_NETWORK
#include <array>
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"

namespace esphome::udp {

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;

ESPHOME_LOG_TAG(TAG, "udp.packet_interface");

void UdpPacketInterface::setup() {
  this->parent_->add_listener([this](std::span<const uint8_t> buf) {
    this->on_receive_from_interface_(PacketBuffer(buf.data(), buf.size()), {});
  });
}

bool UdpPacketInterface::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  if (!network::is_connected())
    return false;
  if (data.is_contiguous()) {
    auto span = data.as_span();
    return this->parent_->send_packet(span.data(), span.size());
  }
  if (data.size() > MAX_PACKET_SIZE) {
    ESP_LOGW(TAG, "Packet of %zu bytes is larger than %zu", data.size(), MAX_PACKET_SIZE);
    return false;
  }
  std::array<uint8_t, MAX_PACKET_SIZE> buf;
  size_t len = data.copy_to(buf.data(), buf.size());
  return this->parent_->send_packet(buf.data(), len);
}

}  // namespace esphome::udp
#endif
