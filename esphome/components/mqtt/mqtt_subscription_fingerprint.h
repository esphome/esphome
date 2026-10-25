#pragma once

#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::mqtt {

/// Adds one subscription to a fingerprint of a set of subscriptions; the order they are added in does not matter.
inline uint32_t add_subscription_fingerprint(uint32_t fingerprint, const char *topic, uint8_t qos) {
  return fingerprint + fnv1a_hash_extend(fnv1a_hash(topic), qos);
}

/// True when a resumed session still holds the subscriptions made at the previous connect.
constexpr bool session_has_subscriptions(bool session_present, uint32_t stored, uint32_t current) {
  return session_present && stored == current;
}

/// The fingerprint to keep saved while the subscriptions are sent again. Cleared, so a link lost part way through
/// leaves a partial session that the next connect does not take for a complete one.
constexpr uint32_t stored_fingerprint_during_rebuild(bool resumed, uint32_t stored) { return resumed ? stored : 0; }

/// True when a SUBACK return code grants the subscription; 0x80 and up are refusals.
constexpr bool subscribe_granted(uint8_t return_code) { return return_code < 0x80; }

/// True when the broker acknowledged every SUBSCRIBE sent on this connection and none was refused.
constexpr bool fingerprint_save_allowed(uint16_t sent, uint16_t acknowledged, bool blocked) {
  return !blocked && sent == acknowledged;
}

}  // namespace esphome::mqtt
