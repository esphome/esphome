#include "ble_client_state.h"

namespace esphome::ble_device_base {

const char *client_state_to_string(ClientState state) {
  switch (state) {
    case ClientState::INIT:
      return "INIT";
    case ClientState::DISCONNECTING:
      return "DISCONNECTING";
    case ClientState::IDLE:
      return "IDLE";
    case ClientState::DISCOVERED:
      return "DISCOVERED";
    case ClientState::CONNECTING:
      return "CONNECTING";
    case ClientState::CONNECTED:
      return "CONNECTED";
    case ClientState::ESTABLISHED:
      return "ESTABLISHED";
    default:
      return "UNKNOWN";
  }
}

}  // namespace esphome::ble_device_base
