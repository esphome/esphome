#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace safe_mode {

/// SafeModeComponent provides a safe way to recover from repeated boot failures
class SafeModeComponent : public Component {
 public:
  bool should_enter_safe_mode(uint8_t num_attempts, uint32_t enable_time);

  /// Set to true if the next startup will enter safe mode
  void set_safe_mode_pending(const bool &pending);
  bool get_safe_mode_pending();

  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  void clean_rtc();

  void on_safe_shutdown() override;

  void add_on_safe_mode_callback(std::function<void()> &&callback) {
    this->safe_mode_callback_.add(std::move(callback));
  }

 protected:
  void write_rtc_(uint32_t val);
  uint32_t read_rtc_();

  bool boot_successful_{false};            ///< set to true after boot is considered successful
  uint32_t safe_mode_start_time_;          ///< stores when safe mode was enabled
  uint32_t safe_mode_enable_time_{60000};  ///< The time safe mode should remain active for
  uint32_t safe_mode_rtc_value_;
  uint8_t safe_mode_num_attempts_;
  ESPPreferenceObject rtc_;
  CallbackManager<void()> safe_mode_callback_{};

  static const uint32_t ENTER_SAFE_MODE_MAGIC =
      0x5afe5afe;  ///< a magic number to indicate that safe mode should be entered on next boot
};

}  // namespace safe_mode
}  // namespace esphome
