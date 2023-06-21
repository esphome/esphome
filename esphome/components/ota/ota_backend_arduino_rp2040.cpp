#include "esphome/core/defines.h"
#ifdef USE_ARDUINO
#ifdef USE_RP2040

#include "esphome/components/rp2040/preferences.h"
#include "ota_backend.h"
#include "ota_backend_arduino_rp2040.h"
#include "ota_component.h"

#include <Updater.h>

namespace esphome {
namespace ota {

OTAResponseTypes ArduinoRP2040OTABackend::begin(size_t image_size) {
  bool ret = Update.begin(image_size, U_FLASH);
  if (ret) {
    rp2040::preferences_prevent_write(true);
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
    return OTA_RESPONSE_ERROR_RP2040_NOT_ENOUGH_SPACE;
  return OTA_RESPONSE_ERROR_UNKNOWN;
}

void ArduinoRP2040OTABackend::set_update_md5(const char *md5) { Update.setMD5(md5); }

OTAResponseTypes ArduinoRP2040OTABackend::write(uint8_t *data, size_t len) {
  size_t written = Update.write(data, len);
  if (written != len) {
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ArduinoRP2040OTABackend::end() {
  if (!Update.end())
    return OTA_RESPONSE_ERROR_UPDATE_END;
  return OTA_RESPONSE_OK;
}

void ArduinoRP2040OTABackend::abort() {
  Update.end();
  rp2040::preferences_prevent_write(false);
}

}  // namespace ota
}  // namespace esphome

#endif  // USE_RP2040
#endif  // USE_ARDUINO
