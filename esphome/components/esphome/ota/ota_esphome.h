#pragma once

#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace ota_esphome {

enum OTAESPHomeState { OTA_COMPLETED = 0, OTA_STARTED, OTA_IN_PROGRESS, OTA_ERROR };

/// OTAESPHomeComponent provides a simple way to integrate Over-the-Air updates into your app using ArduinoOTA.
class OTAESPHomeComponent : public Component {
 public:
  OTAESPHomeComponent();
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
  void add_on_state_callback(std::function<void(OTAESPHomeState, float, uint8_t)> &&callback);
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

  bool has_safe_mode_{false};              ///< stores whether safe mode can be enabled
  uint32_t safe_mode_start_time_;          ///< stores when safe mode was enabled
  uint32_t safe_mode_enable_time_{60000};  ///< The time safe mode should be on for
  uint32_t safe_mode_rtc_value_;
  uint8_t safe_mode_num_attempts_;
  ESPPreferenceObject rtc_;

  static const uint32_t ENTER_SAFE_MODE_MAGIC =
      0x5afe5afe;  ///< a magic number to indicate that safe mode should be entered on next boot

#ifdef USE_OTA_STATE_CALLBACK
  CallbackManager<void(OTAESPHomeState, float, uint8_t)> state_callback_{};
#endif
};

extern OTAESPHomeComponent *global_ota_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace ota_esphome
}  // namespace esphome
