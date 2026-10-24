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

}  // namespace esphome::mqtt
