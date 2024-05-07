#pragma once

#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

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

enum OTAState { OTA_COMPLETED = 0, OTA_STARTED, OTA_IN_PROGRESS, OTA_ERROR };

/// OTAComponent provides a simple way to integrate Over-the-Air updates into your app using ArduinoOTA.
class OTAComponent : public Component {
 public:
  OTAComponent();
#ifdef USE_OTA_PASSWORD
  void set_auth_password(const std::string &password) { password_ = password; }
#endif  // USE_OTA_PASSWORD

  /// Manually set the port OTA should listen on.
  void set_port(uint16_t port);

  bool should_enter_safe_mode(uint8_t num_attempts, uint32_t enable_time);

  /// Set to true if the next startup will enter safe mode
  void set_safe_mode_pending(const bool &pending);
  bool get_safe_mode_pending();

#ifdef USE_OTA_STATE_CALLBACK
  void add_on_state_callback(std::function<void(OTAState, float, uint8_t)> &&callback);
#endif

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  uint16_t get_port() const;

  void clean_rtc();

  void on_safe_shutdown() override;

 protected:
  void write_rtc_(uint32_t val);
  uint32_t read_rtc_();

  void handle_();
  bool readall_(uint8_t *buf, size_t len);
  bool writeall_(const uint8_t *buf, size_t len);

#ifdef USE_OTA_PASSWORD
  std::string password_;
#endif  // USE_OTA_PASSWORD

  uint16_t port_;

  std::unique_ptr<socket::Socket> server_;
  std::unique_ptr<socket::Socket> client_;

  bool has_safe_mode_{false};              ///< stores whether safe mode can be enabled.
  uint32_t safe_mode_start_time_;          ///< stores when safe mode was enabled.
  uint32_t safe_mode_enable_time_{60000};  ///< The time safe mode should be on for.
  uint32_t safe_mode_rtc_value_;
  uint8_t safe_mode_num_attempts_;
  ESPPreferenceObject rtc_;

  static const uint32_t ENTER_SAFE_MODE_MAGIC =
      0x5afe5afe;  ///< a magic number to indicate that safe mode should be entered on next boot

#ifdef USE_OTA_STATE_CALLBACK
  CallbackManager<void(OTAState, float, uint8_t)> state_callback_{};
#endif
};

extern OTAComponent *global_ota_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace ota
}  // namespace esphome
