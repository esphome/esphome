#pragma once

#include "../espnow_component.h"

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include <cstring>
#include <span>

#include "esphome/components/packet_interface/packet_interface.h"

namespace esphome::espnow {

/**
 * A packet interface link over ESP-NOW.
 *
 * With a peer address other than broadcast, packets go to that device only, and only packets sent to this device by
 * it are received, so the link provides the addressing capability itself. With the broadcast address, packets go to
 * every device on the channel and every broadcast packet is received, from known and unknown senders; an addressing
 * layer then tells the devices apart. ESP8266 does not report a packet's destination, so there a broadcast link also
 * receives packets sent to this device, and a peer link also receives the peer's broadcasts.
 *
 * ESP-NOW queues each packet and reports later whether it was delivered; a reliability layer above resends what is
 * lost.
 */
class ESPNowPacketInterface : public packet_interface::PacketInterface,
                              public ESPNowReceivedPacketHandler,
                              public ESPNowBroadcastHandler,
                              public ESPNowUnknownPeerHandler {
 public:
  /// @param tx_buffer Joins packets split across buffers; its size is the largest packet, and it must outlive the link
  ESPNowPacketInterface(ESPNowComponent *parent, std::span<uint8_t> tx_buffer, peer_address_t peer_address);

  void setup() override;
  void dump_config() override;
  bool send_to_interface(const packet_interface::PacketBuffer &data,
                         const packet_interface::PacketMetaData &meta_data) override;

  bool on_receive(const ESPNowRecvInfo &info, const uint8_t *data, uint16_t size) override;
  bool on_broadcast(const ESPNowRecvInfo &info, const uint8_t *data, uint16_t size) override;
  bool on_unknown_peer(const ESPNowRecvInfo &info, const uint8_t *data, uint16_t size) override;

 protected:
  bool is_broadcast_() const {
    return memcmp(this->peer_address_.data(), ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0;
  }
  void receive_(const ESPNowRecvInfo &info, const uint8_t *data, uint16_t size);

  ESPNowComponent *parent_;
  peer_address_t peer_address_;
  const std::span<uint8_t> tx_buffer_;
};

}  // namespace esphome::espnow

#endif  // USE_ESP32 || USE_ESP8266
