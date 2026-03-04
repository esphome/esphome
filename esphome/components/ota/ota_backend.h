#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_OTA_STATE_LISTENER
#include <vector>
#endif

namespace esphome {
namespace ota {

enum OTAResponseTypes {
  OTA_RESPONSE_OK = 0x00,
  OTA_RESPONSE_REQUEST_AUTH = 0x01,
  OTA_RESPONSE_REQUEST_SHA256_AUTH = 0x02,

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

/** Listener interface for OTA state changes.
 *
 * Components can implement this interface to receive OTA state updates
 * without the overhead of std::function callbacks.
 */
class OTAStateListener {
 public:
  virtual ~OTAStateListener() = default;
  virtual void on_ota_state(OTAState state, float progress, uint8_t error) = 0;
};

class OTAComponent : public Component {
#ifdef USE_OTA_STATE_LISTENER
 public:
  void add_state_listener(OTAStateListener *listener) { this->state_listeners_.push_back(listener); }

 protected:
  void notify_state_(OTAState state, float progress, uint8_t error);

  /** Notify state with deferral to main loop (for thread safety).
   *
   * This should be used by OTA implementations that run in separate tasks
   * (like web_server OTA) to ensure listeners execute in the main loop.
   */
  void notify_state_deferred_(OTAState state, float progress, uint8_t error) {
    this->defer([this, state, progress, error]() { this->notify_state_(state, progress, error); });
  }

  std::vector<OTAStateListener *> state_listeners_;
#endif
};

#ifdef USE_OTA_STATE_LISTENER

/** Listener interface for global OTA state changes (includes OTA component pointer).
 *
 * Used by OTAGlobalCallback to aggregate state from multiple OTA components.
 */
class OTAGlobalStateListener {
 public:
  virtual ~OTAGlobalStateListener() = default;
  virtual void on_ota_global_state(OTAState state, float progress, uint8_t error, OTAComponent *component) = 0;
};

/** Global callback that aggregates OTA state from all OTA components.
 *
 * OTA components call notify_ota_state() directly with their pointer,
 * which forwards the event to all registered global listeners.
 */
class OTAGlobalCallback {
 public:
  void add_global_state_listener(OTAGlobalStateListener *listener) { this->global_listeners_.push_back(listener); }

  void notify_ota_state(OTAState state, float progress, uint8_t error, OTAComponent *component) {
    for (auto *listener : this->global_listeners_) {
      listener->on_ota_global_state(state, progress, error, component);
    }
  }

 protected:
  std::vector<OTAGlobalStateListener *> global_listeners_;
};

OTAGlobalCallback *get_global_ota_callback();

// OTA implementations should use:
// - notify_state_() when already in main loop (e.g., esphome OTA)
// - notify_state_deferred_() when in separate task (e.g., web_server OTA)
// This ensures proper listener execution in all contexts.
#endif
std::unique_ptr<ota::OTABackend> make_ota_backend();

}  // namespace ota
}  // namespace esphome
