#pragma once

#if defined(USE_ESP32) || (defined(USE_ARDUINO) && defined(USE_ESP8266))
// Use AsyncTCP library for ESP32 (Arduino or ESP-IDF) and ESP8266 (Arduino)
#ifdef USE_ESP32
#include <AsyncTCP.h>
#elif defined(USE_ESP8266)
#include <ESPAsyncTCP.h>
#endif
#else
// Use socket-based implementation for other platforms
#include "async_tcp_socket.h"
// Expose AsyncClient in global namespace to match Arduino library behavior
using esphome::async_tcp::AsyncClient;  // NOLINT(google-global-names-in-headers)
#endif
