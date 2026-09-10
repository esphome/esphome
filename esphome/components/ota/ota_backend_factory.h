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
#elif defined(USE_HOST)
#include "ota_backend_host.h"
#else
// Stub for static analysis when no platform is defined
namespace esphome::ota {
struct StubOTABackend {
  OTAResponseTypes begin(size_t image_size, OTAType ota_type = OTA_TYPE_UPDATE_APP) {
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  void set_update_md5(const char *md5) {}
  OTAResponseTypes write(uint8_t *data, size_t len) { return OTA_RESPONSE_ERROR_UNKNOWN; }
  OTAResponseTypes end() { return OTA_RESPONSE_ERROR_UNKNOWN; }
  void abort() {}
  bool supports_compression() { return false; }
};
std::unique_ptr<StubOTABackend> make_ota_backend();
}  // namespace esphome::ota
#endif

namespace esphome::ota {
using OTABackendPtr = decltype(make_ota_backend());
static_assert(OTABackendContract<OTABackendPtr::element_type>,
              "The platform's OTA backend is missing part of the backend surface (ota_backend.h)");
}  // namespace esphome::ota
