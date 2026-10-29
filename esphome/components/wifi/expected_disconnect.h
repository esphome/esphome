#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace esphome::wifi {

// Switching APs calls esp_wifi_disconnect(), which queues an association-leave event from the old AP. Remembers
// that AP so its leave event does not end the connection attempt started after it. Platform free for host tests.
class ExpectedDisconnect {
 public:
  void arm(const uint8_t *bssid) {
    std::copy_n(bssid, this->bssid_.size(), this->bssid_.begin());
    this->armed_ = true;
  }
  void clear() { this->armed_ = false; }
  bool is_armed() const { return this->armed_; }

  /// A connected event still queued from the previous AP; it keeps the expectation.
  bool is_previous_ap(const uint8_t *bssid) const {
    return this->armed_ && std::equal(this->bssid_.begin(), this->bssid_.end(), bssid);
  }

  /// Whether a disconnect is the previous AP's leave. Every disconnect except a roaming one ends the
  /// expectation, so it matches at most once.
  bool consume_disconnect(const uint8_t *bssid, bool assoc_leave, bool roaming) {
    const bool expected = assoc_leave && this->is_previous_ap(bssid);
    if (!roaming)
      this->armed_ = false;
    return expected;
  }

 protected:
  std::array<uint8_t, 6> bssid_{};
  bool armed_{false};
};

}  // namespace esphome::wifi
