#pragma once

#include <string>
#include "IPAddress.h"

namespace esphome {

/// Return whether the node is connected to the network (through wifi, eth, ...)
bool network_is_connected();
/// Get the active network hostname
std::string network_get_address();

/// Manually set up the network stack (outside of the App.setup() loop, for example in OTA safe mode)
#ifdef ARDUINO_ARCH_ESP8266
void network_setup_mdns(IPAddress address, int interface);
#endif
#ifdef ARDUINO_ARCH_ESP32
void network_setup_mdns();
#endif
void network_tick_mdns();

}  // namespace esphome
