#pragma once

#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "ota_backend.h"

namespace esphome {
namespace ota {

enum OTAFeatureTypes {
  OTA_FEATURE_COMPRESSION = 1,
  OTA_FEATURE_WRITING_BOOTLOADER = 2,
  OTA_FEATURE_WRITING_PARTITION_TABLE = 3,
  OTA_FEATURE_WRITING_PARTITIONS = 4,
  OTA_FEATURE_READING = 5,
};

enum OTACommands {
  OTA_COMMAND_WRITE = 1,
  OTA_COMMAND_REBOOT = 2,
  OTA_COMMAND_END = 3,
  OTA_COMMAND_READ = 4,
};

enum OTAState { OTA_COMPLETED = 0, OTA_STARTED, OTA_IN_PROGRESS, OTA_ERROR };

/// Create a singleton with the backend implementation for the selected framework/platform
std::unique_ptr<OTABackend> make_ota_backend();

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

  OTAResponseTypes get_partition_info_(uint8_t *buf, OTAPartitionType &bin_type, size_t &ota_size);
  OTAResponseTypes write_flash_(uint8_t *buf, std::unique_ptr<OTABackend> &backend, const OTAPartitionType &bin_type,
                                size_t ota_size);
  OTAResponseTypes read_flash_(uint8_t *buf, std::unique_ptr<OTABackend> &backend, const OTAPartitionType &bin_type);

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
