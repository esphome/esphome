#pragma once

#include "esphome/core/defines.h"

// The long lived stream response the /events layer writes to, for the platform's web server
#ifdef USE_ESP32
#include "esphome/components/web_server_idf/web_server_idf.h"

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
// Guards the hand-off of new streams from the network context to the main loop. RP2's network callbacks run in an
// IRQ that holds the lwIP lock, and ESPHome's Mutex is a no-op there; ESP8266's never preempt the loop.
class StreamIntakeGuard {
 public:
  explicit StreamIntakeGuard(Mutex &mutex)
#ifndef USE_RP2
      : guard_(mutex)
#endif
  {
#ifdef USE_RP2
    (void) mutex;
#endif
  }

 protected:
#ifdef USE_RP2
  LwIPLock lock_;
#else
  LockGuard guard_;
#endif
};
}  // namespace esphome::web_server_base
#endif
