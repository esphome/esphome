#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API
#if defined(USE_API_TRANSPORT_IP)
#include "esphome/components/socket/headers.h"
#include "esphome/components/socket/socket.h"
#elif defined(USE_API_TRANSPORT_BLE)
#include "esphome/components/socket_ble/headers.h"
#include "esphome/components/socket_ble/socket_ble.h"
#endif

namespace esphome::api {

#ifdef USE_API_TRANSPORT_IP
using APIListenSocket = socket::ListenSocket;
using APISocket = socket::Socket;
static constexpr size_t API_SOCKADDR_STR_LEN = socket::SOCKADDR_STR_LEN;
using api_sockaddr_t = struct sockaddr;
using api_sockaddr_storage_t = struct sockaddr_storage;
#elif defined(USE_API_TRANSPORT_BLE)
using APIListenSocket = socket_ble::BleL2capListenSocket;
using APISocket = socket_ble::BleL2capSocket;
static constexpr size_t API_SOCKADDR_STR_LEN = BDADDR_STR_LEN;
using api_sockaddr_t = struct sockaddr_l2;
using api_sockaddr_storage_t = struct sockaddr_l2;
#else
#error "No API transport defined"
#endif

}  // namespace esphome::api

#endif
