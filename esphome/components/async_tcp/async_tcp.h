#pragma once
#include "esphome/core/defines.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
// Use AsyncTCP library for ESP32 (Arduino or ESP-IDF) and LibreTiny
#ifdef __has_include
#if __has_include(<AsyncTCP.h>)
#include <AsyncTCP.h>
#else
// Fallback for clang-tidy or other analysis tools
#include "async_tcp_socket.h"
using esphome::async_tcp::AsyncClient;  // NOLINT(google-global-names-in-headers)
#endif
#else
#include <AsyncTCP.h>
#endif
#elif defined(USE_ESP8266)
// Use ESPAsyncTCP library for ESP8266 (always Arduino)
#include <ESPAsyncTCP.h>
#elif defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)
// Use socket-based implementation for platforms with socket support
#include "async_tcp_socket.h"
// Expose AsyncClient in global namespace to match Arduino library behavior
using esphome::async_tcp::AsyncClient;  // NOLINT(google-global-names-in-headers)
#endif
