#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_OTA_STATE_CALLBACK
#include "esphome/core/automation.h"
#endif

namespace esphome {
namespace ota {

enum OTAResponseTypes {
  OTA_RESPONSE_OK = 0x00,
  OTA_RESPONSE_REQUEST_AUTH = 0x01,

  OTA_RESPONSE_HEADER_OK = 0x40,
  OTA_RESPONSE_AUTH_OK = 0x41,
  OTA_RESPONSE_UPDATE_PREPARE_OK = 0x42,
  OTA_RESPONSE_BIN_MD5_OK = 0x43,
  OTA_RESPONSE_RECEIVE_OK = 0x44,
  OTA_RESPONSE_UPDATE_END_OK = 0x45,
  OTA_RESPONSE_SUPPORTS_COMPRESSION = 0x46,
  OTA_RESPONSE_CHUNK_OK = 0x47,

  OTA_RESPONSE_ERROR_MAGIC = 0x80,
  OTA_RESPONSE_ERROR_UPDATE_PREPARE = 0x81,
  OTA_RESPONSE_ERROR_AUTH_INVALID = 0x82,
  OTA_RESPONSE_ERROR_WRITING_FLASH = 0x83,
  OTA_RESPONSE_ERROR_UPDATE_END = 0x84,
  OTA_RESPONSE_ERROR_INVALID_BOOTSTRAPPING = 0x85,
  OTA_RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG = 0x86,
  OTA_RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG = 0x87,
  OTA_RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE = 0x88,
  OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE = 0x89,
  OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION = 0x8A,
  OTA_RESPONSE_ERROR_MD5_MISMATCH = 0x8B,
  OTA_RESPONSE_ERROR_RP2040_NOT_ENOUGH_SPACE = 0x8C,
  OTA_RESPONSE_ERROR_UNKNOWN = 0xFF,
};

enum OTAState {
  OTA_COMPLETED = 0,
  OTA_STARTED,
  OTA_IN_PROGRESS,
  OTA_ABORT,
  OTA_ERROR,
};

class OTABackend {
 public:
  virtual ~OTABackend() = default;
  virtual OTAResponseTypes begin(size_t image_size) = 0;
  virtual void set_update_md5(const char *md5) = 0;
  virtual OTAResponseTypes write(uint8_t *data, size_t len) = 0;
  virtual OTAResponseTypes end() = 0;
  virtual void abort() = 0;
  virtual bool supports_compression() = 0;
};

class OTAComponent : public Component {
#ifdef USE_OTA_STATE_CALLBACK
 public:
  void add_on_state_callback(std::function<void(ota::OTAState, float, uint8_t)> &&callback) {
    this->state_callback_.add(std::move(callback));
  }

 protected:
  CallbackManager<void(ota::OTAState, float, uint8_t)> state_callback_{};
#endif
};

#ifdef USE_OTA_STATE_CALLBACK
class OTAGlobalCallback {
 public:
  void register_ota(OTAComponent *ota_caller) {
    ota_caller->add_on_state_callback([this, ota_caller](OTAState state, float progress, uint8_t error) {
      this->state_callback_.call(state, progress, error, ota_caller);
    });
  }
  void add_on_state_callback(std::function<void(OTAState, float, uint8_t, OTAComponent *)> &&callback) {
    this->state_callback_.add(std::move(callback));
  }

 protected:
  CallbackManager<void(OTAState, float, uint8_t, OTAComponent *)> state_callback_{};
};

OTAGlobalCallback *get_global_ota_callback();
void register_ota_platform(OTAComponent *ota_caller);
#endif
std::unique_ptr<ota::OTABackend> make_ota_backend();

}  // namespace ota
}  // namespace esphome
