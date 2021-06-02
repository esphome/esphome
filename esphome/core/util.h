#pragma once

#include <string>
#include "IPAddress.h"

namespace esphome {

/// Macro for IDF version comparision
#ifndef ESP_IDF_VERSION_VAL
#define ESP_IDF_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
#endif

/// Return whether the node is connected to the network (through wifi, eth, ...)
bool network_is_connected();
/// Get the active network hostname
std::string network_get_address();

/// Return whether the node has at least one client connected to the native API
bool api_is_connected();

/// Return whether the node has an active connection to an MQTT broker
bool mqtt_is_connected();

/// Return whether the node has any form of "remote" connection via the API or to an MQTT broker
bool remote_is_connected();

/// Manually set up the network stack (outside of the App.setup() loop, for example in OTA safe mode)
#ifdef ARDUINO_ARCH_ESP8266
void network_setup_mdns(IPAddress address, int interface);
#endif
#ifdef ARDUINO_ARCH_ESP32
void network_setup_mdns();
#endif

void network_tick_mdns();

}  // namespace esphome
