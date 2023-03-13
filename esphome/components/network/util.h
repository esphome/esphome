#pragma once

#include <string>
#include "ip_address.h"

namespace esphome {
namespace network {

/// Return whether the node is connected to the network (through wifi, eth, ...)
bool is_connected();
/// Get the active network hostname
std::string get_use_address();
IPAddress get_ip_address();

}  // namespace network
}  // namespace esphome
