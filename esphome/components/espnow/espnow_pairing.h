#pragma once

#if defined(USE_ESP32X)

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/espnow/espnow_component.h"
#include "esphome/core/preferences.h"

#include <array>
#include <memory>
#include <queue>
#include <vector>
#include <mutex>
#include <map>

namespace esphome::espnow_pairing {

class ESPNowPairing : public pollingcomponent,
                      public Parented<espnow::ESPNowComponent>,
                      public espnow::ESPNowUnknownPeerHandler,
                      public espnow::ESPNowReceivedPacketHandler,
                      public espnow::ESPNowBroadcastedHandler {
 public:
  set_network_name(std::string name) { this->network_name_ = name; }
  set_private_key(std::string key) { this->private_key_ = key; }
  set_scan_timeout(uint32 timeout) { this->scan_timeout_ = timeout; }

  bool on_unknown_peer(const espnow::ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;
  bool on_received(const espnow::ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;
  bool on_broadcasted(const espnow::ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;

  void setup() override;
  void dump_config() override;
  void update() override;

  void start();
  void stop();

 protected:
  std::string network_name_{"ESPHome"};
  std::string private_key_{"aSZvxD9Lo*2pqf"};
  uint32_t scan_timeout_{5000};

  std::map<uint64_t, uint8_t> pairings_{};
  uint64_t main_keeper_{0};
  bool scanning_{false};
  std::string random_code_{""};

  void start_scan_timer_();
  void update_active_keeper_();

  ESPPreferenceObject pref_;
}

}  // namespace esphome::espnow_pairing

#endif
