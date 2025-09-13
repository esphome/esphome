#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include <string>
#include "ip_address.h"

namespace esphome {
namespace network {

/// Return whether the node is connected to the network (through wifi, eth, ...)
bool is_connected();
/// Return whether the network is disabled (only wifi for now)
bool is_disabled();
/// Get the active network hostname
std::string get_use_address();
IPAddresses get_ip_addresses();

}  // namespace network
}  // namespace esphome
#endif
