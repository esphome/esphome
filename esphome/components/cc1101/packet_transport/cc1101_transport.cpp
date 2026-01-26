#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "cc1101_transport.h"

namespace esphome {
namespace cc1101 {

static const char *const TAG = "cc1101_transport";

void CC1101Transport::setup() {
  PacketTransport::setup();
  this->parent_->register_listener(this);
}

void CC1101Transport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->transmit_packet(buf); }

void CC1101Transport::on_packet(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) {
  this->process_(packet);
}

}  // namespace cc1101
}  // namespace esphome
