#pragma once

#include "esphome/components/api/api_connection.h"
#include "esphome/core/component.h"

namespace esphome::sndbuf_pin {

// Test-only (host): pins SO_SNDBUF on every open TCP socket so integration
// tests get deterministic backpressure; an explicit SO_SNDBUF also disables
// kernel autotuning, and accepted sockets inherit it from the listener.
class SndbufPinComponent : public Component {
 public:
  explicit SndbufPinComponent(int buffer_size) : buffer_size_(buffer_size) {}
  void setup() override;
  // After the api server so its listening socket exists
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  int buffer_size_;
};

/// The api client subscribed to logs, or nullptr. A fixture lambda cannot name the api namespace:
/// the test harness rewrites every "api" key followed by a colon in the YAML.
api::APIConnection *api_log_client();

#ifdef __linux__
/// True once the api client's receive window is closed: everything sent is acknowledged and data still
/// waits to be sent, so no acknowledgement can free send buffer space until the client reads again.
bool api_peer_window_closed();
#endif

}  // namespace esphome::sndbuf_pin
