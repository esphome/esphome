#pragma once

#include "../espnow_component.h"

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"

#include <vector>

namespace esphome {
namespace espnow {

class ESPNowTransport : public packet_transport::PacketTransport,
                        public Parented<ESPNowComponent>,
                        public ESPNowReceivedPacketHandler,
                        public ESPNowBroadcastedHandler {
 public:
  void setup() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_broadcast_address(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
    this->broadcast_address_[0] = a;
    this->broadcast_address_[1] = b;
    this->broadcast_address_[2] = c;
    this->broadcast_address_[3] = d;
    this->broadcast_address_[4] = e;
    this->broadcast_address_[5] = f;
  }

  // ESPNow handler interface
  bool on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;
  bool on_broadcasted(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return ESP_NOW_MAX_DATA_LEN; }
  bool should_send() override;

  uint8_t broadcast_address_[ESP_NOW_ETH_ALEN]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};

}  // namespace espnow
}  // namespace esphome

#endif  // USE_ESP32