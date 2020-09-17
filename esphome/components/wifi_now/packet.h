#pragma once

#include <string>

#include "esphome/core/defines.h"
#include "esphome/core/component.h"

#include "simple_types.h"
#include "peer.h"

namespace esphome {
namespace wifi_now {

class WifiNowPacket {
 public:
  WifiNowPacket(const uint8_t* bssid, const uint8_t* packet, size_t length);

  WifiNowPacket(bssid_t bssid, servicekey_t servicekey, payload_t payload);

  const bssid_t get_bssid() const;
  const packetdata_t get_packetdata() const;
  const servicekey_t get_servicekey() const;
  const payload_t get_payload() const;

 protected:
  bssid_t bssid_;
  packetdata_t packetdata_;
};

}  // namespace wifi_now
}  // namespace esphome
