#pragma once

#include "esphome/core/defines.h"

// The long lived stream response the /events layer writes to, for the platform's web server
#ifdef USE_ESP32
#include "esphome/components/web_server_idf/web_server_idf.h"
#include "esphome/core/helpers.h"

#include <sys/socket.h>

namespace esphome::web_server_base {
using web_server_idf::AsyncStreamResponse;
// One entry of the gather list AsyncStreamResponse::write() takes
using StreamIovec = ::iovec;
// Guards the hand-off of new streams from the server task to the main loop
using StreamIntakeGuard = LockGuard;
}  // namespace esphome::web_server_base
#elif defined(USE_ARDUINO)
// ESP8266, RP2040 and LibreTiny: ESPAsyncWebServer, with the connection taken from the request
#include "async_stream_response.h"
#include "esphome/core/helpers.h"

namespace esphome::web_server_base {
// Guards the hand-off of new streams from the network context to the main loop: the lwIP lock keeps RP2's network
// IRQ out, the mutex LibreTiny's async_tcp task; each is a no-op where it is not needed (both on ESP8266)
class StreamIntakeGuard {
 public:
  explicit StreamIntakeGuard(Mutex &mutex) : guard_(mutex) {}

 protected:
  LwIPLock lock_;
  LockGuard guard_;
};
}  // namespace esphome::web_server_base
#endif
