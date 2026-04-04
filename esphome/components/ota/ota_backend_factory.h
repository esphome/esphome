#pragma once

#include "ota_backend.h"

#include <memory>

#ifdef USE_ESP8266
#include "ota_backend_esp8266.h"
#elif defined(USE_ESP32)
#include "ota_backend_esp_idf.h"
#elif defined(USE_RP2040)
#include "ota_backend_arduino_rp2040.h"
#elif defined(USE_LIBRETINY)
#include "ota_backend_arduino_libretiny.h"
#elif defined(USE_HOST)
#include "ota_backend_host.h"
#else
// Stub for static analysis when no platform is defined
namespace esphome::ota {
struct StubOTABackend {};
std::unique_ptr<StubOTABackend> make_ota_backend();
}  // namespace esphome::ota
#endif

namespace esphome::ota {
using OTABackendPtr = decltype(make_ota_backend());
}  // namespace esphome::ota
