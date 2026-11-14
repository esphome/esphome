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
}  // namespace esphome::web_server_base
#endif
