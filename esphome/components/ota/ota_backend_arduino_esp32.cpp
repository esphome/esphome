#include "esphome/core/defines.h"
#include "ota_component.h"
#include "ota_backend.h"

#ifdef USE_OTA_PASSWORD
#include <MD5Builder.h>
#endif

#include <Update.h>

namespace esphome {
namespace ota {

class ArduinoESP32OTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override {
    bool ret = Update.begin(image_size, U_FLASH);
    if (ret) {
      return OTA_RESPONSE_OK;
    }

    uint8_t error = Update.getError();
    if (error == UPDATE_ERROR_SIZE)
      return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
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
    Update.abort();
  }
};

}  // namespace ota
}  // namespace esphome
