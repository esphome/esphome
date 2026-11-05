#pragma once

#include <atomic>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::mqtt {

/// Adds one subscription to a fingerprint of a set of subscriptions; the order they are added in does not matter.
inline uint32_t add_subscription_fingerprint(uint32_t fingerprint, const char *topic, uint8_t qos) {
  return fingerprint + fnv1a_hash_extend(fnv1a_hash(topic), qos);
}

/// Session fingerprint while a rebuild is in progress, and after a power loss or a cold boot.
static constexpr uint32_t UNTRUSTED_SESSION_FINGERPRINT = 0;

/// True when a resumed session holds in full the subscriptions it is trusted with, and they are the current ones.
constexpr bool session_has_subscriptions(bool session_present, uint32_t trusted, uint32_t current) {
  return session_present && trusted != UNTRUSTED_SESSION_FINGERPRINT && trusted == current;
}

/// First SUBACK return code that refuses a subscription; also stands in for a SUBACK with no return code.
static constexpr uint8_t SUBACK_FAILURE = 0x80;

/// True when a SUBACK return code grants the subscription.
constexpr bool subscribe_granted(uint8_t return_code) { return return_code < SUBACK_FAILURE; }

enum class SubackTally : uint8_t {
  SUBACK_TALLY_PENDING,
  SUBACK_TALLY_ALL_GRANTED,
  SUBACK_TALLY_NOT_ALL_GRANTED,
};

/// SUBACKs received on the current connection. Written only by the context that delivers broker events (so plain
/// loads and stores, no read-modify-write), read by the main loop.
class SubackCounter {
 public:
  void reset() {
    this->received_.store(0, std::memory_order_relaxed);
    this->granted_.store(0, std::memory_order_relaxed);
  }
  void add(uint8_t return_code) {
    if (subscribe_granted(return_code))
      this->granted_.store(this->granted_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    // Released after granted_, so a reader that sees this SUBACK in received_ also sees it in granted_
    this->received_.store(this->received_.load(std::memory_order_relaxed) + 1, std::memory_order_release);
  }
  /// Compares the SUBACKs received with the number of SUBSCRIBEs sent on this connection.
  SubackTally tally(uint16_t sent) const {
    const uint16_t received = this->received_.load(std::memory_order_acquire);
    if (received < sent)
      return SubackTally::SUBACK_TALLY_PENDING;
    // More than sent means a SUBACK from an earlier connection was counted, so none can be trusted
    return received == sent && this->granted_.load(std::memory_order_relaxed) == sent
               ? SubackTally::SUBACK_TALLY_ALL_GRANTED
               : SubackTally::SUBACK_TALLY_NOT_ALL_GRANTED;
  }

 protected:
  std::atomic<uint16_t> received_{0};
  std::atomic<uint16_t> granted_{0};
};

}  // namespace esphome::mqtt
