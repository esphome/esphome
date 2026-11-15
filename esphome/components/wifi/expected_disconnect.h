#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
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

  /// Disconnects from the current AP and, once that succeeds, expects its leave event. read_bssid(uint8_t *out)
  /// returns whether there was an AP and runs first, since the record is gone after the disconnect; disconnect()
  /// returns whether it succeeded, which is also the result. Any earlier expectation is dropped.
  template<typename ReadBssid, typename Disconnect>
  bool disconnect_and_expect_leave(ReadBssid &&read_bssid, Disconnect &&disconnect) {
    this->armed_ = false;
    std::array<uint8_t, 6> bssid{};
    const bool had_ap = read_bssid(bssid.data());
    if (!disconnect())
      return false;
    if (had_ap)
      this->arm(bssid.data());
    return true;
  }

  /// A connected event still queued from the previous AP; it keeps the expectation.
  bool is_previous_ap(const uint8_t *bssid) const {
    return this->armed_ && std::equal(this->bssid_.begin(), this->bssid_.end(), bssid);
  }

  /// Whether a disconnect is the previous AP's leave. Every disconnect ends the expectation, so it matches at
  /// most once; the caller handles roaming disconnects before this.
  bool consume_disconnect(const uint8_t *bssid, bool assoc_leave) {
    const bool expected = assoc_leave && this->is_previous_ap(bssid);
    this->armed_ = false;
    return expected;
  }

 protected:
  std::array<uint8_t, 6> bssid_{};
  bool armed_{false};
};

/// The SSID of the connect attempt in progress. When a new attempt replaces one that never associated, the driver
/// reports the old attempt's leave (reason 36, no BSSID) only after the new one has started, naming the old SSID.
class AttemptSsid {
 public:
  void set(const uint8_t *ssid, size_t len) {
    this->len_ = static_cast<uint8_t>(std::min(len, this->ssid_.size()));
    std::copy_n(ssid, this->len_, this->ssid_.begin());
    this->set_ = true;
  }

  /// Whether a disconnect names another SSID than the current attempt, so it belongs to an earlier attempt.
  bool is_other(const uint8_t *ssid, size_t len) const {
    return this->set_ && (len != this->len_ || !std::equal(ssid, ssid + len, this->ssid_.begin()));
  }

 protected:
  std::array<uint8_t, 32> ssid_{};
  uint8_t len_{0};
  bool set_{false};
};

}  // namespace esphome::wifi
