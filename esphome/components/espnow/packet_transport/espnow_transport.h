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
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_peer_address(peer_address_t address) {
    memcpy(this->peer_address_.data(), address.data(), ESP_NOW_ETH_ALEN);
  }

  // ESPNow handler interface
  bool on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;
  bool on_broadcasted(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return ESP_NOW_MAX_DATA_LEN; }
  bool should_send() override;

  peer_address_t peer_address_{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
  std::vector<uint8_t> packet_buffer_;
};

}  // namespace espnow
}  // namespace esphome

#endif  // USE_ESP32
