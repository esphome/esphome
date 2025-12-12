#pragma once
#include "esphome/core/defines.h"

#if (defined(USE_ESP32) || defined(USE_LIBRETINY)) && !defined(CLANG_TIDY)
// Use AsyncTCP library for ESP32 (Arduino or ESP-IDF) and LibreTiny
// But not for clang-tidy as the header file isn't present in that case
#include <AsyncTCP.h>
#elif defined(USE_ESP8266)
// Use ESPAsyncTCP library for ESP8266 (always Arduino)
#include <ESPAsyncTCP.h>
#elif defined(USE_RP2040)
// Use AsyncTCP_RP2040W library for RP2040
#include <AsyncTCP_RP2040W.h>
#else
// Use socket-based implementation for other platforms and clang-tidy
#include "async_tcp_socket.h"
// Expose AsyncClient in global namespace to match library behavior
using esphome::async_tcp::AsyncClient;  // NOLINT(google-global-names-in-headers)
#define ESPHOME_ASYNC_TCP_SOCKET_IMPL
#endif
