#pragma once

#include "esphome/core/defines.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::zigbee {

#ifdef ZIGBEE_MAX_ENDPOINT_ID
static constexpr uint8_t MAX_ENDPOINT_ID = ZIGBEE_MAX_ENDPOINT_ID;
#else
// Standalone tests use the protocol limit; firmware gets it from Python validation.
static constexpr uint8_t MAX_ENDPOINT_ID = 239;
#endif

/// Endpoint listeners, found by the endpoint each one reports. Wire endpoint IDs are sparse, so capacity counts
/// listeners, not IDs. T provides get_endpoint() and on_zcl_device_event(buffer).
template<typename T, size_t N> class EndpointListeners {
 public:
  /// False for an endpoint outside 1..MAX_ENDPOINT_ID, a duplicate endpoint or a full table.
  bool add(T *listener) {
    const uint8_t endpoint = listener->get_endpoint();
    if (endpoint == 0 || endpoint > MAX_ENDPOINT_ID)
      return false;
    for (T *&slot : this->listeners_) {
      if (slot == nullptr) {
        slot = listener;
        return true;
      }
      if (slot->get_endpoint() == endpoint)
        return false;
    }
    return false;
  }

  bool dispatch(uint8_t endpoint, uint8_t buffer) const {
    for (T *listener : this->listeners_) {
      if (listener == nullptr)
        break;
      if (listener->get_endpoint() == endpoint) {
        listener->on_zcl_device_event(buffer);
        return true;
      }
    }
    return false;
  }

 protected:
  std::array<T *, N> listeners_{};
};

}  // namespace esphome::zigbee
