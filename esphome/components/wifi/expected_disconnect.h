#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace esphome::wifi {

// Switching APs calls esp_wifi_disconnect(), which queues a leave event for the old AP, or with no BSSID for an
// attempt that never associated. Remembers it so that event does not end the connection attempt started after it.
// When the driver posts no such event, the next real disconnect clears the expectation without matching it.
// Platform free for host tests.
/// Which leave a disconnect event matched.
enum class ExpectedLeave : uint8_t {
  EXPECTED_LEAVE_NONE,     ///< Not expected: a real disconnect
  EXPECTED_LEAVE_AP,       ///< The previous AP's leave
  EXPECTED_LEAVE_ATTEMPT,  ///< The leave of an attempt that never associated, with no BSSID
};

class ExpectedDisconnect {
 public:
  void arm(const uint8_t *bssid) {
    std::copy_n(bssid, this->bssid_.size(), this->bssid_.begin());
    this->had_ap_ = true;
    this->armed_ = true;
  }
  void clear() { this->armed_ = false; }

  /// Disconnects from the current AP and, once that succeeds, expects its leave event. read_bssid(uint8_t *out)
  /// returns whether there was an AP and runs first, since the record is gone after the disconnect; disconnect()
  /// returns whether it succeeded, which is also the result. Any earlier expectation is dropped. Without an AP the
  /// driver still reports an attempt it was making as left, with no BSSID, so that leave is expected too.
  template<typename ReadBssid, typename Disconnect>
  bool disconnect_and_expect_leave(ReadBssid &&read_bssid, Disconnect &&disconnect) {
    this->armed_ = false;
    std::array<uint8_t, 6> bssid{};
    const bool had_ap = read_bssid(bssid.data());
    if (!disconnect())
      return false;
    if (!had_ap)
      bssid.fill(0);
    this->arm(bssid.data());
    this->had_ap_ = had_ap;
    return true;
  }

  /// A connected event still queued from the previous AP; it keeps the expectation.
  bool is_previous_ap(const uint8_t *bssid) const {
    return this->armed_ && std::equal(this->bssid_.begin(), this->bssid_.end(), bssid);
  }

  /// Which expected leave a disconnect is, if any. \p is_leave is true for a leave reason: the association leave
  /// of an AP (8) and the station leave the driver reports for an attempt that never associated (36). Every
  /// disconnect ends the expectation, so it matches at most once; the caller handles roaming before this.
  ExpectedLeave consume_disconnect(const uint8_t *bssid, bool is_leave) {
    ExpectedLeave expected = ExpectedLeave::EXPECTED_LEAVE_NONE;
    if (is_leave && this->is_previous_ap(bssid))
      expected = this->had_ap_ ? ExpectedLeave::EXPECTED_LEAVE_AP : ExpectedLeave::EXPECTED_LEAVE_ATTEMPT;
    this->armed_ = false;
    return expected;
  }

 protected:
  std::array<uint8_t, 6> bssid_{};
  bool had_ap_{false};
  bool armed_{false};
};

}  // namespace esphome::wifi
