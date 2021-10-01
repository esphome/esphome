#include "esphome/core/defines.h"
#if defined(USE_ARDUINO) && defined(USE_ESP8266)

#include "esphome/components/esp8266/preferences.h"
#include "ota_component.h"
#include "ota_backend.h"

#ifdef USE_OTA_PASSWORD
#include <MD5Builder.h>
#endif

#include <Updater.h>

namespace esphome {
namespace ota {

class ArduinoESP8266OTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override {
    bool ret = Update.begin(image_size, U_FLASH);
    if (ret) {
      esp8266::preferences_prevent_write(true);
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

  void set_update_md5(const char *md5) override { Update.setMD5(md5); }
  OTAResponseTypes write(uint8_t *data, size_t len) override {
    size_t written = Update.write(data, len);
    if (written != len) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_OK;
  }

  OTAResponseTypes end() override {
    if (!Update.end())
      return OTA_RESPONSE_ERROR_UPDATE_END;
    return OTA_RESPONSE_OK;
  }

  void abort() override {
    Update.end();
    esp8266::preferences_prevent_write(false);
  }
};

}  // namespace ota
}  // namespace esphome
#endif
