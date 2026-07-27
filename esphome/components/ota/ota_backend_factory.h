#pragma once

#include "ota_backend.h"

#include <memory>

#ifdef USE_ESP8266
#include "ota_backend_esp8266.h"
#elif defined(USE_ESP32)
#include "ota_backend_esp_idf.h"
#elif defined(USE_RP2)
#include "ota_backend_arduino_rp2.h"
#elif defined(USE_LIBRETINY)
#include "ota_backend_arduino_libretiny.h"
#elif defined(USE_ZEPHYR) && !defined(USE_NRF52)
// nrf52 also defines USE_ZEPHYR but has its own separate OTA rollback handling
// (nrf52/__init__.py's advanced.enable_ota_rollback) -- excluded here, falls through
// to the stub below.
#include "ota_backend_zephyr.h"
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
