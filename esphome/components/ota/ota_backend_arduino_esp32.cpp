#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include "esphome/core/defines.h"

#include "ota_backend_arduino_esp32.h"
#include "ota_backend.h"

#include <Update.h>

namespace esphome {
namespace ota {

std::unique_ptr<ota::OTABackend> make_ota_backend() { return make_unique<ota::ArduinoESP32OTABackend>(); }

OTAResponseTypes ArduinoESP32OTABackend::begin(size_t image_size) {
  bool ret = Update.begin(image_size, U_FLASH);
  if (ret) {
    return OTA_RESPONSE_OK;
  }

  uint8_t error = Update.getError();
  if (error == UPDATE_ERROR_SIZE)
    return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void ArduinoESP32OTABackend::set_update_md5(const char *md5) { Update.setMD5(md5); }

OTAResponseTypes ArduinoESP32OTABackend::write(uint8_t *data, size_t len) {
  size_t written = Update.write(data, len);
  if (written != len) {
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ArduinoESP32OTABackend::end() {
  if (!Update.end())
    return OTA_RESPONSE_ERROR_UPDATE_END;
  return OTA_RESPONSE_OK;
}

void ArduinoESP32OTABackend::abort() { Update.abort(); }

}  // namespace ota
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
