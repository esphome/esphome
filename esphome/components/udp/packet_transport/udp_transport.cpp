#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "udp_transport.h"

namespace esphome {
namespace udp {

static const char *const TAG = "udp_transport";

bool UDPTransport::should_send() { return this->should_broadcast_ && network::is_connected(); }
void UDPTransport::setup() {
  PacketTransport::setup();
  this->should_broadcast_ = this->ping_pong_enable_;
#ifdef USE_SENSOR
  this->should_broadcast_ |= !this->sensors_.empty();
#endif
#ifdef USE_BINARY_SENSOR
  this->should_broadcast_ |= !this->binary_sensors_.empty();
#endif
  if (this->should_broadcast_)
    this->parent_->set_should_broadcast();
  if (!this->providers_.empty() || this->is_encrypted_()) {
    this->parent_->add_listener([this](std::vector<uint8_t> &buf) { this->process_(buf); });
  }
}

void UDPTransport::update() {
  PacketTransport::update();
  this->updated_ = true;
  this->resend_data_ = this->should_broadcast_;
}

void UDPTransport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->send_packet(buf); }
}  // namespace udp
}  // namespace esphome
