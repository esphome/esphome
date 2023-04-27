#include "esphome/core/defines.h"
#if defined(USE_ESP32_FRAMEWORK_ARDUINO) || defined(USE_LIBRETINY)

#include "ota_backend_arduino_esp32.h"
#include "ota_component.h"
#include "ota_backend.h"

#include <Update.h>

namespace esphome {
namespace ota {

OTAResponseTypes ArduinoESP32OTABackend::begin(size_t image_size) {
  bool ret = Update.begin(image_size, U_FLASH);
  if (ret) {
    return OTA_RESPONSE_OK;
  }

  uint8_t error = Update.getError();
#ifndef USE_LIBRETINY
  this->last_errno_ = error;
#else
  this->last_errno_ = Update.getErrorCode();
#endif
  if (error == UPDATE_ERROR_SIZE)
    return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void ArduinoESP32OTABackend::set_update_md5(const char *md5) {
#ifndef USE_LIBRETINY  // not yet implemented
  Update.setMD5(md5);
#endif
}

OTAResponseTypes ArduinoESP32OTABackend::write(uint8_t *data, size_t len) {
  size_t written = Update.write(data, len);
#ifndef USE_LIBRETINY
  this->last_errno_ = Update.getError();
#else
  this->last_errno_ = Update.getErrorCode();
#endif
  if (written != len) {
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ArduinoESP32OTABackend::end() {
  if (!Update.end()) {
#ifndef USE_LIBRETINY
    this->last_errno_ = Update.getError();
#else
    this->last_errno_ = Update.getErrorCode();
#endif
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }
  return OTA_RESPONSE_OK;
}

void ArduinoESP32OTABackend::abort() { Update.abort(); }

}  // namespace ota
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO || USE_LIBRETINY
