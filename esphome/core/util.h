#pragma once

#include <string>

namespace esphome {

/// Return whether the node is connected to the network (through wifi, eth, ...)
bool network_is_connected();
/// Get the active network hostname
std::string network_get_address();

/// Manually set up the network stack (outside of the App.setup() loop, for example in OTA safe mode)
void network_setup_mdns();
void network_tick_mdns();

}  // namespace esphome
