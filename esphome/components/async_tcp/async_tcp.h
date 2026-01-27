#pragma once
#include "esphome/core/defines.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
// Use AsyncTCP library for ESP32 (Arduino or ESP-IDF) and LibreTiny
#include <AsyncTCP.h>
#elif defined(USE_ESP8266)
// Use ESPAsyncTCP library for ESP8266 (always Arduino)
#include <ESPAsyncTCP.h>
#elif defined(USE_RP2040)
// Use AsyncTCP_RP2040W library for RP2040
#include <AsyncTCP_RP2040W.h>
#else
// Use socket-based implementation for other platforms
#include "async_tcp_socket.h"
#endif
