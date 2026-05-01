#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "udp_transport.h"

namespace esphome {
namespace udp {

static const char *const TAG = "udp_transport";

bool UDPTransport::should_send() { return network::is_connected(); }
void UDPTransport::setup() {
  PacketTransport::setup();
  if (!this->providers_.empty() || this->is_encrypted_()) {
    this->parent_->add_listener([this](std::span<const uint8_t> data) { this->process_(data); });
  }
}

void UDPTransport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->send_packet(buf); }
}  // namespace udp
}  // namespace esphome
