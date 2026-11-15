#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace esphome::wifi {

// Switching APs calls esp_wifi_disconnect(), which queues a leave event for the old AP, or with no BSSID for an
// attempt that never associated. Remembers it so that event does not end the next attempt. Platform free for tests.
enum class ExpectedLeave : uint8_t {
  EXPECTED_LEAVE_NONE,
  EXPECTED_LEAVE_AP,
  EXPECTED_LEAVE_ATTEMPT,  ///< An attempt that never associated, so no BSSID
};

class ExpectedDisconnect {
 public:
  void arm(const uint8_t *bssid) { this->arm_(bssid, ExpectedLeave::EXPECTED_LEAVE_AP); }
  void clear() { this->expected_ = ExpectedLeave::EXPECTED_LEAVE_NONE; }

  /// Disconnects and, once that succeeds, expects the leave event. read_bssid(uint8_t *out) runs first, since the
  /// record is gone after the disconnect, and writes the AP's BSSID only when it returns true; without an AP the
  /// attempt's leave, with no BSSID, is expected. disconnect() returns whether it succeeded, which is the result.
  template<typename ReadBssid, typename Disconnect>
  bool disconnect_and_expect_leave(ReadBssid &&read_bssid, Disconnect &&disconnect) {
    this->clear();
    std::array<uint8_t, 6> bssid{};
    const bool had_ap = read_bssid(bssid.data());
    if (!disconnect())
      return false;
    this->arm_(bssid.data(), had_ap ? ExpectedLeave::EXPECTED_LEAVE_AP : ExpectedLeave::EXPECTED_LEAVE_ATTEMPT);
    return true;
  }

  /// A connected event still queued from the previous AP; it keeps the expectation.
  bool is_previous_ap(const uint8_t *bssid) const {
    return this->expected_ != ExpectedLeave::EXPECTED_LEAVE_NONE &&
           std::equal(this->bssid_.begin(), this->bssid_.end(), bssid);
  }

  /// Which expected leave a disconnect is. \p is_leave covers an AP's association leave (8) and the station leave
  /// of an attempt that never associated (36). Every disconnect ends the expectation, so it matches at most once.
  ExpectedLeave consume_disconnect(const uint8_t *bssid, bool is_leave) {
    const ExpectedLeave matched =
        is_leave && this->is_previous_ap(bssid) ? this->expected_ : ExpectedLeave::EXPECTED_LEAVE_NONE;
    this->clear();
    return matched;
  }

 protected:
  void arm_(const uint8_t *bssid, ExpectedLeave expected) {
    std::copy_n(bssid, this->bssid_.size(), this->bssid_.begin());
    this->expected_ = expected;
  }

  std::array<uint8_t, 6> bssid_{};
  ExpectedLeave expected_{ExpectedLeave::EXPECTED_LEAVE_NONE};
};

}  // namespace esphome::wifi
