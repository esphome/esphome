#pragma once

#include <string>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {

/// Return whether the node has at least one client connected to the native API.
///
/// Inline so that hot-path callers (e.g. component loop() ticks that check connectivity every
/// iteration) can skip the call8/return pair. With USE_API disabled this trivially returns false
/// and collapses at compile time.
#ifdef USE_API
ESPHOME_ALWAYS_INLINE inline bool api_is_connected() {
  return api::global_api_server != nullptr && api::global_api_server->is_connected();
}
#else
ESPHOME_ALWAYS_INLINE inline bool api_is_connected() { return false; }
#endif

/// Return whether the node has an active connection to an MQTT broker
bool mqtt_is_connected();

/// Return whether the node has any form of "remote" connection via the API or to an MQTT broker
bool remote_is_connected();

}  // namespace esphome
