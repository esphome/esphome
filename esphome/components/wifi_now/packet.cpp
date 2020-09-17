#include <algorithm>

#include "packet.h"

namespace esphome {
namespace wifi_now {

static const char* TAG = "wifi_now.packet";

WifiNowPacket::WifiNowPacket(const uint8_t* bssid, const uint8_t* packetdata, size_t length) : packetdata_(length) {
  std::copy_n(bssid, bssid_.size(), bssid_.begin());
  std::copy_n(packetdata, length, packetdata_.begin());
}

WifiNowPacket::WifiNowPacket(bssid_t bssid, servicekey_t servicekey, payload_t payload)
    : bssid_(bssid), packetdata_(sizeof(packet_t) + payload.size()) {
  auto packet = (packet_t*) packetdata_.data();

  std::copy(servicekey.cbegin(), servicekey.cend(), packet->servicekey);
  std::copy(payload.cbegin(), payload.cend(), packet->payload);
}

const bssid_t WifiNowPacket::get_bssid() const { return bssid_; }

const packetdata_t WifiNowPacket::get_packetdata() const { return packetdata_; }

const servicekey_t WifiNowPacket::get_servicekey() const {
  if (packetdata_.size() < sizeof(packet_t)) {
    servicekey_t result;
    result.fill(0);
    return result;
  } else {
    auto packet = (packet_t*) packetdata_.data();
    servicekey_t result;
    std::copy_n(packet->servicekey, result.size(), result.begin());
    return result;
  }
}

const payload_t WifiNowPacket::get_payload() const {
  if (packetdata_.size() < sizeof(packet_t)) {
    return payload_t();
  } else {
    return payload_t(packetdata_.cbegin() + sizeof(packet_t), packetdata_.cend());
  }
}

}  // namespace wifi_now
}  // namespace esphome
