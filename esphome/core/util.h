#pragma once

#include <string>
namespace esphome {

/// Return whether the node has at least one client connected to the native API
bool api_is_connected();

/// Return whether the node has an active connection to an MQTT broker
bool mqtt_is_connected();

/// Return whether the node has any form of "remote" connection via the API or to an MQTT broker
bool remote_is_connected();

}  // namespace esphome
