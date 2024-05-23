#ifdef USE_ARDUINO
#ifdef USE_ESP8266
#include "ota_backend.h"
#include "ota_backend_arduino_esp8266.h"

#include "esphome/core/defines.h"
#include "esphome/components/esp8266/preferences.h"

#include <Updater.h>

namespace esphome {
namespace ota {

std::unique_ptr<ota::OTABackend> make_ota_backend() { return make_unique<ota::ArduinoESP8266OTABackend>(); }

OTAResponseTypes ArduinoESP8266OTABackend::begin(size_t image_size) {
  bool ret = Update.begin(image_size, U_FLASH);
  if (ret) {
    esp8266::preferences_prevent_write(true);
    return OTA_RESPONSE_OK;
  }

  uint8_t error = Update.getError();
  if (error == UPDATE_ERROR_BOOTSTRAP)
    return OTA_RESPONSE_ERROR_INVALID_BOOTSTRAPPING;
  if (error == UPDATE_ERROR_NEW_FLASH_CONFIG)
    return OTA_RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG;
  if (error == UPDATE_ERROR_FLASH_CONFIG)
    return OTA_RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG;
  if (error == UPDATE_ERROR_SPACE)
    return OTA_RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE;
  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void ArduinoESP8266OTABackend::set_update_md5(const char *md5) { Update.setMD5(md5); }

OTAResponseTypes ArduinoESP8266OTABackend::write(uint8_t *data, size_t len) {
  size_t written = Update.write(data, len);
  if (written != len) {
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ArduinoESP8266OTABackend::end() {
  if (!Update.end())
    return OTA_RESPONSE_ERROR_UPDATE_END;
  return OTA_RESPONSE_OK;
}

void ArduinoESP8266OTABackend::abort() {
  Update.end();
  esp8266::preferences_prevent_write(false);
}

}  // namespace ota
}  // namespace esphome

#endif
#endif
