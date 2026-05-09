#pragma once
#ifdef USE_ESP32_VARIANT_ESP32P4
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esp_ldo_regulator.h"

namespace esphome::esp_ldo {

class EspLdo : public Component {
 public:
  EspLdo(int channel) : channel_(channel) {}

  void setup() override;
  void dump_config() override;

  void set_adjustable(bool adjustable) { this->adjustable_ = adjustable; }
  void set_voltage(int voltage_mv) { this->voltage_mv_ = voltage_mv; }
  void adjust_voltage(float voltage);
  float get_setup_priority() const override {
    return setup_priority::BUS;  // LDO setup should be done early
  }

 protected:
  int channel_;
  int voltage_mv_{2700};
  bool adjustable_{false};
  esp_ldo_channel_handle_t handle_{};
};

template<typename... Ts> class AdjustAction : public Action<Ts...> {
 public:
  explicit AdjustAction(EspLdo *ldo) : ldo_(ldo) {}

  TEMPLATABLE_VALUE(float, voltage)

  void play(const Ts &...x) override { this->ldo_->adjust_voltage(this->voltage_.value(x...)); }

 protected:
  EspLdo *ldo_;
};

}  // namespace esphome::esp_ldo

#endif  // USE_ESP32_VARIANT_ESP32P4
