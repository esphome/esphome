#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "sx126x_transport.h"

namespace esphome::sx126x {

ESPHOME_LOG_TAG(TAG, "sx126x_transport");

void SX126xTransport::setup() {
  PacketTransport::setup();
  this->parent_->register_listener(this);
}

void SX126xTransport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->transmit_packet(buf); }

bool SX126xTransport::try_send_packet(const std::vector<uint8_t> &buf) const {
  return this->parent_->transmit_packet(buf) == SX126xError::NONE;
}

void SX126xTransport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) { this->process_(packet); }

}  // namespace esphome::sx126x
