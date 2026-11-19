#pragma once

#include "esphome/core/defines.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace esphome::zigbee {

#ifdef ZIGBEE_MAX_ENDPOINT_ID
static constexpr uint8_t MAX_ENDPOINT_ID = ZIGBEE_MAX_ENDPOINT_ID;
#else
// Standalone tests use the protocol limit; firmware gets it from Python validation.
static constexpr uint8_t MAX_ENDPOINT_ID = 239;
#endif

// Wire endpoint IDs are sparse; capacity counts registered handlers, not IDs.
template<size_t N> class EndpointCallbacks {
 public:
  template<typename F> bool add(uint8_t endpoint, F &&callback) {
    if (endpoint == 0 || endpoint > MAX_ENDPOINT_ID)
      return false;
    for (const auto &entry : this->entries_) {
      if (entry.endpoint == endpoint)
        return false;
    }
    for (auto &entry : this->entries_) {
      if (entry.endpoint == 0) {
        entry.callback = std::forward<F>(callback);
        entry.endpoint = endpoint;
        return true;
      }
    }
    return false;
  }

  bool dispatch(uint8_t endpoint, uint8_t buffer) const {
    if (endpoint == 0 || endpoint > MAX_ENDPOINT_ID)
      return false;
    for (const auto &entry : this->entries_) {
      if (entry.endpoint == endpoint && entry.callback) {
        entry.callback(buffer);
        return true;
      }
    }
    return false;
  }

 protected:
  struct Entry {
    uint8_t endpoint{};
    std::function<void(uint8_t)> callback;
  };
  std::array<Entry, N> entries_{};
};

}  // namespace esphome::zigbee
