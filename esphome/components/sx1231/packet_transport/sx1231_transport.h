#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sx1231/sx1231.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include <vector>

namespace esphome::sx1231 {

class SX1231Transport final : public packet_transport::PacketTransport, public Parented<SX1231>, public SX1231Listener {
 public:
  void setup() override;
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  bool should_send() override { return true; }
  size_t get_max_packet_size() override { return this->parent_->get_max_packet_size(); }
};

}  // namespace esphome::sx1231
