#pragma once

#include "esphome/components/ld600x/ld600x.h"
#include "esphome/core/gpio.h"

namespace esphome::ld6002b {

enum LD6002BButtonType : uint8_t {
  BUTTON_WAKE = ld600x::LD600X_MODEL_KIND_BASE,
};

class LD6002BComponent final : public ld600x::LD600XComponent {
 public:
  LD6002BComponent() : LD600XComponent(LOG_STR("ld6002b")) {}

  void setup() override;

  void set_wakeup_pin(GPIOPin *pin) { this->wakeup_pin_ = pin; }
  void set_wakeup_pulse_ms(uint32_t ms) { this->wakeup_pulse_ms_ = ms; }
  void set_auto_wake(bool enable) { this->auto_wake_ = enable; }

#ifdef USE_SWITCH
  SUB_SWITCH(low_power)
  bool set_switch_state(switch_::Switch *entity, bool state) override;
#endif
#ifdef USE_BUTTON
  void press_button(uint8_t kind) override;
#endif

 protected:
  bool handle_model_report(uint16_t type, const uint8_t *data, uint16_t len) override;
  bool model_wants_target_stream() const override;
  void on_presence_update() override { this->update_work_mode_fallback_(); }
  bool defer_frame_write(uint16_t type, const uint8_t *data, uint8_t len, bool track) override;
  void setup_model() override;
  void dump_model_config() override;

  void handle_low_power_report_(const uint8_t *data, uint16_t len);
  void handle_work_mode_report_(const uint8_t *data, uint16_t len);
  void update_work_mode_fallback_();
  void publish_work_mode_(bool low_power);
  void wake_();

  // Keyed so a repeated press replaces its own pending timeout instead of stacking
  // another, and so the command path can cancel it when it takes the pin over.
  static constexpr uint32_t WAKE_BUTTON_TIMEOUT_ID = MODEL_TIMEOUT_ID;

  GPIOPin *wakeup_pin_{nullptr};
  uint32_t wakeup_pulse_ms_{50};
  // Frame a pending wake pulse will write, snapshotted because active_command_ may move on first.
  std::array<uint8_t, ld600x::CMD_MAX_DATA_LEN> wake_scratch_{};
  bool auto_wake_{true};
  bool wake_pulse_pending_{false};
  bool work_mode_reported_{false};
  bool low_power_enabled_{false};
  bool low_power_reported_{false};
  Deduplicator<bool> work_mode_low_power_;
};

}  // namespace esphome::ld6002b
