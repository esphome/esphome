#pragma once
#include "esphome/core/defines.h"

#ifdef USE_ZEPHYR
#include <memory>
#include <span>

#include "headers.h"
#include "zephyr_sockets_l2cap_impl.h"

namespace esphome::socket_ble {

// One implementation is active per build, so the socket types are plain aliases
using BleL2capSocket = ZephyrBleL2capImpl;
using BleL2capListenSocket = ZephyrBleL2capListenImpl;

/// Create a listening socket; accepted channels wake the main loop.
std::unique_ptr<BleL2capListenSocket> socket_ble_listen_loop_monitored(int domain, int type, int protocol);

/// Format a Bluetooth address into buf, returns the length written (excluding null)
size_t format_bdaddr_to(const bdaddr_t addr, std::span<char, BDADDR_STR_LEN> buf);

}  // namespace esphome::socket_ble

#endif  // USE_ZEPHYR
