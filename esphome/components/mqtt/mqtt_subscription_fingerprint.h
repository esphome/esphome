#pragma once

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

/// The session fingerprint to keep while the subscriptions are sent again. Untrusted, so a link lost or a reset
/// part way through leaves a partial session that the next connect does not take for a complete one.
constexpr uint32_t session_fingerprint_during_rebuild(bool resumed, uint32_t trusted) {
  return resumed ? trusted : UNTRUSTED_SESSION_FINGERPRINT;
}

/// True when a SUBACK return code grants the subscription; 0x80 and up are refusals.
constexpr bool subscribe_granted(uint8_t return_code) { return return_code < 0x80; }

/// True when the broker acknowledged every SUBSCRIBE sent on this connection and none was refused.
constexpr bool fingerprint_save_allowed(uint16_t sent, uint16_t acknowledged, bool blocked) {
  return !blocked && sent == acknowledged;
}

}  // namespace esphome::mqtt
