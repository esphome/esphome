#ifdef USE_ARDUINO
#ifdef USE_ESP8266
#include "ota_backend_arduino_esp8266.h"
#include "ota_backend.h"

#include "esphome/components/esp8266/preferences.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include <Updater.h>

namespace esphome {
namespace ota {

static const char *const TAG = "ota.arduino_esp8266";

std::unique_ptr<ota::OTABackend> make_ota_backend() { return make_unique<ota::ArduinoESP8266OTABackend>(); }

OTAResponseTypes ArduinoESP8266OTABackend::begin(size_t image_size) {
  // Handle UPDATE_SIZE_UNKNOWN (0) by calculating available space
  if (image_size == 0) {
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    image_size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  }
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

  ESP_LOGE(TAG, "Begin error: %d", error);

  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void ArduinoESP8266OTABackend::set_update_md5(const char *md5) {
  Update.setMD5(md5);
  this->md5_set_ = true;
}

OTAResponseTypes ArduinoESP8266OTABackend::write(uint8_t *data, size_t len) {
  size_t written = Update.write(data, len);
  if (written == len) {
    return OTA_RESPONSE_OK;
  }

  uint8_t error = Update.getError();
  ESP_LOGE(TAG, "Write error: %d", error);

  return OTA_RESPONSE_ERROR_WRITING_FLASH;
}

OTAResponseTypes ArduinoESP8266OTABackend::end() {
  // Use strict validation (false) when MD5 is set, lenient validation (true) when no MD5
  // This matches the behavior of the old web_server OTA implementation
  bool success = Update.end(!this->md5_set_);

  // On ESP8266, Update.end() might return false even with error code 0
  // Check the actual error code to determine success
  uint8_t error = Update.getError();

  if (success || error == UPDATE_ERROR_OK) {
    return OTA_RESPONSE_OK;
  }

  ESP_LOGE(TAG, "End error: %d", error);
  return OTA_RESPONSE_ERROR_UPDATE_END;
}

void ArduinoESP8266OTABackend::abort() {
  Update.end();
  esp8266::preferences_prevent_write(false);
}

}  // namespace ota
}  // namespace esphome

#endif
#endif
