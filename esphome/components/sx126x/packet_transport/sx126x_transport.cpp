#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "sx126x_transport.h"

namespace esphome {
namespace sx126x {

static const char *const TAG = "sx126x_transport";

void SX126xTransport::setup() {
  PacketTransport::setup();
  this->parent_->register_listener(this);
}

void SX126xTransport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->transmit_packet(buf); }

void SX126xTransport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) { this->process_(packet); }

}  // namespace sx126x
}  // namespace esphome
