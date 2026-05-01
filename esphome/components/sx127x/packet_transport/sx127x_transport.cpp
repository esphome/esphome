#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "sx127x_transport.h"

namespace esphome {
namespace sx127x {

static const char *const TAG = "sx127x_transport";

void SX127xTransport::setup() {
  PacketTransport::setup();
  this->parent_->register_listener(this);
}

void SX127xTransport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->transmit_packet(buf); }

void SX127xTransport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) { this->process_(packet); }

}  // namespace sx127x
}  // namespace esphome
