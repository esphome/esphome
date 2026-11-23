#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "sx1231_transport.h"

namespace esphome::sx1231 {

ESPHOME_LOG_TAG(TAG, "sx1231_transport");

void SX1231Transport::setup() {
  PacketTransport::setup();
  this->parent_->register_listener(this);
}

void SX1231Transport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->transmit_packet(buf); }

void SX1231Transport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) { this->process_(packet); }

}  // namespace esphome::sx1231
