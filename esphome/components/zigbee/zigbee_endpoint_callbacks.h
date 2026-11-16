#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace esphome::zigbee {

static constexpr uint8_t MAX_ENDPOINT_ID = 239;

// Wire endpoint IDs are sparse; capacity counts registered handlers, not IDs.
template<size_t N> class EndpointCallbacks {
 public:
  template<typename F> bool add(uint8_t endpoint, F &&callback) {
    if (endpoint == 0 || endpoint > MAX_ENDPOINT_ID)
      return false;
    for (const auto registered_endpoint : this->endpoints_) {
      if (registered_endpoint == endpoint)
        return false;
    }
    for (size_t i = 0; i < N; i++) {
      if (this->endpoints_[i] == 0) {
        this->callbacks_[i] = std::forward<F>(callback);
        this->endpoints_[i] = endpoint;
        return true;
      }
    }
    return false;
  }

  bool dispatch(uint8_t endpoint, uint8_t buffer) const {
    if (endpoint == 0 || endpoint > MAX_ENDPOINT_ID)
      return false;
    for (size_t i = 0; i < N; i++) {
      if (this->endpoints_[i] == endpoint && this->callbacks_[i]) {
        this->callbacks_[i](buffer);
        return true;
      }
    }
    return false;
  }

 protected:
  std::array<uint8_t, N> endpoints_{};
  std::array<std::function<void(uint8_t)>, N> callbacks_{};
};

}  // namespace esphome::zigbee
