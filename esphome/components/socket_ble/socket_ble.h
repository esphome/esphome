#pragma once
#include "esphome/core/defines.h"

#if (defined(USE_ESP32) && defined(CONFIG_BT_NIMBLE_ENABLED)) || defined(USE_ZEPHYR) || defined(USE_RP2040)
#include <memory>
#include <span>
#include <string>

#include "esphome/core/optional.h"
#include "headers.h"

// Include only the active implementation's header.
#if defined(USE_ESP32)
#include "esp32_sockets_l2cap_impl.h"
#elif defined(USE_ZEPHYR)
#include "zephyr_sockets_l2cap_impl.h"
#elif defined(USE_RP2040)
#include "rp2040_sockets_l2cap_impl.h"
#endif

namespace esphome::socket_ble {

// Type aliases — only one implementation is active per build.
// Socket is the concrete type for connected sockets.
// ListenSocket is the concrete type for listening/server sockets.

#if defined(USE_ESP32)
using BleL2capSocket = ESP32BleL2capImpl;
using BleL2capListenSocket = ESP32BleL2capListenImpl;
#elif defined(USE_ZEPHYR)
using BleL2capSocket = ZephyrBleL2capImpl;
using BleL2capListenSocket = ZephyrBleL2capListenImpl;
#elif defined(USE_RP2040)
using BleL2capSocket = RP2040BleL2capImpl;
using BleL2capListenSocket = RP2040BleL2capListenImpl;
#endif

/// Create a listening socket of the given domain, type and protocol.
/// Create a listening socket and monitor it for data in the main loop.
std::unique_ptr<BleL2capListenSocket> socket_ble_listen(int domain, int type, int protocol);
std::unique_ptr<BleL2capListenSocket> socket_ble_listen_loop_monitored(int domain, int type, int protocol);

size_t format_bdaddr_to(const bdaddr_t addr, std::span<char, BDADDR_STR_LEN> buf);

}  // namespace esphome::socket_ble

#endif
