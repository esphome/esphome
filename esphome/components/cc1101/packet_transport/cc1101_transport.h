#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cc1101/cc1101.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include <vector>

namespace esphome {
namespace cc1101 {

class CC1101Transport : public packet_transport::PacketTransport,
                        public Parented<CC1101Component>,
                        public CC1101Listener {
 public:
  void setup() override;
  void on_packet(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  bool should_send() override { return true; }
  size_t get_max_packet_size() override { return this->parent_->get_max_packet_size(); }
};

}  // namespace cc1101
}  // namespace esphome
