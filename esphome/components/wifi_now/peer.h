#pragma once

#include <utility>

#include "esphome/core/defines.h"
#include "esphome/core/optional.h"

#include "simple_types.h"

namespace esphome {
namespace wifi_now {

class WifiNowPeer {
 public:
  WifiNowPeer();

  void set_bssid(bssid_t bssid);
  const bssid_t &get_bssid() const;
  void set_aeskey(aeskey_t aeskey);
  void set_aeskey(optional<aeskey_t> aeskey);
  const optional<aeskey_t> &get_aeskey() const;

 protected:
  bssid_t bssid_{};
  optional<aeskey_t> aeskey_{};
};

}  // namespace wifi_now
}  // namespace esphome
