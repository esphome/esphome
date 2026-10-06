#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API
// IP is the transport unless the BLE one is selected
#ifdef USE_API_TRANSPORT_BLE
#include "esphome/components/socket_ble/headers.h"
#include "esphome/components/socket_ble/socket_ble.h"
#else
#include "esphome/components/socket/headers.h"
#include "esphome/components/socket/socket.h"
#endif

namespace esphome::api {

#ifdef USE_API_TRANSPORT_BLE
using APIListenSocket = socket_ble::BleL2capListenSocket;
using APISocket = socket_ble::BleL2capSocket;
static constexpr size_t API_SOCKADDR_STR_LEN = BDADDR_STR_LEN;
using api_sockaddr_t = struct sockaddr_l2;
using api_sockaddr_storage_t = struct sockaddr_l2;
#else
using APIListenSocket = socket::ListenSocket;
using APISocket = socket::Socket;
static constexpr size_t API_SOCKADDR_STR_LEN = socket::SOCKADDR_STR_LEN;
using api_sockaddr_t = struct sockaddr;
using api_sockaddr_storage_t = struct sockaddr_storage;
#endif

}  // namespace esphome::api

#endif  // USE_API
