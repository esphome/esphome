#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sx127x/sx127x.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include <vector>

namespace esphome {
namespace sx127x {

class SX127xTransport : public packet_transport::PacketTransport, public Parented<SX127x>, public SX127xListener {
 public:
  void setup() override;
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  bool should_send() override { return true; }
  size_t get_max_packet_size() override { return this->parent_->get_max_packet_size(); }
};

}  // namespace sx127x
}  // namespace esphome
