#pragma once

#ifdef USE_ARDUINO
#ifdef USE_ESP32
#include <AsyncTCP.h>
#elif defined(USE_ESP8266)
#include <ESPAsyncTCP.h>
#endif
#else
#include "async_tcp_socket.h"
// Expose AsyncClient in global namespace to match Arduino library behavior
using esphome::async_tcp::AsyncClient;
#endif
