#pragma once

#include "esphome/core/defines.h"

// The long lived stream response the /events layer writes to, for the platform's web server
#ifdef USE_ESP32
#include "esphome/components/web_server_idf/web_server_idf.h"

namespace esphome::web_server_base {
using web_server_idf::AsyncStreamResponse;
}  // namespace esphome::web_server_base
#endif
